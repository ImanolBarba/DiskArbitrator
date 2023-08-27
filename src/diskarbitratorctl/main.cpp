/***************************************************************************
 *   main.cpp  --  This file is part of diskarbitratorctl.                 *
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

#include "commands.hpp"

static void printHelp() {
  std::cout << "diskarbitratorctl: diskarbitratord CLI client" << std::endl;
  std::cout << "Usage:" << std::endl;
  std::cout << "  diskarbitratorctl COMMAND" << std::endl;
  std::cout << std::endl;
  std::cout << "  COMMAND           See below for available commands." << std::endl;
  std::cout << "                    run COMMAND --help to get command-specific usage instructions." << std::endl;
  std::cout << "  -h, --help        Print usage" << std::endl;
  std::cout << std::endl;
  std::cout << "Available commands:" << std::endl;
  std::cout << "  arbitrate  Changes disk arbitration mode" << std::endl;
  std::cout << "  list       Lists available disks in the system" << std::endl;
  std::cout << "  info       Shows information about a specific disk" << std::endl;
  std::cout << "  mount      Mounts the specified disk" << std::endl;
  std::cout << "  umount     Unmounts the specified disk" << std::endl;
  std::cout << "  attach     Attaches a disk image (and optionally mounts it) to the system" << std::endl;
  std::cout << "  eject      Ejects a disk from the system" << std::endl;
  std::cout << std::endl;  
}

int main(int argc, char** argv) {
  if(argc < 2) {
    std::cerr << "Not enough arguments" << std::endl;
    printHelp();
    return 1;
  }

  std::string command = parseCommand(argv[1]);
 
  if(!command.size()) {
    std::cerr << "Invalid command: " << argv[1] << std::endl;
    printHelp();
    return 1;
  }

  if(command == "mount") {
    if(!doMount(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "umount") {
    if(!doUmount(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "eject") {
    if(!doEject(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "attach") {
    if(!doAttach(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "info") {
    if(!doInfo(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "list") {
    if(!doList(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "arbitrate") {
    if(!doArbitrate(argc - 1, argv + 1)) {
      return 1;
    }
  } else if(command == "-h" || command == "--help") {
    printHelp();
  }

  return 0;
}