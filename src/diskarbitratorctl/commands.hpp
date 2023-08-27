/***************************************************************************
 *   commands.hpp  --  This file is part of diskarbitratorctl.             *
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


#ifndef COMMANDS_HPP_
#define COMMANDS_HPP_

bool doMount(int argc, char** argv);
bool doUmount(int argc, char** argv);
bool doEject(int argc, char** argv);
bool doAttach(int argc, char** argv);
bool doInfo(int argc, char** argv);
bool doList(int argc, char** argv);
bool doArbitrate(int argc, char** argv);

const std::string parseCommand(const char* arg) {
  const std::vector<const std::string> validCommands = {
    "eject",
    "mount",
    "umount",
    "attach",
    "arbitrate",
    "list",
    "info",
  };

  for(const auto& cmd : validCommands) {
    std::string argStr = std::string(arg);
    if(cmd == argStr) {
      return std::move(cmd);
    }
  }

  return std::move("");
}

#endif