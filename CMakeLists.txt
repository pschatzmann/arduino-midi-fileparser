cmake_minimum_required(VERSION 3.0)

# set the project name
project(midi-file)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

add_library(midi-file INTERFACE)

# make include directory available to calling projects 
target_include_directories (midi-file INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# installation of all header files
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/" # source directory
    DESTINATION "include/midi-file" # target directory
    FILES_MATCHING # install only matched files
    PATTERN "*.h" # select header files
)

# desktop unit tests: cmake -B build -DMIDI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
option(MIDI_BUILD_TESTS "Build the desktop unit tests" OFF)
if(MIDI_BUILD_TESTS)
    enable_testing()
    add_executable(midi-file-tests tests/test_parser.cpp)
    target_compile_features(midi-file-tests PRIVATE cxx_std_11)
    target_link_libraries(midi-file-tests midi-file)
    add_test(NAME midi-file-tests COMMAND midi-file-tests)
endif()
