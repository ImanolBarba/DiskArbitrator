/***************************************************************************
 *   cftypes.hpp  --  This file is part of diskarbitratord.                *
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

#ifndef CFTYPES_HPP_
#define CFTYPES_HPP_

#include <string>

#include <CoreFoundation/CoreFoundation.h>

std::string CFStrToStr(CFStringRef cfstr);
std::string CFUUIDToStr(CFUUIDRef cfuuid);
std::string CFURLToStr(CFURLRef cfurl);
std::string CFDataToStr(CFDataRef cfdata);
bool CFBoolToBool(CFBooleanRef cfbool);
int64_t CFNumberToInt(CFNumberRef cfnum);
float CFNumberToFloat(CFNumberRef cfnum);
double CFNumberToDouble(CFNumberRef cfnum);

// It appears the "Reference Time" in macOS frameworks is 00:00:00 UTC Jan 1st
// 2001. This function converts the number of seconds from that time reference,
// to the epoch reference (00:00:00 UTC Jan 1st 1970).
uint64_t CFTimeIntervalToEpoch(double timeRef);
std::string formatStringAsGUID(const std::string& input);

CFTypeRef getKey(CFDictionaryRef dict, const std::string key);

#endif