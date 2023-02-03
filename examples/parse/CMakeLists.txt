cmake_minimum_required(VERSION 3.20)

# set the project name
project(midi-parse)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

# Build with library
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/midi-file )
endif()

# build sketch as executable
SET_SOURCE_FILES_PROPERTIES(parse.ino PROPERTIES LANGUAGE CXX)
add_executable (midi-parse parse.ino ../main.cpp)

# specify libraries
target_link_libraries(midi-parse midi-file)
