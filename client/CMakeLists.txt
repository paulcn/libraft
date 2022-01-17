message("in src lib ..." )

#添加include 路径
include_directories(${SRC_DIR})

link_libraries(${THIRDPARTY_LIBS} -lz)

FILE(GLOB_RECURSE UTIL_LIB_SRCS "*.c" "*.cc" "*.cpp" )
#message("UTIL_LIB_SRCS ="  ${UTIL_LIB_SRCS})

# 生成链接库

add_library (${LIB_RAFT_CLIENT} ${UTIL_LIB_SRCS} )

set(INC_INSTALL ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_INSTALL ${CMAKE_INSTALL_PREFIX}/lib)

#INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/RaftClient.h DESTINATION ${INC_INSTALL}/)
#INSTALL (TARGETS ${LIB_RAFT_DATA_CLIENT} DESTINATION ${LIB_INSTALL})
