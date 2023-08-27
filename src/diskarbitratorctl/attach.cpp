/***************************************************************************
 *   attach.cpp  --  This file is part of diskarbitratorctl.               *
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

bool doAttach(int argc, char** argv) {
  cxxopts::Options options("diskarbitratorctl attach", "attach: Attaches a disk image (and optionally mounts it) to the system");
  options.add_options()
      ("image", "Image to mount", cxxopts::value<std::string>())
      ("m,mode", "Mode to mount the disk. Either nomount, ro or rw.", cxxopts::value<std::string>()->default_value("nomount"))
      ("s,socket", "diskarbitratord socket path", cxxopts::value<std::string>()->default_value(DEFAULT_SOCKET_PATH))
      ("h,help", "Print usage")
  ;
  
  std::string socketPath;
  std::string image;
  std::string modeStr;

  try {
    options.parse_positional({"image"});
    options.positional_help("image");
    cxxopts::ParseResult result = options.parse(argc, argv);
    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return true;
    }

    if(!result.count("image")) {
      std::cout << "image argument was not provided" << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }

    std::vector<std::string> unmatched = result.unmatched();
    if(unmatched.size()) {
      std::cout << "Unrecognized argument: " << unmatched[0] << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }

    socketPath = result["socket"].as<std::string>();
    image = result["image"].as<std::string>();
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

  diskarbitrator::MountMode mode = diskarbitrator::MountMode::MOUNT_NONE;
  if(modeStr == "ro") {
    mode = diskarbitrator::MountMode::MOUNT_RDONLY;
  } else if(modeStr == "rw") {
    mode = diskarbitrator::MountMode::MOUNT_RDWR;
  }

  DiskArbitratorClient client = getClient(socketPath);
  std::vector<std::string> disks = client.AttachDisk(image, mode);
  if(!disks.size()) {
    return false;
  }
  std::cout << "Disks attached:" << std::endl;
  for(const auto& d : disks) {
    std::cout << d << std::endl;
  }

  return true;
}