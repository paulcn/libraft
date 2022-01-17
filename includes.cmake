
include(ExternalProject)

add_custom_target(thirdparty)

set(THIRDPARTY_PATH "${CMAKE_BINARY_DIR}/src")

###################################################################

set(LIB_GFLAG "gflag")

ExternalProject_Add(ADD_${LIB_GFLAG}
        URL http://cdn.tarsyun.com/src/gflag-2.2.2.tar.gz
        DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download
        PREFIX ${CMAKE_BINARY_DIR}
        INSTALL_DIR ${CMAKE_SOURCE_DIR}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} . -DCMAKE_INSTALL_PREFIX=${THIRDPARTY_PATH}/gflag
        SOURCE_DIR ${THIRDPARTY_PATH}/gflag-lib
        BUILD_IN_SOURCE 1
        BUILD_COMMAND make
        URL_MD5 1a865b93bacfa963201af3f75b7bd64c
        )

#INSTALL(DIRECTORY ${THIRDPARTY_PATH}/gflag/ DESTINATION thirdparty)

set(GFLAG_DIR "${THIRDPARTY_PATH}/gflag")
set(GFLAG_DIR_INC "${THIRDPARTY_PATH}/gflag/include/")
set(GFLAG_DIR_LIB "${THIRDPARTY_PATH}/gflag/lib")
include_directories(${GFLAG_DIR_INC})
link_directories(${GFLAG_DIR_LIB})


###################################################################

set(LIB_SNAPPY "snappy")

ExternalProject_Add(ADD_${LIB_SNAPPY}
        URL http://tars-thirdpart-1300910346.cos.ap-guangzhou.myqcloud.com/src/snappy-1.1.8.tar.gz
        DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download
        PREFIX ${CMAKE_BINARY_DIR}
        INSTALL_DIR ${CMAKE_SOURCE_DIR}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} . -DCMAKE_INSTALL_PREFIX=${THIRDPARTY_PATH}/snappy
        SOURCE_DIR ${THIRDPARTY_PATH}/snappy-lib
        BUILD_IN_SOURCE 1
        BUILD_COMMAND make
        URL_MD5 70e48cba7fecf289153d009791c9977f
        )

INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/src/snappy/ DESTINATION thirdparty)

#INSTALL(DIRECTORY ${THIRDPARTY_PATH}/gflag/ DESTINATION thirdparty)

set(SNAPPY_DIR "${THIRDPARTY_PATH}/snappy")
set(SNAPPY_DIR_INC "${THIRDPARTY_PATH}/snappy/include/")
set(SNAPPY_DIR_LIB "${THIRDPARTY_PATH}/snappy/lib")
set(SNAPPY_DIR_LIB_64 "${THIRDPARTY_PATH}/snappy/lib64")
include_directories(${SNAPPY_DIR_INC})
link_directories(${SNAPPY_DIR_LIB})
link_directories(${SNAPPY_DIR_LIB_64})

###################################################################


set(LIB_ROCKSDB "rocksdb")

#需要修改rocksdb的CMakeLists.txt, 去掉-Wall
ExternalProject_Add(ADD_${LIB_ROCKSDB}
        URL http://cdn.tarsyun.com/src/rocksdb-6.15.5.tar.gz
        DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download
        PREFIX ${CMAKE_BINARY_DIR}
        INSTALL_DIR ${CMAKE_SOURCE_DIR}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} . -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-Wno-error -DCMAKE_INSTALL_PREFIX=${THIRDPARTY_PATH}/rocksdb -DCMAKE_CXX_FLAGS=-Wno-error -DPORTABLE=ON -DSnappy_DIR=${SNAPPY_DIR_LIB}/cmake/Snappy -DWITH_BENCHMARK_TOOLS=OFF -DWITH_TESTS=OFF -DWITH_TOOLS:BOOL=OFF -DWITH_SNAPPY=ON -DROCKSDB_BUILD_SHARED=OFF -Dgflags_DIR=${GFLAG_DIR}
        SOURCE_DIR ${THIRDPARTY_PATH}/rocksdb-lib
        BUILD_IN_SOURCE 1
        BUILD_COMMAND cd ${THIRDPARTY_PATH}/rocksdb-lib && make rocksdb -j4
        URL_MD5 9bd64f1b7b74342ba4c045e9a6dd2bd2
        )

INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/src/rocksdb/ DESTINATION thirdparty)

add_dependencies(ADD_${LIB_ROCKSDB} ADD_${LIB_GFLAG} ADD_${LIB_SNAPPY})

add_dependencies(thirdparty ADD_${LIB_ROCKSDB})

set(ROCKSDB_DIR "${THIRDPARTY_PATH}/rocksdb")
include_directories(${ROCKSDB_DIR}/include/)
link_directories(${ROCKSDB_DIR}/lib)
link_directories(${ROCKSDB_DIR}/lib)
link_directories(${ROCKSDB_DIR}/lib64)

add_dependencies(thirdparty ADD_${LIB_ROCKSDB})

###################################################################
