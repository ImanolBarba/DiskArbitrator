/***************************************************************************
 *   list.cpp  --  This file is part of diskarbitratorctl.                 *
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


#include <cxxopts.hpp>

#include "diskarbitrator.grpc.pb.h"

#include "common.hpp"
#include "client.hpp"
#include "socket.hpp"

#define COLUMN_SEPARATION 2
#define DECIMAL_PLACES 2

std::map<std::string, size_t> getColumnWidths(std::vector<std::map<std::string, std::string>>& values) {
  std::map<std::string, size_t> colWidths;
  std::vector<std::string> colNames;

  // get the column list using the first value, all of them should be the same
  for(const auto& it : values[0]) {
    colNames.push_back(it.first);
  }

  // For every column, get the value with the maximum length, and calculate how many spaces we need to print it
  for(const auto& col : colNames) {
    size_t maxFieldLength = 0;
    for(auto& it: values) {
      size_t strSize = it[col].size();
      if(strSize > maxFieldLength) {
        maxFieldLength = strSize;
      }
    }
    colWidths[col] = std::max(maxFieldLength, col.size());
  }

  return std::move(colWidths);
}

void printValue(std::map<std::string, size_t>& colWidths, const std::string& col, const std::string& val, bool alignRight = false) {
  if(!alignRight) {
    std::cout << val;
    for(unsigned int i = val.size(); i < (colWidths[col] + COLUMN_SEPARATION); ++i) {
      std::cout << " ";
    }
  } else {
    for(unsigned int i = val.size(); i < (colWidths[col]); ++i) {
      std::cout << " ";
    }
    std::cout << val;
    for(unsigned int i = 0; i < COLUMN_SEPARATION; ++i) {
      std::cout << " ";
    }
  }
}

// This code is a horrible O(n²) inefficient mess. Just don't look at it.
std::vector<diskarbitrator::Disk> orderDisks(std::vector<diskarbitrator::Disk>& disks) {
  std::vector<diskarbitrator::Disk> orderedDisks;
  std::vector<diskarbitrator::Disk> parentDisks;
  for(const auto& d : disks) {
    if(d.description().media_whole()) {
      parentDisks.push_back(d);
    }
  }

  for(const auto& d: parentDisks) {
    orderedDisks.push_back(d);

    std::vector<std::string> childrenSorted;
    for(const auto& child : d.children()) {
      childrenSorted.push_back(child);
    }
    std::sort(childrenSorted.begin(), childrenSorted.end());
    
    for(const auto& child : childrenSorted) {
      for(unsigned int i = 0; i < disks.size(); ++i) {
        if(disks[i].disk() == child) {
          orderedDisks.push_back(disks[i]);
          break;
        }
      }
    }
  }
  return std::move(orderedDisks);
}

// Took inspiration from `lsblk` because `lsblk` is AWESOME
void printDiskList(std::vector<diskarbitrator::Disk>& disks) {
  // order disks hierarchically
  std::vector<diskarbitrator::Disk> orderedDisks = orderDisks(disks);

  // put values in columns
  std::vector<std::map<std::string, std::string>> values;
  for(unsigned int i = 0; i < orderedDisks.size(); ++i) {
    std::map<std::string, std::string> diskValuesMap;

    // Extra chars for hierarchy tree view
    std::string prefix;
    if(orderedDisks[i].description().media_whole()) {
      prefix = "";
    } else {
      if(i == (orderedDisks.size() - 1) || orderedDisks[i+1].parent_disk() != orderedDisks[i].parent_disk()) {
        // TODO: Using the fancy unicode symbols like ├─disk4 causes some real issues with calculation of column length. I'll fix it eventually
        prefix = "\\_";
      } else {
        prefix = "|-";
      }
    }
    diskValuesMap["DISK"] = prefix + orderedDisks[i].disk();
    diskValuesMap["NAME"] = orderedDisks[i].description().volume_name();
    diskValuesMap["RM"] = orderedDisks[i].description().media_removable() ? "1" : "0";
    diskValuesMap["SIZE"] = sizeToHuman(orderedDisks[i].description().media_size());
    diskValuesMap["RO"] = orderedDisks[i].description().media_writable() ? "0" : "1";
    diskValuesMap["TYPE"] = orderedDisks[i].description().media_whole() ? "disk" : "part";
    diskValuesMap["FS"] = orderedDisks[i].description().volume_kind();
    diskValuesMap["MOUNT"] = orderedDisks[i].description().volume_path();
    values.push_back(diskValuesMap);
  }

  // get column widths
  std::map<std::string, size_t> colWidths = getColumnWidths(values);

  // print header
  printValue(colWidths, "DISK", "DISK");
  printValue(colWidths, "NAME", "NAME");
  printValue(colWidths, "RM", "RM");
  printValue(colWidths, "SIZE", "SIZE");
  printValue(colWidths, "RO", "RO");
  printValue(colWidths, "TYPE", "TYPE");
  printValue(colWidths, "FS", "FS");
  printValue(colWidths, "MOUNT", "MOUNT");
  std::cout << std::endl;

  // print disks
  for(auto& d : values) {
    printValue(colWidths, "DISK", d["DISK"]);
    printValue(colWidths, "NAME", d["NAME"]);
    printValue(colWidths, "RM", d["RM"]);
    printValue(colWidths, "SIZE", d["SIZE"], true);
    printValue(colWidths, "RO", d["RO"]);
    printValue(colWidths, "TYPE", d["TYPE"]);
    printValue(colWidths, "FS", d["FS"]);
    printValue(colWidths, "MOUNT", d["MOUNT"]);
    std::cout << std::endl;
  }
}

bool doList(int argc, char** argv) {
  cxxopts::Options options("diskarbitratorctl list", "list: Lists available disks in the system");
  options.add_options()
      ("s,socket", "diskarbitratord socket path", cxxopts::value<std::string>()->default_value(DEFAULT_SOCKET_PATH))
      ("h,help", "Print usage")
  ;
  
  std::string socketPath;

  try {
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
  } catch(const cxxopts::exceptions::exception& ex) {
    std::cout << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return false;
  }
  
  DiskArbitratorClient client = getClient(socketPath);
  std::vector<diskarbitrator::Disk> disks = client.ListDisks();
  printDiskList(disks);
  
  return true;
}