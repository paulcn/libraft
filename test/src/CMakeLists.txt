
gen_lib(raft unittest)

add_dependencies(unittest ${LIB_RAFT} rafttest)

#target_link_libraries(UnitTestServer ${LIB_RAFT} rocksdb snappy)
