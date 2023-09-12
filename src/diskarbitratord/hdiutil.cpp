/***************************************************************************
 *   hdiutil.cpp  --  This file is part of diskarbitratord.                *
 *                                                                         *
 *   Copyright (C) 2023 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   diskarbitratord is free software: you can redistribute it and/or      *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   diskarbitratord is distributed in the hope that it will be useful,    *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/

#include <mutex>
#include <thread>

#include <spawn.h>

#include <glog/logging.h>

#include "hdiutil.hpp"
#include "plist.hpp"
#include "scope_guard.hpp"


#define HDIUTIL_PATH "/usr/bin/hdiutil"
// Strategically, this is the same size as the pipe buffer in XNU kernel
#define READ_BUFFER_SIZE 16384 

typedef struct CommandOutput {
  int retCode;
  std::string stdout;
  std::string stderr;
} CommandOutput;

// Let's talk about hdiutil for a minute.
// Despite my absolute hatred for using CLI tools from code, having to deal
// with unparseable, ever-changing stdout from calls, etc. I have no choice but
// to call hdiutil(1) here. The reasons for this are:
// - hdiutil uses the DiskImages *private* framework. Unfortunately this is a
//   private framework even on macOS. Assuming I somehow got/reverse engineered
//   a header, it could change at Apple's whim between versions. Also makes
//   life complicated for the poor soul trying to compile this application.
// - hdiutil has the 'com.apple.private.diskimages.kext.user-client-access'
//   entitlement. I would wager that most likely, even if I somehow was able to
//   easily use and link against the framework, I'd need that entitlement to do
//   anything remotely interesting at all.
//
// Hence, with a heavy heart, here's a series of functions that wrap hdiutil 
// for attaching disks

// Used to send stdin data to child
void streamDataIn(int fd, const std::string* in, std::mutex* mutex, bool* err) {
  const std::lock_guard<std::mutex> lock(*mutex);
  size_t totalBytesWritten = 0;
  ssize_t bytesWritten = 0;
  while(totalBytesWritten != in->size()) {
    bytesWritten = write(fd, in->c_str() + totalBytesWritten, in->size() - totalBytesWritten);
    if(bytesWritten < 0) {
      if(errno != EINTR) {
        *err = true;
        break;
      }
    }
    totalBytesWritten += bytesWritten;
  }
  close(fd);
}

// Used to read stdout/err data from child
void streamDataOut(int fd, std::string* out, std::mutex* mutex, bool* err) {
  const std::lock_guard<std::mutex> lock(*mutex);
  ssize_t bytesRead = 0;
  char buffer[READ_BUFFER_SIZE];

  while((bytesRead = read(fd, buffer, READ_BUFFER_SIZE)) != 0) {
    if(bytesRead < 0) {
      if(errno != EINTR) {
        *err = true;
        break;
      }
    }
    *out += std::string(buffer, READ_BUFFER_SIZE);
  }
  close(fd);
}

// Just the typical fork/exec wrap. Since vfork is deprecated in macOS, we're
// using posix_spawn, since it also avoids duplicating process memory.
CommandOutput runHdiutil(const std::string& command, const std::string& image, std::vector<std::string> extraArgs, const std::string& stdinData) {
  CommandOutput co;

  pid_t childPid;
  
  int stdoutPipeFds[2];
  int stderrPipeFds[2];
  int stdinPipeFds[2];
  if(pipe(stdoutPipeFds) != 0) {
    throw std::runtime_error("Unable to open stdout pipe: " + std::string(strerror(errno)));
  }
  if(pipe(stderrPipeFds) != 0) {
    throw std::runtime_error("Unable to open stderr pipe: " + std::string(strerror(errno)));
  }
  if(pipe(stdinPipeFds) != 0) {
    throw std::runtime_error("Unable to open stdin pipe: " + std::string(strerror(errno)));
  }

  ScopeGuard pipeGuard([stdinPipeFds, stdoutPipeFds, stderrPipeFds]() {
    close(stdinPipeFds[0]);
    close(stdinPipeFds[1]);

    close(stdoutPipeFds[0]);
    close(stdoutPipeFds[1]);

    close(stderrPipeFds[0]);
    close(stderrPipeFds[1]);
  });

  posix_spawn_file_actions_t fileActions;
  if(posix_spawn_file_actions_init(&fileActions) != 0) {
    throw std::runtime_error("Unable to init spawn file actions:" + std::string(strerror(errno)));
  }
  // Child process will write/read std streams from the pipes
  if(posix_spawn_file_actions_adddup2(&fileActions, stdinPipeFds[0], STDIN_FILENO) != 0) {
    throw std::runtime_error("Unable to dup stdin fd:" + std::string(strerror(errno)));
  }
  if(posix_spawn_file_actions_adddup2(&fileActions, stdoutPipeFds[1], STDOUT_FILENO) != 0) {
    throw std::runtime_error("Unable to dup stdout fd:" + std::string(strerror(errno)));
  }
  if(posix_spawn_file_actions_adddup2(&fileActions, stderrPipeFds[1], STDERR_FILENO) != 0) {
    throw std::runtime_error("Unable to dup stderr fd:" + std::string(strerror(errno)));
  }

  std::string stdoutData;
  std::string stderrData;

  bool stdinErr = false;
  bool stdoutErr = false;
  bool stderrErr = false;

  std::mutex stdinMutex;
  std::mutex stdoutMutex;
  std::mutex stderrMutex;

  std::thread stdinThread(&streamDataIn, stdinPipeFds[1], &stdinData, &stdinMutex, &stdinErr);
  std::thread stdoutThread(&streamDataOut, stdoutPipeFds[0], &stdoutData, &stdoutMutex, &stdoutErr);
  std::thread stderrThread(&streamDataOut, stderrPipeFds[0], &stderrData, &stderrMutex, &stderrErr);

  // Running these in joinable state is not a good idea. We'd need to use a
  // scope guard to ensure they are joined before the thread is destroyed, and
  // unfortunately the order of destructor calls is the reverse of declaration.
  // So this guard would run before the pipe file descriptors are closed, which
  // would lead to the threads never joining in some sceanarios. Given that 
  // these threads are *guaranteed* to finish if the file descriptors are 
  // closed, I'm comfortable enough running them in detached state.
  stdinThread.detach();
  stdoutThread.detach();
  stderrThread.detach();

  std::vector<const char*> argv;
  // From posix_spawn(2), argv[0] must be the path to the executable (it's not
  // added automatically)
  argv.push_back(HDIUTIL_PATH);

  // Add the command
  argv.push_back(command.c_str());

  // Add extra args
  for(const auto& arg : extraArgs) {
    argv.push_back(arg.c_str());
  }

  // Add the image path, if specified
  if(image.size()) {
    argv.push_back(image.c_str());
  }

  // Push final NULL string
  argv.push_back(NULL);

  if(posix_spawn(&childPid, HDIUTIL_PATH, &fileActions, NULL, (char* const*) argv.data(), NULL) != 0) {
    throw std::runtime_error("Unable to spawn child hdiutil process: " + std::string(strerror(errno)));
  }
  
  // get exit code
  int status;
  while(waitpid(childPid, &status, 0) == -1) {
    if(errno != EINTR) {
      throw std::runtime_error("Error waiting for child process PID: " + std::string(strerror(errno)));
    }
  }
  if (WIFEXITED(status)) {
    co.retCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    // Child exited due to signal. Return which one was it.
    co.retCode = WTERMSIG(status);  
  }

  // Stronger guarantee that the threads have exhausted all input/output

  close(stdinPipeFds[0]);
  close(stdinPipeFds[1]);

  close(stdoutPipeFds[0]);
  close(stdoutPipeFds[1]);

  close(stderrPipeFds[0]);
  close(stderrPipeFds[1]);

  const std::lock_guard<std::mutex> stdinLock(stdinMutex);
  const std::lock_guard<std::mutex> stdoutLock(stdoutMutex);
  const std::lock_guard<std::mutex> stderrLock(stderrMutex);

  posix_spawn_file_actions_destroy(&fileActions);

  if(stdinErr || stdoutErr || stderrErr) {
    throw std::runtime_error("Error streaming data in/out child process");
  }

  co.stdout = stdoutData;
  co.stderr = stderrData;

  return co;
}

// Returns true if the image requires a passphrase.
bool isImageEncrypted(const std::string& path) {
  CommandOutput output = runHdiutil("isencrypted", path, {"-plist"}, "");
  if(output.retCode) {
    throw std::runtime_error("hdiutil returned: " + std::to_string(output.retCode) + ". Error: " + output.stderr);
  }
	
  std::unique_ptr<Plist> pl = std::unique_ptr<Plist>(new Plist(output.stdout));
  bool value = (*pl)["encrypted"].get<bool>();
  return value;
}

// Returns true if the image has a Software License Agreement attached.
bool imageHasSLA(const std::string& path, const std::string& password) {
  CommandOutput output = runHdiutil("imageinfo", path, {"-plist"}, password);
  if(output.retCode) {
    throw std::runtime_error("hdiutil returned: " + std::to_string(output.retCode) + ". Error: " + output.stderr);
  }
	
  std::unique_ptr<Plist> pl = std::unique_ptr<Plist>(new Plist(output.stdout));
  bool value = (*pl)["Properties"]["Software License Agreement"].get<bool>();
  return value;
}

std::vector<std::string> attachDisk(const std::string& path, diskarbitrator::MountMode mode, const std::string& password) {
  std::vector<std::string> args;
  std::string stdinData;

  args.push_back("-plist");
  args.push_back("-noverify");

  if(isImageEncrypted(path) && password == "") {
    throw std::runtime_error("Image is encrypted and a password was not provided");
  }

  if(password != "") {
    stdinData = password + "\n";
    args.push_back("-stdinpass");
  }
	
  if(imageHasSLA(path, password)) {
    // hdiutil prompts the user with a (Y/n) dialog if the image has a SLA
    stdinData += "Y\n";
  }
	
  if(mode == diskarbitrator::MountMode::MOUNT_NONE) {
    args.push_back("-nomount");
  } else if(diskarbitrator::MountMode::MOUNT_RDONLY) {
    args.push_back("-readonly");
  }

  // TODO: Implement execution timeout in case it gets stuck
  CommandOutput output = runHdiutil("attach", path, args, stdinData);
  if(output.retCode) {
    throw std::runtime_error("hdiutil returned: " + std::to_string(output.retCode) + ". Error: " + output.stderr);
  }

  std::vector<std::string> disks;
	std::unique_ptr<Plist> pl = std::unique_ptr<Plist>(new Plist(output.stdout));
  
  for(unsigned int i = 0; i < (*pl)["system-entities"].size(); ++i) {
    const std::string devEntry = (*pl)["system-entities"][i]["dev-entry"].get<std::string>();
    disks.push_back(devEntry);
  }

  return std::move(disks);
}