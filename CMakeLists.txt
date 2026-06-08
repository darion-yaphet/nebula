# Copyright (c) 2018 vesoft inc. All rights reserved.
#
# This source code is licensed under Apache 2.0 License.

# The build can be controlled by defining following variables on the
# <cmake> command line
#
#   CMAKE_C_COMPILER                -- Specify the compiler for C language
#   CMAKE_CXX_COMPILER              -- Specify the compiler for C++ language
#
#   NEBULA_THIRDPARTY_ROOT          -- Specify the root directory for third-party
#   NEBULA_OTHER_ROOT               -- Specify the root directory for user build
#                                   -- Split with ":", exp: DIR:DIR
#
#   ENABLE_JEMALLOC                -- Link jemalloc into all executables
#   ENABLE_NATIVE                  -- Build native client
#   ENABLE_TESTING                 -- Build unit test
#   ENABLE_PACK_ONE                -- Package to one or multi packages

# CMake version check
cmake_minimum_required(VERSION 3.16...3.28)

# Set the project name
project("Nebula Graph" C CXX)

option(ENABLE_PACK_ONE "Whether to package into one" ON)
option(ENABLE_VERBOSE_BISON "Enable Bison to report state" OFF)
option(ENABLE_CONSOLE_COMPILATION "Enable nebula-console compilation" OFF)
option(ENABLE_PACKAGE_TAR "Enable package artifacts to tar." OFF)
option(ENABLE_CREATE_GIT_HOOKS "Enable create git hooks." ON)
option(ENABLE_INCLUDE_WHAT_YOU_USE "Enable include-what-you-use find  nouse include files" OFF)

add_compile_definitions(NEBULA_HOME="${CMAKE_SOURCE_DIR}")

if(ENABLE_STANDALONE_VERSION)
    add_compile_definitions(BUILD_STANDALONE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/nebula)

include(PlatformCheck)
include(NebulaCMakeMacros)
include(GeneralCMakeOptions)
include(GeneralCMakeConfig)
include(GeneralCompilerConfig)
include(LinkerConfig)
include(CompilerLauncher)
include(ThirdPartyConfig)
include(SanitizerConfig)
include(GitHooksConfig)
include(GitInfoConfig)
include(NebulaCustomTargets)
include(IncludeWhatYouUse)
include(TimeTrace)

find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(
        clang-format
        COMMAND bash -c "find src/ -type f \\( -iname '*.h' -o -iname '*.cpp' \\) | xargs ${CLANG_FORMAT_EXECUTABLE} -i"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format"
        VERBATIM
    )
endif()

if (ENABLE_NATIVE)
    message(STATUS "ENABLE_NATIVE is ${ENABLE_NATIVE}")
    add_compile_options(-fPIC)
endif()

if(ENABLE_MEMORY_TRACKER)
    if(ENABLE_JEMALLOC)
        if(NOT ENABLE_ASAN)
            add_definitions(-DENABLE_MEMORY_TRACKER)
            message(STATUS "MemoryTracker is ENABLED")
        else()
            message(FATAL_ERROR "MemoryTracker need -DENABLE_ASAN=off")
        endif()
    else()
        message(FATAL_ERROR "MemoryTracker need -DENABLE_JEMALLOC=on")
    endif()
else()
    message(STATUS "MemoryTracker is DISABLED")
endif()

# For simplicity, we make all ordinary libraries depend on the compile-time generated files,
# including the precompiled header, a.k.a Base.h.gch, and thrift headers.
function(nebula_add_library name type)
    add_library(${name} ${type} ${ARGN})
    if(PCHSupport_FOUND)
        add_dependencies(
            ${name}
            base_obj_gch
        )
    endif()
    add_dependencies(
        ${name}
        common_thrift_generator
        graph_thrift_generator
        storage_thrift_generator
        meta_thrift_generator
        raftex_thrift_generator
        # hbase_thrift_generator
        parser_target
        wkt_parser_target
        datetime_parser_target
    )
endfunction()

nebula_add_subdirectory(src)
nebula_add_subdirectory(conf)
nebula_add_subdirectory(resources)
nebula_add_subdirectory(scripts)

include(CPackage)
package(
  PACK_ONE    ${ENABLE_PACK_ONE}
  NAME        "nebula-graph"
  HOME_PAGE   "https://github.com/vesoft-inc/nebula/releases"
  SCRIPTS_DIR ${CMAKE_SOURCE_DIR}/package
)
