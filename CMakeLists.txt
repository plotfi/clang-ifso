
project(clang-ifso)
cmake_minimum_required(VERSION 3.4)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

find_package(Clang REQUIRED CONFIG)

add_definitions(${CLANG_DEFINITIONS})
include_directories(${CLANG_INCLUDE_DIRS})

add_llvm_loadable_module(clang-ifso
                           clang-ifso.cpp
                         PLUGIN_TOOL
                           clang)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
  target_link_libraries(clang-ifso
                        PRIVATE
                          clangAST
                          clangBasic
                          clangFrontend
                          LLVMSupport)
endif()

install(TARGETS
          clang-ifso
        LIBRARY
        DESTINATION
          lib
        COMPONENT
          clang-ifso)
add_llvm_install_targets(install-clang-ifso
                         DEPENDS
                           clang-ifso
                         COMPONENT
                           clang-ifso)

