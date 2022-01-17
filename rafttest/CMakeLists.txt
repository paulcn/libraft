
gen_lib(raft rafttest)

add_dependencies(rafttest ${LIB_RAFT})

install(DIRECTORY . DESTINATION include/rafttest FILES_MATCHING PATTERN "*.h")

install(TARGETS rafttest RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

