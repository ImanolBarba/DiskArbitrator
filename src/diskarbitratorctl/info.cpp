/***************************************************************************
 *   info.cpp  --  This file is part of diskarbitratorctl.                 *
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

#include <cxxopts.hpp>

#include "diskarbitrator.grpc.pb.h"

#include "common.hpp"
#include "client.hpp"
#include "socket.hpp"

static void printDiskDescription(diskarbitrator::DiskDescription& desc) {
  std::cout << "== DISK INFO ==" << std::endl;
  std::cout << "Disk Appeared at: " << unixTimeToString(desc.appearance_time()) << std::endl;
  std::cout << "Disk BSD Name: " << desc.media_bsd_name() << std::endl;
  std::cout << std::endl;

  std::cout << "== VOLUME INFO ==" << std::endl;
  if(desc.has_volume_name()) {
    std::cout << "Volume Name: " << desc.volume_name() << std::endl;
  }
  if(desc.has_volume_kind()) {
    std::cout << "Volume Kind: " << desc.volume_kind() << std::endl;
  }
  if(desc.has_volume_mountable()) {
    std::cout << "Is Mountable: " << (desc.volume_mountable() ? "true" : "false") << std::endl;
  }
  if(desc.has_media_ejectable()) {
    std::cout << "Is Ejectable: " << (desc.media_ejectable() ? "true" : "false") << std::endl;
  }
  if(desc.has_volume_path()) {
    std::cout << "Mounted at :" << desc.volume_path() << std::endl;
  }
  if(desc.has_volume_uuid()) {
    std::cout << "Volume UUID: " << desc.volume_uuid() << std::endl;
  }
  if(desc.has_volume_network()) {
    std::cout << "Is a network volume: " << (desc.volume_network() ? "true" : "false") << std::endl;
  }
  std::cout << std::endl;

  std::cout << "== MEDIA INFO ==" << std::endl;
  if(desc.has_media_name()) {
    std::cout << "Media Name: " << desc.media_name() << std::endl;
  }
  if(desc.has_media_size()) {
    std::cout << "Media Size: " << sizeToHuman(desc.media_size()) << std::endl;
  }
  if(desc.has_media_block_size()) {
    std::cout << "Block Size: " << desc.media_block_size() << std::endl;
  }
  if(desc.has_media_removable()) {
    std::cout << "Is Removable: " << (desc.media_removable() ? "true" : "false") << std::endl;
  }
  if(desc.has_media_writable()) {
    std::cout << "Is Writable: " << (desc.media_writable() ? "true" : "false") << std::endl;
  }
  if(desc.has_media_whole()) {
    std::cout << "Is Whole Disk: " << (desc.media_whole() ? "true" : "false") << std::endl;
  }
  if(desc.has_media_leaf()) {
    std::cout << "Is Leaf Disk: " << (desc.media_leaf() ? "true" : "false") << std::endl;
  }
  if(desc.has_media_type()) {
    std::cout << "Media Type: " << desc.media_type() << std::endl;
  }
  if(desc.has_media_uuid()) {
    std::cout << "Media UUID: " << desc.media_uuid() << std::endl;
  }
  if(desc.has_media_kind()) {
    std::cout << "Media Kind: " << desc.media_kind() << std::endl;
  }
  if(desc.has_media_bsd_major()) {
    std::cout << "BSD Major Number: " << desc.media_bsd_major() << std::endl;
  }
  if(desc.has_media_bsd_minor()) {
    std::cout << "BSD Minor Number: " << desc.media_bsd_minor() << std::endl;
  }
  if(desc.has_media_bsd_unit()) {
    std::cout << "BSD Unit: " << desc.media_bsd_unit() << std::endl;
  }
  if(desc.has_media_content()) {
    std::cout << "Media Content: " << desc.media_content() << std::endl;
  }
  if(desc.has_media_path()) {
    std::cout << "Media Path: " << desc.media_path() << std::endl;
  }
  if(desc.media_icon().size()) {
    std::cout << "Media Icon = {" << std::endl; 
    for(const auto& it : desc.media_icon()) {
      std::cout << "\t" << it.first << ": " << it.second << std::endl;
    }
    std::cout << "}" << std::endl; 
  }
  std::cout << std::endl;

  std::cout << "== DEVICE INFO ==" << std::endl;
  if(desc.has_device_vendor()) {
    std::cout << "Device Vendor: " << desc.device_vendor() << std::endl;
  }
  if(desc.has_device_model()) {
    std::cout << "Device Model: " << desc.device_model() << std::endl;
  }
  if(desc.has_device_guid()) {
    std::cout << "Device GUID: " << desc.device_guid() << std::endl;
  }
  if(desc.has_device_internal()) {
    std::cout << "Is Internal Device: " << (desc.device_internal() ? "true" : "false") << std::endl;
  }
  if(desc.has_device_protocol()) {
    std::cout << "Device Protocol: " << desc.device_protocol() << std::endl;
  }
  if(desc.has_device_path()) {
    std::cout << "Device Path: " << desc.device_path() << std::endl;
  }
  if(desc.has_device_revision()) {
    std::cout << "Device Revision: " << desc.device_revision() << std::endl;
  }
  if(desc.has_device_unit()) {
    std::cout << "Device Unit: " << desc.device_unit() << std::endl;
  }
  
  std::cout << std::endl;
  std::cout << "== BUS INFO ==" << std::endl;
  if(desc.has_bus_name()) {
    std::cout << "Bus Name: " << desc.bus_name() << std::endl;
  }
  if(desc.has_bus_path()) {
    std::cout << "Bus Path: " << desc.bus_path() << std::endl;
  }
}

bool doInfo(int argc, char** argv) {
  cxxopts::Options options("diskarbitratorctl info", "info: Shows information about a specific disk");
  options.add_options()
      ("disk", "Disk to get info from", cxxopts::value<std::string>())
      ("s,socket", "diskarbitratord socket path", cxxopts::value<std::string>()->default_value(DEFAULT_SOCKET_PATH))
      ("h,help", "Print usage")
  ;
  
  std::string socketPath;
  std::string disk;

  try {
    options.parse_positional({"disk"});
    options.positional_help("disk");
    cxxopts::ParseResult result = options.parse(argc, argv);
    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return true;
    }

    if(!result.count("disk")) {
      std::cout << "disk argument was not provided" << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }

    std::vector<std::string> unmatched = result.unmatched();
    if(unmatched.size()) {
      std::cout << "Unrecognized argument: " << unmatched[0] << std::endl;
      std::cout << options.help() << std::endl;
      return false;
    }

    socketPath = result["socket"].as<std::string>();
    disk = result["disk"].as<std::string>();
  } catch(const cxxopts::exceptions::exception& ex) {
    std::cout << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return false;
  }

  DiskArbitratorClient client = getClient(socketPath);
  std::unique_ptr<diskarbitrator::DiskDescription> desc = client.DiskInfo(disk);
  if(desc == nullptr) {
    return false;
  }
  std::cout << "Printing disk description for disk " << disk << ":" << std::endl;
  printDiskDescription(*desc);
  
  return true;
}

