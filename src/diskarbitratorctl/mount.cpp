/***************************************************************************
 *   mount.cpp  --  This file is part of diskarbitratorctl.                *
 *                                                                         *
 *   Copyright (C) 2023 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   diskarbitratorctl is free software: you can redistribute it and/or    *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   diskarbitratorctl is distributed in the hope that it will be useful,  *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/


#include <iostream>

#include <cxxopts.hpp>

#include "diskarbitrator.grpc.pb.h"

#include "common.hpp"
#include "client.hpp"
#include "socket.hpp"


bool doMount(int argc, char** argv) {
  cxxopts::Options options("diskarbitratorctl mount", "mount: Mounts the specified disk");
  options.add_options()
    ("disk", "Disk to mount", cxxopts::value<std::string>())
    ("path", "path where to mount the disk. If not provided, will mount the disk under /Volumes", cxxopts::value<std::string>())
    ("m,mode", "Mode to mount the disk. Either 'ro' or 'rw'.", cxxopts::value<std::string>()->default_value("ro"))
    ("o,opts", "Options to pass to the underlaying mount call", cxxopts::value<std::vector<std::string>>())
    ("s,socket", "diskarbitratord socket path", cxxopts::value<std::string>()->default_value(DEFAULT_SOCKET_PATH))
    ("h,help", "Print usage")
  ;
  
  std::string disk;
  std::string modeStr;
  std::string path;
  std::vector<std::string> args;
  std::string socketPath;

  try {
    options.parse_positional({"disk", "path"});
    options.positional_help("disk [path (defaults to /Volumes/${VOLUME_NAME} if unspecified)]");
    cxxopts::ParseResult result = options.parse(argc, argv);
    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return true;
    }

    if(!result.count("disk")) {
      std::cout << "disk argument was not provided" << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }

    if(result.count("path")) {
      path = result["path"].as<std::string>();
    }

    if(result.count("opts")) {
      args = result["opts"].as<std::vector<std::string>>();
    }

    std::vector<std::string> unmatched = result.unmatched();
    if(unmatched.size()) {
      std::cout << "Unrecognized argument: " << unmatched[0] << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }
  
    socketPath = result["socket"].as<std::string>();
    disk = result["disk"].as<std::string>();
    modeStr = result["mode"].as<std::string>();
  } catch(const cxxopts::exceptions::exception& ex) {
    std::cout << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return false;
  }

  if(!validateMountMode(modeStr)) {
    std::cout << "Specified mode " << modeStr << " is not valid" << std::endl;
    std::cout << options.help() << std::endl;
    return false;
  }

  diskarbitrator::MountMode mode = diskarbitrator::MountMode::MOUNT_RDONLY;
  if(modeStr == "rw") {
    mode = diskarbitrator::MountMode::MOUNT_RDWR;
  }
  
  DiskArbitratorClient client = getClient(socketPath);
  std::string mountpoint = client.MountDisk(disk, mode, args, path);
  if(!mountpoint.size()) {
    return false;
  }
  std::cout << "Mounted " << disk << " in " << mountpoint << std::endl;
  return true;
}