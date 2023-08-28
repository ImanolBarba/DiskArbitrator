/***************************************************************************
 *   common.hpp  --  This file is part of diskarbitratorctl.               *
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


#ifndef COMMON_HPP_
#define COMMON_HPP_

#include <string>

bool validateMountMode(const std::string& mode);
bool validateArbitrationMode(const std::string& mode);
std::string sizeToHuman(uint64_t size);
std::string unixTimeToString(uint64_t ts);
std::wstring strToWstr(const std::string& str);

#endif