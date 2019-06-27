cmake_minimum_required(VERSION 3.5.2 FATAL_ERROR)

# Download dependencies automatically when conan is installed
find_program(CONAN NAMES conan)
if (CONAN)
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/master/conan.cmake"
                      "${CMAKE_BINARY_DIR}/conan.cmake")
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_cmake_run(CONANFILE conanfile.py
                    BASIC_SETUP CMAKE_TARGETS NO_OUTPUT_DIRS KEEP_RPATHS
                    BUILD never)
endif()
