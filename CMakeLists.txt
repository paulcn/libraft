cmake_minimum_required(VERSION 3.15)

project(Raft)

set(MAJOR_VERSION 1)
set(MINOR_VERSION 0)
set(PATCH_VERSION 0)
set(RAFT_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

set(CMAKE_INSTALL_PREFIX "/usr/local/tars/raft_${RAFT_VERSION}" CACHE STRING "set install path" FORCE)

include(includes.cmake)

option(TARS_MYSQL "option for mysql" ON)
option(TARS_SSL "option for ssl" OFF)
option(TARS_HTTP2 "option for http2" OFF)

if(WIN32)
    include (c:\\tars\\cpp\\makefile\\tars-tools.cmake)
else()
    include (/usr/local/tars/cpp/makefile/tars-tools.cmake)
endif()

include_directories(/usr/local/tars/cpp/include/servant)
include_directories(/usr/local/tars/cpp/thirdparty/include)
link_directories(/usr/local/tars/cpp/thirdparty/lib)
link_directories(/usr/local/tars/cpp/thirdparty/lib64)

# #编译选项 Release 还是 Debug
# set(CMAKE_BUILD_TYPE "Release" CACHE STRING "set build type to release default")
# set(Ext "")
# IF (CMAKE_BUILD_TYPE STREQUAL "" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
#     set(CMAKE_BUILD_TYPE "Debug")
#     set(Ext ".debug")
# ENDIF()
# message("CMAKE_BUILD_TYPE TYPE = " ${CMAKE_BUILD_TYPE})
# message("Ext = " ${Ext})

set(CMAKE_CXX_FLAGS_DEBUG "-Wall -g")

set(CLEAR_INCLUDE "clear-install.cmake")
FILE(WRITE ${CLEAR_INCLUDE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/include)\n")

install(SCRIPT ${CLEAR_INCLUDE})

include_directories(src)

set(LIB_RAFT "raft")
set(LIB_RAFT_CLIENT "raftclient")
set(LIB_RAFT_TEST "rafttest")

message("LIB_RAFT = " ${LIB_RAFT}, "LIB_RAFT_TEST = " ${LIB_RAFT_TEST})

add_subdirectory(src)
add_subdirectory(rafttest)
add_subdirectory(client)

include_directories(rafttest)
add_subdirectory(test)

IF (UNIX)
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}  /usr/local/tars/raft)")
ENDIF ()