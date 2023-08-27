/***************************************************************************
 *   hdiutil.hpp  --  This file is part of diskarbitratord.                *
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

#ifndef HDIUTIL_HPP_
#define HDIUTIL_HPP_

#include "diskarbitrator.grpc.pb.h"

// Attaches a disk image, returns the BSD disk names from the attach operation.
std::vector<std::string> attachDisk(const std::string& path, diskarbitrator::MountMode mode, const std::string& password = "");

#endif