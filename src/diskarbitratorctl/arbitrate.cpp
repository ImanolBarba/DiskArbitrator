/***************************************************************************
 *   arbitrate.cpp  --  This file is part of diskarbitratorctl.            *
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

bool doArbitrate(int argc, char** argv) {
  cxxopts::Options options("diskarbitratorctl arbitrate", "arbitrate: Changes disk arbitration mode");
  options.add_options()
      ("mode", "Mode to mount the disk. Either off, readonly or block.", cxxopts::value<std::string>()->default_value("block"))
      ("s,socket", "diskarbitratord socket path", cxxopts::value<std::string>()->default_value(DEFAULT_SOCKET_PATH))
      ("h,help", "Print usage")
  ;
  
  std::string socketPath;
  std::string disk;
  std::string modeStr;

  try {
    options.parse_positional({"mode"});
    options.positional_help("mode (off|ro|block). Default is block.");
    cxxopts::ParseResult result = options.parse(argc, argv);
    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return true;
    }

    std::vector<std::string> unmatched = result.unmatched();
    if(unmatched.size()) {
      std::cout << "Unrecognized argument: " << unmatched[0] << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }

    socketPath = result["socket"].as<std::string>();
    modeStr = result["mode"].as<std::string>();
  } catch(const cxxopts::exceptions::exception& ex) {
    std::cout << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return false;
  }

  if(!validateArbitrationMode(modeStr)) {
    std::cout << "Specified mode " << modeStr << " is not valid" << std::endl;
    std::cout << options.help() << std::endl;
    return false;
  }

  diskarbitrator::ArbitrationMode mode = diskarbitrator::ArbitrationMode::ARBITRATOR_BLOCK;
  if(modeStr == "ro") {
    mode = diskarbitrator::ArbitrationMode::ARBITRATOR_RDONLY;
  } else if(modeStr == "off") {
    mode = diskarbitrator::ArbitrationMode::ARBITRATOR_NONE;
  }
  
  DiskArbitratorClient client = getClient(socketPath);
  if(!client.Arbitrate(mode)) {
    return false;
  }

  return true;
}