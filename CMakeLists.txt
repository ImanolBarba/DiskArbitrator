cmake_minimum_required(VERSION 3.13)
project(diskarbitrator LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-declarations")

add_subdirectory(proto)

find_package(glog REQUIRED)
find_package(gRPC REQUIRED)
find_package(cxxopts REQUIRED)
find_library(CoreFoundation CoreFoundation)
find_library(DiskArbitration DiskArbitration)


set(DISKARBITRATORD_SOURCES
  src/diskarbitratord/cftypes.cpp
  src/diskarbitratord/main.cpp 
  src/diskarbitratord/server.cpp
  src/diskarbitratord/hdiutil.cpp
  src/diskarbitratord/plist.cpp
  src/diskarbitratord/diskarbitration.cpp
)
set(DISKARBITRATORCTL_SOURCES
  src/diskarbitratorctl/main.cpp
  src/diskarbitratorctl/mount.cpp
  src/diskarbitratorctl/umount.cpp
  src/diskarbitratorctl/eject.cpp
  src/diskarbitratorctl/attach.cpp
  src/diskarbitratorctl/info.cpp
  src/diskarbitratorctl/list.cpp
  src/diskarbitratorctl/arbitrate.cpp
  src/diskarbitratorctl/socket.cpp
  src/diskarbitratorctl/common.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DISKARBITRATORD_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${DISKARBITRATORCTL_SOURCES})

add_executable(diskarbitratord ${DISKARBITRATORD_SOURCES})
target_link_libraries(diskarbitratord PRIVATE proto gRPC::grpc++ gRPC::grpc++_reflection glog::glog ${CoreFoundation} ${DiskArbitration})

add_executable(diskarbitratorctl ${DISKARBITRATORCTL_SOURCES})
target_link_libraries(diskarbitratorctl PRIVATE proto)
