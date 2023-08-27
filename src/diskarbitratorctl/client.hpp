/***************************************************************************
 *   client.hpp  --  This file is part of diskarbitratorctl.               *
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


#ifndef CLIENT_HPP_H
#define CLIENT_HPP_H

#include <grpcpp/grpcpp.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "diskarbitrator.grpc.pb.h"

class DiskArbitratorClient {
 public:
  DiskArbitratorClient(std::shared_ptr<grpc::Channel> channel)
      : stub(diskarbitrator::DiskArbitrator::NewStub(channel)) {}

  std::vector<diskarbitrator::Disk> ListDisks() {
    grpc::ClientContext context;

    ::google::protobuf::Empty request;
    diskarbitrator::ListDisksOutput reply;

    grpc::Status status = stub->ListDisks(&context, request, &reply);

    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
      return {};
    }

    std::vector<diskarbitrator::Disk> disks;
    for (unsigned int i = 0; i < reply.disks_size(); ++i) {
      disks.push_back(std::move(reply.disks(i)));
    }

    return std::move(disks);
  }

  bool EjectDisk(const std::string& disk) {
    grpc::ClientContext context;

    diskarbitrator::EjectDiskInput request;
    ::google::protobuf::Empty reply;
    request.set_disk(disk);

    grpc::Status status = stub->EjectDisk(&context, request, &reply);

    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
      return false;
    }

    return true;
  }

  std::string MountDisk(const std::string& disk, diskarbitrator::MountMode mode, std::vector<std::string> args, const std::string& path = "") {
    grpc::ClientContext context;

    diskarbitrator::MountDiskInput request;
    diskarbitrator::MountDiskOutput reply;
    request.set_disk(disk);
    request.set_mode(mode);
    if(path.size()) {
      request.set_path(path);  
    }
    for(const auto& arg : args) {
      std::string* newArg = request.add_arguments();
      *newArg = arg;
    }

    grpc::Status status = stub->MountDisk(&context, request, &reply);

    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "";
    }

    return reply.path();
  }

  bool UnmountDisk(const std::string& disk) {
    grpc::ClientContext context;

    diskarbitrator::UnmountDiskInput request;
    ::google::protobuf::Empty reply;
    request.set_disk(disk);

    grpc::Status status = stub->UnmountDisk(&context, request, &reply);

    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
      return false;
    }

    return true;
  }

  bool Arbitrate(diskarbitrator::ArbitrationMode mode) {
    grpc::ClientContext context;

    diskarbitrator::ArbitrateInput request;
    ::google::protobuf::Empty reply;
    request.set_mode(mode);

    grpc::Status status = stub->Arbitrate(&context, request, &reply);

    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
      return false;
    }

    return true;
  }

  std::unique_ptr<diskarbitrator::DiskDescription> DiskInfo(const std::string& disk) {
    grpc::ClientContext context;

    diskarbitrator::DiskInfoInput request;
    diskarbitrator::DiskDescription* reply = new diskarbitrator::DiskDescription;
    request.set_disk(disk);

    grpc::Status status = stub->DiskInfo(&context, request, reply);

    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
      return nullptr;
    }

    return std::move(std::unique_ptr<diskarbitrator::DiskDescription>(reply));
  }

  std::vector<std::string> AttachDisk(const std::string& disk, diskarbitrator::MountMode mode) {
    grpc::ClientContext context;

    diskarbitrator::AttachDiskInput request;
    diskarbitrator::AttachDiskOutput reply;
    request.set_disk(disk);
    request.set_mode(mode);

    grpc::Status status = stub->AttachDisk(&context, request, &reply);

    std::vector<std::string> disks;
    if(!status.ok()) {
      std::cerr << status.error_code() << ": " << status.error_message()
                << std::endl;
    } else {
      for(const auto& disk : reply.disks()) {
        disks.push_back(disk);
      }
    }

    return disks;
  }

 private:
  std::unique_ptr<diskarbitrator::DiskArbitrator::Stub> stub;
};

#endif