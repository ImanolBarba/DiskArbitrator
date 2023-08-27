/***************************************************************************
 *   socket.cpp  --  This file is part of diskarbitratorctl.               *
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

#include <sys/stat.h>
#include <unistd.h>

#include "socket.hpp"

bool validateSocketPath(const std::string& socketPath) {
  struct stat statbuf;
  if(stat(socketPath.c_str(), &statbuf) != 0) {
    std::cerr << "Invalid socket path: " << strerror(errno) << std::endl;
    return false;
  }
  if(!S_ISSOCK(statbuf.st_mode)) {
    std::cerr << "Invalid socket path: not a socket" << std::endl;
    return false;
  }
  if(access(socketPath.c_str(), R_OK | W_OK) != 0) {
    std::cerr << "Invalid socket path: " << strerror(errno) << std::endl;
    return false;
  }
  return true;
}

DiskArbitratorClient getClient(const std::string& socketPath) {
  DiskArbitratorClient client(
    grpc::CreateChannel(std::string("unix://") + socketPath, grpc::InsecureChannelCredentials())
  );
  return std::move(client);
}