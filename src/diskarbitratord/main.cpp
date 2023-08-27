/***************************************************************************
 *   main.cpp  --  This file is part of diskarbitratord.                   *
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

#include <iostream>
#include <map>
#include <string>
#include <thread>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <semaphore.h>

#include <glog/logging.h>

#include <cxxopts.hpp>

#include "server.hpp"

#define DEFAULT_SOCKET_PATH "/private/var/diskarbitratord/socket"

int main(int argc, char** argv) {
  // Init logging
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_stderrthreshold = 0;

  // Parse CLI flags
  cxxopts::Options options("diskarbitratord", "Disk Arbitrator daemon");
  options.add_options() 
      ("s,socket", "diskarbitratord service socket path", cxxopts::value<std::string>()->default_value(DEFAULT_SOCKET_PATH))
      ("h,help", "Print usage")
  ;
  cxxopts::ParseResult result = options.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }
  std::string socketPath = result["socket"].as<std::string>();

  // Main server method. Returns when it's shut down.
  RunServer(socketPath);

  LOG(INFO) << "Exiting...";
  return 0;
}