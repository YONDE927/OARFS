cmake_minimum_required(VERSION 3.18.0)

project(ofs C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -ggdb")

set(common-srcs "common.cc")
set(client-srcs "connection.cc" "ftpclient.cc")
set(server-srcs "connection.cc" "ftpserver.cc")
set(delaytask-srcs "delaytask.cc")
set(fs-srcs "filesystem.cc" "fuse-system.cc") 
set(rfs-srcs "rfs.cc" "metacache.cc") 
set(drfs-srcs "drfs.cc" "delaytask.cc") 
set(mfs-srcs "mfs.cc" "recorder.cc") 

add_library(client ${client-srcs})

add_executable(server ${server-srcs} ${common-srcs})
target_compile_definitions(server PRIVATE SERVER)

add_executable(delaytask ${delaytask-srcs})
target_compile_definitions(delaytask PRIVATE TEST_DT)

add_executable(recorder "recorder.cc" "common.cc")
target_compile_definitions(recorder PRIVATE RECORDER)
target_link_libraries(recorder sqlite3)

add_executable(passthrough ${fs-srcs} ${common-srcs})
target_compile_definitions(passthrough PRIVATE PASSTHROUGH)
target_link_libraries(passthrough fuse3)

add_executable(rfs ${rfs-srcs} ${fs-srcs} ${common-srcs})
target_compile_definitions(rfs PRIVATE RFS)
target_link_libraries(rfs client fuse3 sqlite3)

add_executable(drfs ${drfs-srcs} ${rfs-srcs} ${fs-srcs} ${common-srcs})
target_compile_definitions(drfs PRIVATE DRFS)
target_link_libraries(drfs client fuse3 sqlite3)

add_executable(mfs ${mfs-srcs} ${drfs-srcs} ${rfs-srcs} ${fs-srcs} ${common-srcs})
target_compile_definitions(mfs PRIVATE MFS)
target_link_libraries(mfs client fuse3 sqlite3)
