/***************************************************************************
 *   server.cpp  --  This file is part of diskarbitratord.                 *
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

#include <libgen.h>
#include <sys/syslimits.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "server.hpp"

bool exitFlag = false;

// Block waiting for SIGINT/SIGTERM signals to be sent to the process, then terminate the server
void shutdownServer(std::shared_ptr<grpc::Server> server) {
  // Block SIGINT and SIGTERM from executing the default disposition (terminate)
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  // Wait until one of the signals is pending (generated but not delivered)
  int sig;
  if(sigwait(&sigset, &sig)) {
    LOG(ERROR) << "Error calling sigwait: " << strerror(errno);
  } else {
    exitFlag = true;
  }

  // Kill the server
  LOG(INFO) << "Stopping server...";
  server->Shutdown();
}

int mkpath(const std::string& dir, mode_t mode) {
  if(dir == "/") {
    return 0;
  }

  int res = mkpath(std::string(dirname(const_cast<char*>(dir.c_str()))), mode);
  if(res) {
    // Propagate the error and abort
    return 1;
  }

  res = mkdir(dir.c_str(), mode);
  if(res && errno != EEXIST) {
    return res;
  }
  return 0;
}


// Open the UNIX domain socket for communicating with clients
int openSocket(const std::string& socketPath) {
  // This whole mess is to avoid alloc/free
  std::string dirpath = std::string(dirname(const_cast<char*>(socketPath.c_str())));
  if(mkpath(dirpath.c_str(), 0755)) {
    LOG(ERROR) << "Unable to create socket directory " << dirpath;
    return -1;
  }

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sockfd == -1) {
    LOG(ERROR) << "Unable to open socket: " << strerror(errno);
  }

  return sockfd;
}

void RunServer(const std::string& socketPath) {
  // Service implementation, this has all the handlers for the gRPC calls
  DiskAbitratorServiceImpl service = DiskAbitratorServiceImpl();

  // Before we start the server, we can start processing DiskArbitration
  // framework callbacks for the disks currently in the system.
  if(!service.StartArbitration()) {
    LOG(ERROR) << "Unable to start arbitration session" << std::endl;
    return;
  }
  LOG(INFO) << "Arbitration session started" << std::endl;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;

  int sock = openSocket(socketPath);
  if(sock == -1) {
    return;
  }

  // Binds the server to the socket
  builder.AddListeningPort("unix://" + socketPath, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  // ACTUALLY starts the server
  std::shared_ptr<grpc::Server> server(builder.BuildAndStart());
  if(server == nullptr) {
    LOG(ERROR) << "Unable to start server" << std::endl;
    return;
  }

  // Start the thread that is responsible for stopping the server
  std::thread serverShutdownThread(&shutdownServer, server);
  LOG(INFO) << "Server listening on " << socketPath;

  // Block until shutdown
  server->Wait();
  
  // Determine if server exited abnormally and signal termination thread 
  // if necessary
  if(!exitFlag) {
    LOG(WARNING) << "Server shutdown unexpectedly. Signaling server shutdown thread to terminate";
  } else {
    LOG(INFO) << "Server shutdown";
  }

  if(close(sock) != 0){
    LOG(WARNING) << "Unable to close socket: " << strerror(errno) << std::endl;
  } else {
    LOG(INFO) << "Socket closed" << std::endl;
  }

  serverShutdownThread.join();
}

// Registers the callbacks with DiskArbitration framework to get notified of
// disk events. Does *not* start intercepting mounts
bool DiskAbitratorServiceImpl::StartArbitration() {
  this->session = DASessionCreate(kCFAllocatorDefault);
	if(this->session == NULL) {
    return false;
	}
  
	std::thread loopThread = std::thread(&CFLoop, this, this->session);
  loopThread.detach();

  // We don't want network devices
	CFMutableDictionaryRef filter = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(filter, kDADiskDescriptionVolumeNetworkKey, kCFBooleanFalse);

	DARegisterDiskAppearedCallback(this->session, filter, &DiskAppearedCallback, this);
	DARegisterDiskDisappearedCallback(this->session, filter, &DiskDisappearedCallback, this);
	DARegisterDiskDescriptionChangedCallback(this->session, filter, NULL, &DiskDescriptionChangedCallback, this);

	CFRelease(filter);

  return true;
}

// Unregisters the callbacks
bool DiskAbitratorServiceImpl::StopArbitration() {
  DAUnregisterCallback(this->session, reinterpret_cast<void*>(&DiskAppearedCallback), this);
  DAUnregisterCallback(this->session, reinterpret_cast<void*>(&DiskDisappearedCallback), this);
  DAUnregisterCallback(this->session, reinterpret_cast<void*>(&DiskDescriptionChangedCallback), this);
  CFRunLoopStop(this->runLoop);
  return true;
}

void DiskAbitratorServiceImpl::startIntercept() {
  this->approvalSession = DAApprovalSessionCreate(kCFAllocatorDefault);
	if(!this->approvalSession) {
		throw std::runtime_error("Unable to create approval session");
	}
	
	DAApprovalSessionScheduleWithRunLoop(approvalSession, this->runLoop, kCFRunLoopCommonModes);
	DARegisterDiskMountApprovalCallback(approvalSession, NULL, DiskMountApprovalCallback, reinterpret_cast<void*>(this));
  LOG(INFO) << "Started Disk Interception";
}

void DiskAbitratorServiceImpl::stopIntercept() {
  DAUnregisterApprovalCallback(this->approvalSession, reinterpret_cast<void*>(DiskMountApprovalCallback), reinterpret_cast<void*>(this));
  DAApprovalSessionUnscheduleFromRunLoop(this->approvalSession, this->runLoop, kCFRunLoopCommonModes);
  CFRelease(this->approvalSession);
  this->approvalSession = NULL;
  LOG(INFO) << "Stopped Disk Interception";
}

void DiskAbitratorServiceImpl::addDisk(std::shared_ptr<diskarbitrator::Disk>& disk) {
  if(this->disks.find(disk->disk()) != this->disks.end()) {
    LOG(WARNING) << "Attempted to add a disk with key: " << disk << " which already exists";
    return;
  }
  this->disks[disk->disk()] = disk;
}

void DiskAbitratorServiceImpl::removeDisk(const std::string& disk) {
  if(this->disks.find(disk) == this->disks.end()) {
    LOG(WARNING) << "Attempted to delete a disk with key: " << disk << " which does not exist";
    return;
  }
  this->disks.erase(disk);
}

bool DiskAbitratorServiceImpl::diskExists(const std::string& disk) {
  return this->disks.find(disk) != this->disks.end();
}

void DiskAbitratorServiceImpl::addChildToParent(const std::string& disk, const std::string& parentDisk) {
  if(this->disks.find(parentDisk) == this->disks.end()) {
    throw std::runtime_error("Attempted to add child disk " + disk + " to parent " + parentDisk + ", but the parent disk does not exist");
  }
  std::shared_ptr<diskarbitrator::Disk> parent = this->disks[parentDisk];
  for(const auto& child : parent->children()) {
    if(child == disk) {
      // Already present
      return;
    }
  }
  std::string* newChild = parent->add_children();
  *newChild = disk;
}

void DiskAbitratorServiceImpl::removeChildFromParent(const std::string& disk, const std::string& parentDisk) {
  if(this->disks.find(parentDisk) == this->disks.end()) {
    throw std::runtime_error("Attempted to remove child disk " + disk + " to parent " + parentDisk + ", but the parent disk does not exist");
  }
  std::shared_ptr<diskarbitrator::Disk> parent = this->disks[parentDisk];

  size_t pos = 0;
  bool found = false;
  for (const auto& slice : parent->children()) {
    if(slice == disk) {
      found = true;
      break;
    }
    ++pos;
  }
  if(found) {
    parent->mutable_children()->erase(parent->mutable_children()->begin() + pos);
  }
}

const std::string DiskAbitratorServiceImpl::getParentDisk(const std::string& disk) {
  if(this->disks.find(disk) == this->disks.end()) {
    throw std::runtime_error("Attempted to fetch parent disk from disk " + disk + ", but it does not exist");
  }
  std::shared_ptr<diskarbitrator::Disk> slice = this->disks[disk];
  return slice->parent_disk();
}

void DiskAbitratorServiceImpl::updateDiskDescription(const std::string& disk, const diskarbitrator::DiskDescription& description) {
  if(this->disks.find(disk) == this->disks.end()) {
    throw std::runtime_error("Attempted to change disk description from disk " + disk + ", but it does not exist");
  }
  std::shared_ptr<diskarbitrator::Disk> d = this->disks[disk];
  *(d->mutable_description()) = description;
}
