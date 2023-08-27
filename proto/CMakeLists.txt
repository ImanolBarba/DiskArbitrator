find_package(protobuf REQUIRED CONFIG)
find_package(gRPC CONFIG REQUIRED)

# Protobuf/Grpc source files
set(PROTO_FILES diskarbitrator.proto)

# Add Library target with protobuf sources
add_library(proto ${PROTO_FILES})
target_link_libraries(proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
)
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Compile protobuf and grpc files in myproto target to cpp
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET proto LANGUAGE cpp)
protobuf_generate(TARGET proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")
