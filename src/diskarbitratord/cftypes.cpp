/***************************************************************************
 *   cftypes.cpp  --  This file is part of diskarbitratord.                *
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

#include <iomanip>
#include <sstream>

#include <sys/syslimits.h>

#include "cftypes.hpp"

#define NUM_SECONDS_REF_TIME_FROM_EPOCH 978307200

std::string CFStrToStr(CFStringRef cfstr) {
  size_t strLen = CFStringGetLength(cfstr)+1;
  std::vector<char> data;
  data.reserve(strLen);

  if(!CFStringGetCString(cfstr, reinterpret_cast<char*>(data.data()), strLen, kCFStringEncodingUTF8)) {
    throw std::runtime_error("Error converting CFString to std::string. Buffer is too small");
  }
  
  return std::move(std::string(reinterpret_cast<char*>(data.data())));
}
std::string CFUUIDToStr(CFUUIDRef cfuuid) {
  CFStringRef cfstr = CFUUIDCreateString(kCFAllocatorDefault, cfuuid);
  std::string uuidStr = CFStrToStr(cfstr);
  CFRelease(cfstr);
  return std::move(uuidStr);
}

std::string CFURLToStr(CFURLRef cfurl) {
  std::vector<char> data;
  data.reserve(PATH_MAX+1);

  Boolean result = CFURLGetFileSystemRepresentation(cfurl, true, reinterpret_cast<UInt8*>(data.data()), PATH_MAX+1);
  if(!result) {
    throw std::runtime_error("Conversion from CFURL to string failed");
  }

  return std::move(std::string(reinterpret_cast<char*>(data.data())));
}

std::string CFDataToStr(CFDataRef cfdata) {
  size_t dataLen = CFDataGetLength(cfdata);
  const UInt8* data = CFDataGetBytePtr(cfdata);
  
  std::stringstream ss;
  ss << std::hex;
  for(unsigned int i = 0; i < dataLen; ++i) {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
  }

  return ss.str();
}

bool CFBoolToBool(CFBooleanRef cfbool) {
  return static_cast<bool>(CFBooleanGetValue(cfbool));
}

int64_t CFNumberToInt(CFNumberRef cfnum) {
  if(CFNumberIsFloatType(cfnum)) {
    throw std::runtime_error("Attempted to convert float CFNumber to int");
  }

  int64_t value;
  Boolean result = CFNumberGetValue(cfnum, kCFNumberSInt64Type, &value);
  if(!result) {
    throw std::runtime_error("Conversion from CFNumber to int64 failed");
  }

  return value;
}

float CFNumberToFloat(CFNumberRef cfnum) {
  if(!CFNumberIsFloatType(cfnum)) {
    throw std::runtime_error("Attempted to convert int CFNumber to float");
  }

  float value;
  Boolean result = CFNumberGetValue(cfnum, kCFNumberFloat32Type, &value);
  if(!result) {
    throw std::runtime_error("Conversion from CFNumber to float failed");
  }

  return value;
}

double CFNumberToDouble(CFNumberRef cfnum) {
  if(!CFNumberIsFloatType(cfnum)) {
    throw std::runtime_error("Attempted to convert int CFNumber to float");
  }

  double value;
  Boolean result = CFNumberGetValue(cfnum, kCFNumberFloat64Type, &value);
  if(!result) {
    throw std::runtime_error("Conversion from CFNumber to double failed");
  }

  return value;  
}

uint64_t CFTimeIntervalToEpoch(double timeRef) {
  return static_cast<uint64_t>(timeRef) + NUM_SECONDS_REF_TIME_FROM_EPOCH;
}

CFTypeRef getKey(CFDictionaryRef dict, const std::string key) {
  CFStringRef keyCFStr = CFStringCreateWithCString(NULL, key.c_str(), kCFStringEncodingUTF8);
  CFTypeRef value = CFDictionaryGetValue(dict, keyCFStr);
  CFRelease(keyCFStr);
  return value;
}

std::string formatStringAsGUID(const std::string& input) {
  // XXXXXXXX                  -    XXXX                  -    XXXX                   -    XXXX                   -    XXXXXXXXXXXX
  return std::move(input.substr(0, 8) + "-" + input.substr(8, 4) + "-" + input.substr(12, 4) + "-" + input.substr(16, 4) + "-" + input.substr(20, 12));
}