/***************************************************************************
 *   diskarbitration.cpp  --  This file is part of diskarbitratord.        *
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

#include <future>

#include <sys/stat.h>
#include <mach/mach_error.h>

#include <DiskArbitration/DiskArbitration.h>

#include "cftypes.hpp"
#include "scope_guard.hpp"
#include "server.hpp"

#define DEFAULT_MACOS_MOUNT_ROOT "/Volumes"

enum Decision {
  DECISION_ALLOW,
  DECISION_DENY,
  DECISION_REMOUNT_RO
};

void CFLoop(DiskAbitratorServiceImpl* instance, DASessionRef session) {
  LOG(INFO) << "CF Run loop starting...";
  instance->runLoop = CFRunLoopGetCurrent();
  DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
  CFRunLoopRun();
  LOG(INFO) << "CF Run loop terminated";
}

Decision approveMount(diskarbitrator::Disk& disk, diskarbitrator::ArbitrationMode mode, bool ourMount) {
  if(mode != diskarbitrator::ArbitrationMode::ARBITRATOR_NONE && !ourMount) {
    if(mode == diskarbitrator::ArbitrationMode::ARBITRATOR_BLOCK) {
      return DECISION_DENY;
    } else if(mode == diskarbitrator::ArbitrationMode::ARBITRATOR_RDONLY) {
      // So, how does the read-only mode even work?
      // Well, we first *reject* the mount request that comes through
      // Then, we create our OWN request, and accept that instead
      // Easy enough!
      //
      // There's a huge limitation with this approach, any original mount options 
      // are not recovered by the arbitrator, so they are lost.
      // TODO:  Figure out if there's a way to obtain these
      return DECISION_REMOUNT_RO;
    }
  }
  return DECISION_ALLOW;
}

void remountRO(std::shared_ptr<diskarbitrator::Disk> disk, DiskAbitratorServiceImpl* instance) {
  instance->ourMounts[disk->disk()] = true;
  mountDisk(instance->session, *disk, diskarbitrator::MountMode::MOUNT_RDONLY, {});
  instance->ourMounts.erase(disk->disk());
}

// This function is called from the framework when a disk is attached. Purely
// informational. Does not allow to do anything.
void DiskAppearedCallback(DADiskRef diskRef, void *context) {
  DiskAbitratorServiceImpl* instance = reinterpret_cast<DiskAbitratorServiceImpl*>(context);
  std::shared_ptr<diskarbitrator::Disk> disk = genDisk(diskRef, instance);
  LOG(INFO)  << "Disk appeared: " << disk->disk();
  
  // We might've already added this disk (see note below about whole disks), so
  // we need to check that it exists first. For the converse case when it
  // disappears, we'll also do the same check ~because it looks pretty~ for
  // consistency, but the `addDisk` and `removeDisk` functions already check
  // for presence/absence of the key
  if(!instance->diskExists(disk->disk())) {
    instance->addDisk(disk);
  }
}

// This function is called from the framework when a disk is detached. Purely
// informational. Does not allow to do anything.
void DiskDisappearedCallback(DADiskRef diskRef, void *context) {
  DiskAbitratorServiceImpl* instance = reinterpret_cast<DiskAbitratorServiceImpl*>(context);
  std::shared_ptr<diskarbitrator::Disk> disk = genDisk(diskRef, instance);
  LOG(INFO)  << "Disk disappeared: " << disk->disk();
  const std::string parentDisk = instance->getParentDisk(disk->disk());
  if(parentDisk.size() && instance->diskExists(parentDisk)) {
    instance->removeChildFromParent(disk->disk(), parentDisk);
  }
  if(instance->diskExists(disk->disk())) {
    for (const std::string& slice : disk->children()) {
      instance->removeDisk(slice);
    }
    instance->removeDisk(disk->disk());
  }
}

// This function is called from the framework when a disk changes description.
// Purely informational. Does not allow to do anything.
void DiskDescriptionChangedCallback(DADiskRef diskRef, CFArrayRef keys, void *context) {
  DiskAbitratorServiceImpl* instance = reinterpret_cast<DiskAbitratorServiceImpl*>(context);
  std::shared_ptr<diskarbitrator::Disk> disk = genDisk(diskRef, instance);
  LOG(INFO) << "Disk description changed: " << disk->disk();
  // The reason we do this instead of removing and adding the disk, is because
  // we would lose the parent/children info about the disk otherwise.
  instance->updateDiskDescription(disk->disk(), disk->description());
}

// This function is called from the framework when arbitration is enabled and
// a mount operation is requested from somewhere. Returning NULL means allowing
// the request, returning a DADissenter is rejecting it
DADissenterRef __attribute__((cf_returns_retained)) DiskMountApprovalCallback(DADiskRef diskRef, void *context) {
  DiskAbitratorServiceImpl* instance = reinterpret_cast<DiskAbitratorServiceImpl*>(context);
  std::shared_ptr<diskarbitrator::Disk> disk = genDisk(diskRef);

  LOG(INFO) << "Mount intercepted for disk: " << disk->disk();

  bool ourMount = instance->ourMounts.find(disk->disk()) != instance->ourMounts.end();
  Decision decision = approveMount(*disk, instance->arbitrationMode, ourMount); 
  DADissenterRef dissenter = NULL;
  if(decision != DECISION_ALLOW) {
    std::string message;
    if(decision == DECISION_DENY) {
      message = "Mounts in this system are currently blocked";
      LOG(INFO) << "Mount blocked for disk: " << disk->disk();
    } else {
      message = "Forcing mount read-only";
      LOG(INFO) << "Mount forced read-only for disk: " << disk->disk();
      std::thread remountThread = std::thread(&remountRO, disk, instance);
      remountThread.detach();
    }
    CFStringRef messageCFStr = CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);
    dissenter = DADissenterCreate(kCFAllocatorDefault, kDAReturnNotPermitted, messageCFStr);
    CFRelease(messageCFStr);
  } else {
    LOG(INFO) << "Mount allowed for disk: " << disk->disk();
  }
  return dissenter;
}

void ejectDisk(DASessionRef session, diskarbitrator::Disk& disk) {
  // TODO: Ejecting APFS containers seems to not eject the underlaying disk
  if(disk.description().has_media_ejectable() && !disk.description().media_ejectable()) {
    throw std::runtime_error("Disk is not ejectable");
  }

  // If this disk has slices, we need to make sure everything from this disk is unmounted
  if(disk.children().size()) {
    try {
      unmountDisk(session, disk);
    } catch(const std::runtime_error& ex) {
      // ignore
    }
  }

  std::promise<std::string> errorPromise;
  std::future<std::string> errorFuture = errorPromise.get_future();
  
  DADiskRef diskRef = DADiskCreateFromBSDName(kCFAllocatorDefault, session, disk.disk().c_str());
	DADiskEject(diskRef, kDADiskEjectOptionDefault, &checkSuccess, &errorPromise);

  CFRelease(diskRef);

  std::string error = errorFuture.get();
  if(error.size()) {
    throw std::runtime_error("Error ejecting disk: " + error);
  }
}

const std::string mountDisk(DASessionRef session, diskarbitrator::Disk& disk, diskarbitrator::MountMode mode, std::vector<std::string> args, const std::string& path) {
  if(disk.description().has_volume_mountable() && !disk.description().volume_mountable()) {
    throw std::runtime_error("Disk is not mountable");
  }
  if(disk.description().has_volume_path() && disk.description().volume_path().size()) {
    throw std::runtime_error("Disk is already mounted");
  }

  if(mode == diskarbitrator::MountMode::MOUNT_RDONLY) {
    if(disk.description().volume_kind() == "hfs") {
      // HFS will reject RO mounts if the journal is dirty, so we need an extra
      // option to ignore it
      args.push_back("-j");
    }
    args.push_back("rdonly");
  }

  std::promise<std::string> errorPromise;
  std::future<std::string> errorFuture = errorPromise.get_future();

  DADiskRef diskRef = DADiskCreateFromBSDName(kCFAllocatorDefault, session, disk.disk().c_str());
  if(diskRef == NULL) {
    throw std::runtime_error("Unable to obtain disk reference");
  }
  CFURLRef urlRef = NULL;
  CFStringRef pathCFStr = NULL;

  if(path.size()) {
    pathCFStr = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
    urlRef = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.c_str()), path.size(), true);
  }
  
  std::vector<CFStringRef> argv;
  for(const auto& arg: args) {
    CFStringRef cfstr = CFStringCreateWithCString(kCFAllocatorDefault, arg.c_str(), kCFStringEncodingUTF8);
    argv.push_back(cfstr);
  }
  argv.push_back(nullptr);

  DADiskMountWithArguments(diskRef, urlRef, kDADiskMountOptionDefault, &checkSuccess, &errorPromise, argv.data());

  for(const auto& arg: argv) {
    if(arg != nullptr) {
      CFRelease(arg);
    }
  }

  if(path.size()) {
    CFRelease(urlRef);
    CFRelease(pathCFStr);
  }
  CFRelease(diskRef);


  std::string error = errorFuture.get();
  if(error.size()) {
    throw std::runtime_error("Error mounting disk: " + error);
  }

  // Get DADisk reference again, in order to obtain the current mountpoint
  diskRef = DADiskCreateFromBSDName(kCFAllocatorDefault, session, disk.disk().c_str());
  if(diskRef == NULL) {
    throw std::runtime_error("Unable to obtain disk reference after mount");
  }
  std::shared_ptr<diskarbitrator::Disk> mountedDisk = genDisk(diskRef);
  CFRelease(diskRef);

  if(!mountedDisk->description().has_volume_path()) {
    throw std::runtime_error("Disk has no mountpoint even after mount operation completed");
  }
  return mountedDisk->description().volume_path();
}

void unmountDisk(DASessionRef session, diskarbitrator::Disk& disk) {
  if(disk.description().has_volume_path() && !disk.description().volume_path().size()) {
    throw std::runtime_error("Disk is not mounted");
  }

  UInt32 unmountOpts = kDADiskUnmountOptionDefault;

  if(disk.description().media_whole()) {
    unmountOpts |= kDADiskUnmountOptionWhole;
  }

  std::promise<std::string> errorPromise;
  std::future<std::string> errorFuture = errorPromise.get_future();
  
  DADiskRef diskRef = DADiskCreateFromBSDName(kCFAllocatorDefault, session, disk.disk().c_str());
	DADiskUnmount(diskRef, unmountOpts, &checkSuccess, &errorPromise);

  CFRelease(diskRef);

  std::string error = errorFuture.get();
  if(error.size()) {
    throw std::runtime_error("Error unmounting disk: " + error);
  }
}

void checkSuccess(DADiskRef diskRef, DADissenterRef dissenterRef, void *context) {
  std::string errorString = "";
  std::promise<std::string>* promise = reinterpret_cast<std::promise<std::string>*>(context); 
  if(dissenterRef) {
    DAReturn status = DADissenterGetStatus(dissenterRef);
    CFStringRef statusCFStr = DADissenterGetStatusString(dissenterRef);
    if(statusCFStr == NULL) {
			IOReturn system = err_get_system(status);
			IOReturn subsystem = err_get_sub(status);
			IOReturn code = err_get_code(status);

      if(system == ERR_KERN && subsystem == SUB_UNIX) {
        // (os/unix) error codes is errno codes
        errorString = "Error (errno: " + std::to_string(static_cast<int>(code)) + "): " + std::string(strerror(code));
      } else if(system == ERR_LOCAL && subsystem == SUB_DISKARBITRATION) {
        // (local/diskarbitration) specific error codes
        errorString = "Error (DA Error Code: " + std::to_string(static_cast<int>(code)) + "): " + genErrorDescription(code);
      } else {
        // anything else
        // This doesn't work everytime, but covers reasonably getting any other possible error descriptions:

        std::string errMessage = "";
        const char* errType = mach_error_type(status);
        if(errType != NULL) {
          errMessage += std::string(errType);
        }
        const char* errDesc = mach_error_string(status);
        if(errDesc != NULL) {
          errMessage += " " + std::string(errDesc);
        }
        errorString = "Unknown error (Code: " + std::to_string(static_cast<int>(status)) + "): System: " + std::to_string(static_cast<int>(system)) + ", Subsystem: " + std::to_string(static_cast<int>(subsystem)) + ", Code: " + std::to_string(static_cast<int>(code)) + " " + errMessage;
      }
    } else {
      std::string err = CFStrToStr(statusCFStr);
      errorString = "Error (Code: " + std::to_string(static_cast<int>(status)) + "): " + err;
    }
  }
  promise->set_value(errorString);
}

std::shared_ptr<diskarbitrator::Disk> genDisk(DADiskRef& disk, DiskAbitratorServiceImpl* instance) {
  diskarbitrator::Disk* d = new diskarbitrator::Disk();

  CFDictionaryRef desc = DADiskCopyDescription(disk);

  CFTypeRef bsdName = getKey(desc, "DAMediaBSDName");
  d->mutable_description()->set_media_bsd_name(CFStrToStr((CFStringRef)bsdName));
  d->set_disk(CFStrToStr((CFStringRef)bsdName));

  CFTypeRef appearanceTime = getKey(desc, "DAAppearanceTime");
  d->mutable_description()->set_appearance_time(CFTimeIntervalToEpoch(CFNumberToDouble((CFNumberRef)appearanceTime)));

  // Strings
  CFTypeRef cfstr;

  cfstr = getKey(desc, "DAVolumeName");
  if(cfstr != NULL) {
    d->mutable_description()->set_volume_name(CFStrToStr((CFStringRef)cfstr));
  }

  cfstr = getKey(desc, "DAVolumeKind");
  if(cfstr != NULL) {
    d->mutable_description()->set_volume_kind(CFStrToStr((CFStringRef)cfstr));
  }

  cfstr = getKey(desc, "DAMediaContent");
  if(cfstr != NULL) {
    d->mutable_description()->set_media_content(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DAMediaKind");
  if(cfstr != NULL) {
    d->mutable_description()->set_media_kind(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DAMediaName");
  if(cfstr != NULL) {
    d->mutable_description()->set_media_name(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DAMediaPath");
  if(cfstr != NULL) {
    d->mutable_description()->set_media_path(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DAMediaType");
  if(cfstr != NULL) {
    d->mutable_description()->set_media_type(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DADeviceModel");
  if(cfstr != NULL) {
    d->mutable_description()->set_device_model(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DADevicePath");
  if(cfstr != NULL) {
    d->mutable_description()->set_device_path(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DADeviceProtocol");
  if(cfstr != NULL) {
    d->mutable_description()->set_device_protocol(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DADeviceRevision");
  if(cfstr != NULL) {
    d->mutable_description()->set_device_revision(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DADeviceVendor");
  if(cfstr != NULL) {
    d->mutable_description()->set_device_vendor(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DABusName");
  if(cfstr != NULL) {
    d->mutable_description()->set_bus_name(CFStrToStr((CFStringRef)cfstr));
  }
  
  cfstr = getKey(desc, "DABusPath");
  if(cfstr != NULL) {
    d->mutable_description()->set_bus_path(CFStrToStr((CFStringRef)cfstr));
  }
  
  // Booleans
  CFTypeRef cfbool;
  
  cfbool = getKey(desc, "DAMediaEjectable");
  if(cfbool != NULL) {
    d->mutable_description()->set_media_ejectable(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DAMediaWhole");
  if(cfbool != NULL) {
    d->mutable_description()->set_media_whole(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DAVolumeMountable");
  if(cfbool != NULL) {
    d->mutable_description()->set_volume_mountable(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DAVolumeNetwork");
  if(cfbool != NULL) {
    d->mutable_description()->set_volume_network(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DAMediaLeaf");
  if(cfbool != NULL) {
    d->mutable_description()->set_media_leaf(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DAMediaRemovable");
  if(cfbool != NULL) {
    d->mutable_description()->set_media_removable(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DAMediaWritable");
  if(cfbool != NULL) {
    d->mutable_description()->set_media_writable(CFBoolToBool((CFBooleanRef)cfbool));
  }

  cfbool = getKey(desc, "DADeviceInternal");
  if(cfbool != NULL) {
    d->mutable_description()->set_device_internal(CFBoolToBool((CFBooleanRef)cfbool));
  }

  // Numbers
  CFTypeRef cfnum;

  cfnum = getKey(desc, "DAMediaBlockSize");
  if(cfnum != NULL) {
    d->mutable_description()->set_media_block_size(static_cast<uint64_t>(CFNumberToInt((CFNumberRef)cfnum)));
  }

  cfnum = getKey(desc, "DAMediaBSDMajor");
  if(cfnum != NULL) {
    d->mutable_description()->set_media_bsd_major(static_cast<uint64_t>(CFNumberToInt((CFNumberRef)cfnum)));
  }

  cfnum = getKey(desc, "DAMediaBSDMinor");
  if(cfnum != NULL) {
    d->mutable_description()->set_media_bsd_minor(static_cast<uint64_t>(CFNumberToInt((CFNumberRef)cfnum)));
  }

  cfnum = getKey(desc, "DAMediaBSDUnit");
  if(cfnum != NULL) {
    d->mutable_description()->set_media_bsd_unit(static_cast<uint64_t>(CFNumberToInt((CFNumberRef)cfnum)));
  }

  cfnum = getKey(desc, "DAMediaSize");
  if(cfnum != NULL) {
    d->mutable_description()->set_media_size(static_cast<uint64_t>(CFNumberToInt((CFNumberRef)cfnum)));
  }

  cfnum = getKey(desc, "DADeviceUnit");
  if(cfnum != NULL) {
    d->mutable_description()->set_device_unit(static_cast<uint64_t>(CFNumberToInt((CFNumberRef)cfnum)));
  }

  // UUIDs
  CFTypeRef cfuuid;

  cfuuid = getKey(desc, "DAMediaUUID");
  if(cfuuid != NULL) {
    d->mutable_description()->set_media_uuid(CFUUIDToStr((CFUUIDRef)cfuuid));
  }
  
  cfuuid = getKey(desc, "DAVolumeUUID");
  if(cfuuid != NULL) {
    d->mutable_description()->set_volume_uuid(CFUUIDToStr((CFUUIDRef)cfuuid));
  }

  // Data
  CFTypeRef cfdata;
  
  cfdata = getKey(desc, "DADeviceGUID");
  if(cfdata != NULL) {
    d->mutable_description()->set_device_guid(formatStringAsGUID(CFDataToStr((CFDataRef)cfdata)));
  }
  
  // URL
  CFTypeRef cfurl;

  cfurl = getKey(desc, "DAVolumePath");
  if(cfurl != NULL) {
    d->mutable_description()->set_volume_path(CFURLToStr((CFURLRef)cfurl));
  }

  // Special cases
  // kDADiskDescriptionMediaIcon is basically an abstraction of kIOMediaIconKey
  // from IORegistry, which is just a dictionary with two string keys
  CFTypeRef mediaIcon = getKey(desc, "DAMediaIcon");
  if(mediaIcon != NULL) {
    CFTypeID type = CFGetTypeID(mediaIcon);
    if (type != CFDictionaryGetTypeID()) {
      LOG(WARNING) << "kDADiskDescriptionMediaIconKey is no longer a dictionary. Skipping key...";
    } else {
      if(!CFDictionaryContainsKey((CFDictionaryRef)mediaIcon, CFSTR("CFBundleIdentifier")) ||
         !CFDictionaryContainsKey((CFDictionaryRef)mediaIcon, CFSTR("IOBundleResourceFile")) ) {
        LOG(WARNING) << "kDADiskDescriptionMediaIcon is missing expected keys. Skipping key...";
      } else {
        CFTypeRef bundleIdentifier = CFDictionaryGetValue((CFDictionaryRef)mediaIcon, CFSTR("CFBundleIdentifier"));
        CFTypeRef bundleResourceFile = CFDictionaryGetValue((CFDictionaryRef)mediaIcon, CFSTR("IOBundleResourceFile"));
        if(bundleIdentifier == NULL || bundleResourceFile == NULL) {
          LOG(WARNING) << "kDADiskDescriptionMediaIcon is missing expected values. Skipping key...";
        } else {
          google::protobuf::Map<std::string, std::string>& mediaIconMap = *(d->mutable_description()->mutable_media_icon());
          mediaIconMap["CFBundleIdentifier"] = CFStrToStr((CFStringRef)bundleIdentifier);
          mediaIconMap["IOBundleResourceFile"] = CFStrToStr((CFStringRef)bundleResourceFile);
        }
      }
    }
  }

  CFRelease(desc);

  std::shared_ptr<diskarbitrator::Disk> diskPtr = std::shared_ptr<diskarbitrator::Disk>(d);

  if(!d->description().media_whole()) {
    // If the disk is not "whole" (meaning the disk represents a slice and not
    // the _whole_ disk), we want to get the reference to the parent and add
    // this disk as it's children. Programmatically speaking, it could be that
    // we got the callback of a slice before we did the whole disk. Despite
    // this event being, at best, quite unlikely, we have to condition the rest
    // of the code for the fact that we may be attempting to add info about a
    // slice disk to it's "parent", and that "parent" might not be initialised.
    // The solution is to force the generation of the parent disk as this
    // stage, and if it doesn't exist add it to the disk list.
    // If we later get through the callback the notification that the parent
    // disk appeared, the callback code will not add the generated disk
    // reference, as it already exists.
    DADiskRef parentRef = DADiskCopyWholeDisk(disk);
    if(parentRef) {
      std::shared_ptr<diskarbitrator::Disk> parentDisk = genDisk(parentRef, instance);
      if(instance) {
        if(!instance->diskExists(parentDisk->disk())) {
          instance->addDisk(parentDisk);
        }
        instance->addChildToParent(diskPtr->disk(), parentDisk->disk());
      }
      diskPtr->set_parent_disk(parentDisk->disk());
      CFRelease(parentRef);
    }
  }

  return std::move(diskPtr);
}

const std::string genErrorDescription(int errCode) {
  // Disclaimer: These are descriptions made by me, not by Apple. This is to
  // the best of my understanding what explains what the error is about
  switch(errCode) {
    case ERROR_SUCCESS:
      return std::move("Success");
    case ERROR_ERROR:
      return std::move("Undetermined error");
    case ERROR_BUSY:
      return std::move("Resource is busy");
    case ERROR_BAD_ARGUMENT:
      return std::move("Bad argument");
    case ERROR_EXCLUSIVE_ACCESS:
      return std::move("Exclusive access to resource denied");
    case ERROR_NO_RESOURCES:
      return std::move("No resources available");
    case ERROR_NOT_FOUND:
      return std::move("Not found");
    case ERROR_NOT_MOUNTED:
      return std::move("Not mounted");
    case ERROR_NOT_PERMITTED:
      return std::move("Not permitted");
    case ERROR_NOT_PRIVILEGED:
      return std::move("Not enough privileges for this request");
    case ERROR_NOT_READY:
      return std::move("Resource not ready");
    case ERROR_NOT_WRITABLE:
      return std::move("Resource is not writable");
    case ERROR_UNSUPPORTED:
      return std::move("Unsupported");
  }
  return std::move("UNKNOWN ERROR CODE");
}