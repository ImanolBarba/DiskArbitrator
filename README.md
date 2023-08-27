# DiskArbitrator
macOS Forensics utility to block or force mounts read-only

# Description
DiskArbitrator is a macOS Forensics utility that prevents volumes attached (physically or otherwise) to the system from being mounted automatically, or be forced to do so in read-only mode. This prevents accidental modification of the data contained in the volume to an extent, but is not a total guarantee of immutability, as this does not prevent writes to happen on the volumes once mounted, or the raw disk.

In short: This application only _interecepts_ mount requests, and doesn't do anything about previous mounts or raw disks.

DiskArbitrator is built on top of Apple's DiskArbitration Framework in order to intercept mounts, and instruments the `hdiutil` CLI tool for attaching disks.

# How is this project different from Aaron Burghardt's Disk Arbitrator?
_tl;dr: It's pretty much the same functionality wise_

This project is a reimplementation of https://github.com/aburgh/Disk-Arbitrator. This version aims to:
- Decouple the Disk Arbitration daemon from the UI, allowing for both/either a CLI/GUI to act as clients
- Abstract all CoreFoundation/DiskArbitration specifics from the UI, using Protobuf and gRPC for communication
- Be a more portable version, offering binary releases for Intel and Apple Silicon macOS versions
- Have better error code descriptions
- Be actively supported

# Requirements
- macOS/OSX 10.5 or later

# Usage
// TODO


