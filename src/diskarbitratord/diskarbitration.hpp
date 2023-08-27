/***************************************************************************
 *   diskarbitration.hpp  --  This file is part of diskarbitratord.        *
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

#ifndef DISKARBITRATION_HPP_
#define DISKARBITRATION_HPP_

#include <vector>

#include <CoreFoundation/CoreFoundation.h>

#include "diskarbitrator.grpc.pb.h"

// Forward declaration
class DiskAbitratorServiceImpl;

// XNU Error systems
#define	ERR_KERN       0x00 // kernel
#define	ERR_US         0x01 // userspace
#define	ERR_SERVER     0x02 // user servers
#define	ERR_IPC        0x03 // old IPC
#define ERR_MACH_IPC   0x04 // mach IPC
#define	ERR_DIPC       0x07 // distributed IPC
#define ERR_LOCAL      0x3E // user defined errors
#define	ERR_IPC_COMPAT 0x3F // mach-ipc errors 

// XNU Error subsystems
#define SUB_UNIX            0x03  // (os/unix)
#define SUB_DISKARBITRATION 0x368 // (local/diskarbitration)

// DiskArbitration error codes
// (As of 2023-08-26)
#define ERROR_SUCCESS          0x00
#define ERROR_ERROR            0x01
#define ERROR_BUSY             0x02
#define ERROR_BAD_ARGUMENT     0x03
#define ERROR_EXCLUSIVE_ACCESS 0x04
#define ERROR_NO_RESOURCES     0x05
#define ERROR_NOT_FOUND        0x06
#define ERROR_NOT_MOUNTED      0x07
#define ERROR_NOT_PERMITTED    0x08
// Fun fact about this error, when mounting the disk into a path that does not
// exist, instead of the expected ERROR_NOT_FOUND error, ERROR_NOT_PRIVILEGED
// is thrown instead. Why? I have no clue
#define ERROR_NOT_PRIVILEGED   0x09
#define ERROR_NOT_READY        0x0A
#define ERROR_NOT_WRITABLE     0x0B
#define ERROR_UNSUPPORTED      0x0C

// The CoreFoundation run loop.
void CFLoop(DiskAbitratorServiceImpl* instance, DASessionRef session);

void DiskAppearedCallback(DADiskRef diskRef, void *context);
void DiskDisappearedCallback(DADiskRef diskRef, void *context);
void DiskDescriptionChangedCallback(DADiskRef diskRef, CFArrayRef keys, void *context);
DADissenterRef __attribute__((cf_returns_retained)) DiskMountApprovalCallback(DADiskRef diskRef, void *arbitrator);

// Ejects the disk. The slices will have to be unmounted or we'll get EBUSY
void ejectDisk(DASessionRef session, diskarbitrator::Disk& disk);

// Returns path
const std::string mountDisk(DASessionRef session, diskarbitrator::Disk& disk, diskarbitrator::MountMode mode, std::vector<std::string> args, const std::string& path = "");

void unmountDisk(DASessionRef session, diskarbitrator::Disk& disk);

// Callback used for mount and eject ops to check for success or failure
void checkSuccess(DADiskRef diskRef, DADissenterRef dissenterRef, void *context);

// Fetches disk info from the BSD disk name and returns a Disk object. If 
// instance is not provided, any generated disk slices will not be added to
// their parents
std::shared_ptr<diskarbitrator::Disk> genDisk(DADiskRef& disk, DiskAbitratorServiceImpl* instance = nullptr);

// Takes a (local/diskarbitration) error code and returns a human readable description
const std::string genErrorDescription(int errCode);




#endif