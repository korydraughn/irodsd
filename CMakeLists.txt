cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)

find_package(IRODS REQUIRED)

set(IRODS_CLIENT_REVISION "0")
set(IRODS_CLIENT_VERSION "${IRODS_VERSION}.${IRODS_CLIENT_REVISION}")

include(RequireOutOfSourceBuild)
include(IrodsCXXCompiler)

set(CMAKE_CXX_STANDARD ${IRODS_CXX_STANDARD})
set(CMAKE_CXX_EXTENSIONS OFF)

# export-dynamic so stacktrace entries from executables have function names.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-rdynamic -Wl,--export-dynamic -Wl,--enable-new-dtags -Wl,--as-needed -Wl,-z,defs")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT "-Wl,--gc-sections -Wl,-z,combreloc")

project(irods_project_template_cpp_client
  VERSION "${IRODS_CLIENT_VERSION}"
  LANGUAGES CXX)

# Include this module to inherit iRODS's defaults for RPATH/RUNPATH handling
include(IrodsRunpathDefaults)

include("${IRODS_TARGETS_PATH}")
include(GNUInstallDirs)
include(UseLibCXX)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build {Debug, Release}." FORCE)
  message(STATUS "Setting unspecified CMAKE_BUILD_TYPE to '${CMAKE_BUILD_TYPE}'. This is the correct setting for normal builds.")
endif()

find_package(nlohmann_json "3.6.1" REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
endif()

add_compile_options(-Wall -Wextra -Wconversion -Wshadow -Wpedantic)

set(IRODS_BUILD_WITH_WERROR OFF CACHE BOOL "Choose whether to compile with -Werror.")

if (IRODS_BUILD_WITH_WERROR)
  add_compile_options(-Werror)
endif()

set(IRODS_CLIENT_NAME "irods_cpp_client_example")

add_executable(${IRODS_CLIENT_NAME}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

target_compile_definitions(
  ${IRODS_CLIENT_NAME}
  PRIVATE
  ${IRODS_COMPILE_DEFINITIONS}
  ${IRODS_COMPILE_DEFINITIONS_PRIVATE})

target_include_directories(
  ${IRODS_CLIENT_NAME}
  PRIVATE
  ${IRODS_INCLUDE_DIRS}
  "${IRODS_EXTERNALS_FULLPATH_BOOST}/include"
  "${IRODS_EXTERNALS_FULLPATH_FMT}/include")

target_link_libraries(
  ${IRODS_CLIENT_NAME}
  PRIVATE
  irods_client
  "${IRODS_EXTERNALS_FULLPATH_FMT}/lib/libfmt.so")
