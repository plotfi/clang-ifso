project(lsif)
cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF) # -std=c++14 (otherwise -std=gnu++14)

add_executable(lsif lsif.cpp)
target_compile_options(lsif PRIVATE
  -Wall -Wextra -pedantic
  -Wno-unused-parameter)

find_package(Clang CONFIG REQUIRED)
target_include_directories(lsif SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(lsif PRIVATE ${LLVM_DEFINITIONS})
if (NOT LLVM_ENABLE_RTTI)
  target_compile_options(lsif PRIVATE -fno-rtti -fno-exceptions)
endif ()
target_link_libraries(lsif PRIVATE clangTooling clangASTMatchers clangIndex)

install(TARGETS lsif RUNTIME DESTINATION bin)
