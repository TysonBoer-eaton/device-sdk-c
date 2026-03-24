add_executable (device-cmdclient device-cmdclient.c)
target_include_directories (device-cmdclient PRIVATE ../../../../include ${INCLUDE_DIRS})
target_link_libraries (device-cmdclient PRIVATE csdk)
