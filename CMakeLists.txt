cmake_minimum_required(VERSION 3.10)
project(dht_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(ExternalProject)

# External kademlia project
ExternalProject_Add(kademlia_ext
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kademlia
  CMAKE_ARGS 
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/kademlia_install
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  BINARY_DIR ${CMAKE_BINARY_DIR}/kademlia_build
  BUILD_ALWAYS FALSE
  INSTALL_COMMAND ""
)

ExternalProject_Get_Property(kademlia_ext source_dir binary_dir)

# -------------------------------------------------------------------
# Create imported library target (choose SHARED by default)
# -------------------------------------------------------------------
add_library(kademlia SHARED IMPORTED)

# include directories
set_target_properties(kademlia PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${source_dir}/include"
)

# Paths to built libraries inside ExternalProject
set(KADEMLIA_SO "${binary_dir}/src/kademlia/libkademlia.so")
set(KADEMLIA_A  "${binary_dir}/src/kademlia/libkademlia_static.a")

# Assign the correct file path
if(EXISTS "${KADEMLIA_SO}")
  set_target_properties(kademlia PROPERTIES
    IMPORTED_LOCATION "${KADEMLIA_SO}"
  )
else()
  set_target_properties(kademlia PROPERTIES
    IMPORTED_LOCATION "${KADEMLIA_A}"
  )
endif()

# Make sure the external project builds before we link against it
add_dependencies(kademlia kademlia_ext)

# -------------------------------------------------------------------
# Executable target
# -------------------------------------------------------------------
add_executable(dht-client dht-client.cpp)

target_include_directories(dht-client PRIVATE
    ${source_dir}/include
)

target_link_libraries(dht-client PRIVATE
    kademlia
    pthread
)

# Optionally add these if kademlia requires Boost or OpenSSL:
# target_link_libraries(dht-client PRIVATE kademlia pthread boost_system ssl crypto)
