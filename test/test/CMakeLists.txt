
include_directories(/usr/local/tars/cpp/thirdparty/include/gtest/include)
include_directories(/usr/local/thirdparty/include/gtest/)
link_directories(/usr/local/thirdparty/lib/)

gen_server(raft autotest)

add_dependencies(autotest rafttest)

target_link_libraries(autotest rafttest ${LIB_RAFT} ${LIB_TARS_SERVANT} ${LIB_TARS_UTIL} crypto ssl unittest gtest rocksdb snappy)



