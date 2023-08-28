/***************************************************************************
 *   common.cpp  --  This file is part of diskarbitratorctl.               *
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


#include <iomanip>
#include <sstream>

#include "common.hpp"

bool validateMountMode(const std::string& mode) {
  return (mode == "ro" || mode == "rw" || mode == "nomount");
}

bool validateArbitrationMode(const std::string& mode) {
  return (mode == "block" || mode == "ro" || mode == "off");
}

std::string sizeToHuman(uint64_t size) {
  if(size >= 1e3 && size < 1e6) {
    return std::move(std::to_string(size / static_cast<uint64_t>(1e3)) + "K");
  }
	else if(size >= 1e6 && size < 1e9) {
    return std::move(std::to_string(size / static_cast<uint64_t>(1e6)) + "M");
  }
	else if(size >= 1e9 && size < 1e12) {
    return std::move(std::to_string(size / static_cast<uint64_t>(1e9)) + "G");
  }
	else if(size >= 1e12) { // Anything bigger we represent in TB
    return std::move(std::to_string(size / static_cast<uint64_t>(1e12)) + "T");
  }

  return std::move(std::to_string(size));
}

std::string unixTimeToString(uint64_t ts) {
  std::time_t time = ts;
  std::tm* t = std::gmtime(&time);
  std::stringstream ss; // or if you're going to print, just input directly into the output stream
  ss << std::put_time(t, "%F %T %z");
  return std::move(ss.str());
}

std::wstring strToWstr(const std::string& str) {
  return std::move(std::wstring(str.begin(), str.end()));
}