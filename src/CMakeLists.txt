
project(libraft)

gen_lib(raft ${LIB_RAFT})

add_dependencies(${LIB_RAFT} thirdparty)

install(DIRECTORY . DESTINATION include FILES_MATCHING PATTERN "*.h")

install(TARGETS ${LIB_RAFT} RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

#INSTALL(TARGET ${THIRDPARTY_PATH}/rocksdb/ DESTINATION lib)


