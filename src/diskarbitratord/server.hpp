/***************************************************************************
 *   server.hpp  --  This file is part of diskarbitratord.                 *
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

#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <map>
#include <string>
#include <thread>

#include <DiskArbitration/DiskArbitration.h>

#include <glog/logging.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "diskarbitrator.grpc.pb.h"

#include "diskarbitration.hpp"
#include "hdiutil.hpp"

class DiskAbitratorServiceImpl final : public diskarbitrator::DiskArbitrator::Service {
  private:
    bool StopArbitration();
    void startIntercept();
    void stopIntercept();
    std::map<std::string, std::shared_ptr<diskarbitrator::Disk>> disks;
    DASessionRef approvalSession;

  public:
    DiskAbitratorServiceImpl() {};
    ~DiskAbitratorServiceImpl() {
      // stop interception if in-place
      if(this->arbitrationMode != diskarbitrator::ArbitrationMode::ARBITRATOR_NONE) {
        this->stopIntercept();
      }

      // Stop arbitration entirely
      if(!this->StopArbitration()) {
        LOG(WARNING) << "Unable to stop arbitration session" << std::endl;
      } else {
        LOG(INFO) << "Arbitration session stopped" << std::endl;
      }
    }

    // Here come all the RPC handling routines
    grpc::Status MountDisk(grpc::ServerContext* context, const diskarbitrator::MountDiskInput* request, diskarbitrator::MountDiskOutput* reply) override {
      std::string args = "";
      for(const auto& arg : request->arguments()) {
        args += arg + ",";
      }
      args = args.substr(0, args.size()-1);

      LOG(INFO) << "Requested disk mount for disk " << request->disk() 
                << " with mode " << diskarbitrator::MountMode_Name(request->mode()) 
                << " path " << (request->has_path() ? request->path() : "(default)") 
                << (request->arguments().size() ? (" args (" + args + ")" ) : "");
      try {
        if(this->disks.find(request->disk()) == this->disks.end()) {
          return grpc::Status(grpc::NOT_FOUND, "Requested disk was not found");
        }
        this->ourMounts[request->disk()] = true;
        diskarbitrator::Disk disk = *(this->disks[request->disk()]);
        std::vector<std::string> args;
        for(const auto& arg : request->arguments()) {
          args.push_back(arg);
        }
        std::string mountPath;
        if(request->has_path()) {
          mountPath = mountDisk(this->session, disk, request->mode(), args, request->path());  
        } else {
          mountPath = mountDisk(this->session, disk, request->mode(), args);
        }
        this->ourMounts.erase(request->disk());
        reply->set_path(mountPath);
      } catch(const std::runtime_error& e) {
        return grpc::Status(grpc::ABORTED, e.what());
      }  
      return grpc::Status::OK;
    }

    grpc::Status UnmountDisk(grpc::ServerContext* context, const diskarbitrator::UnmountDiskInput* request, google::protobuf::Empty* reply) override {
      LOG(INFO) << "Requested disk unmount for disk " << request->disk();
      try {
        if(this->disks.find(request->disk()) == this->disks.end()) {
          return grpc::Status(grpc::NOT_FOUND, "Requested disk was not found");
        }
        diskarbitrator::Disk disk = *(this->disks[request->disk()]);
        unmountDisk(session, disk);
      } catch(const std::runtime_error& e) {
        return grpc::Status(grpc::ABORTED, e.what());
      }  
      return grpc::Status::OK;
    }

    grpc::Status EjectDisk(grpc::ServerContext* context, const diskarbitrator::EjectDiskInput* request, google::protobuf::Empty* reply) override {
      LOG(INFO) << "Requested disk eject for " << request->disk();
      try {
        if(this->disks.find(request->disk()) == this->disks.end()) {
          return grpc::Status(grpc::NOT_FOUND, "Requested disk was not found");
        }
        diskarbitrator::Disk disk = *(this->disks[request->disk()]);
        ejectDisk(this->session, disk);
      } catch(const std::runtime_error& e) {
        LOG(ERROR) << "Eject FAILED: " << e.what();
        return grpc::Status(grpc::ABORTED, e.what());
      }  
      return grpc::Status::OK;
    }

    grpc::Status AttachDisk(grpc::ServerContext* context, const diskarbitrator::AttachDiskInput* request, diskarbitrator::AttachDiskOutput* reply) override {
      LOG(INFO) << "Requested disk attach for image " << request->disk() << " with mode " << diskarbitrator::MountMode_Name(request->mode());
      try {
        std::vector<std::string> disks = attachDisk(request->disk(), request->mode());
        for (const auto& it : disks) {
          std::string* newDisk = reply->add_disks();
          *newDisk = it;
        }
      } catch(const std::runtime_error& e) {
        return grpc::Status(grpc::ABORTED, e.what());
      }  
      return grpc::Status::OK;
    }

    grpc::Status DiskInfo(grpc::ServerContext* context, const diskarbitrator::DiskInfoInput* request, diskarbitrator::DiskDescription* reply) override {
      LOG(INFO) << "Requested disk info for disk " << request->disk();
      if(this->disks.find(request->disk()) == this->disks.end()) {
        return grpc::Status(grpc::NOT_FOUND, "The specified disk was not found in the system");
      }
      *reply = this->disks[request->disk()]->description();
      return grpc::Status::OK;
    }

    grpc::Status ListDisks(grpc::ServerContext* context, const google::protobuf::Empty* request, diskarbitrator::ListDisksOutput* reply) override {
      LOG(INFO) << "Requested disk list";
      for (const auto& it : this->disks) {
        diskarbitrator::Disk* newDisk = reply->add_disks();
        *newDisk = *(it.second);
      }
      return grpc::Status::OK;
    }

    grpc::Status Arbitrate(grpc::ServerContext* context, const diskarbitrator::ArbitrateInput* request, google::protobuf::Empty* reply) override {
      LOG(INFO) << "Requested disk arbitration with mode " << diskarbitrator::ArbitrationMode_Name(request->mode());
      if(request->mode() != this->arbitrationMode) {
        try {
          if(request->mode() == diskarbitrator::ArbitrationMode::ARBITRATOR_NONE) {
            // New status is disabled, current status is any of the two enabled
            this->stopIntercept();
          } else if(this->arbitrationMode == diskarbitrator::ArbitrationMode::ARBITRATOR_NONE) {
            // Current status is disabled, new status is any of the two enabled
            this->startIntercept();
          }
          this->arbitrationMode = request->mode();
        } catch(const std::runtime_error& e) {
          return grpc::Status(grpc::ABORTED, e.what());
        } 
        return grpc::Status::OK;
      }
      return grpc::Status(grpc::ALREADY_EXISTS, "Already arbitrating in the requested mode");
    }

    CFRunLoopRef runLoop;
    
    // The approval callback needs to query these
    DASessionRef session;
    diskarbitrator::ArbitrationMode arbitrationMode = diskarbitrator::ArbitrationMode::ARBITRATOR_NONE;
    std::map<std::string, bool> ourMounts;

    // We need to make this one public. The user should call this method after
    // instantiating the service. The reason for this is to avoid any callback
    // functions being called before the object has been constructed
    bool StartArbitration();

    // These are called from the callback functions
    void addDisk(std::shared_ptr<diskarbitrator::Disk>& disk);
    void removeDisk(const std::string& disk);
    bool diskExists(const std::string& disk);
    void addChildToParent(const std::string& disk, const std::string& parentDisk);
    void removeChildFromParent(const std::string& disk, const std::string& parentDisk);
    const std::string getParentDisk(const std::string& disk);
    void updateDiskDescription(const std::string& disk, const diskarbitrator::DiskDescription& description);
};

// Starts the server. What else?
void RunServer(const std::string& socketPath, int pipeFD);

#endif