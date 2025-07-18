cmake_minimum_required(VERSION 3.15)
project(libsrec VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wsign-conversion")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused -Wunreachable-code")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-align -Wcast-qual")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfloat-equal -Wshadow")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpointer-arith -Wwrite-strings")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch-default -Wswitch-enum")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4013") # Treat undefined function as error
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4700") # Treat uninitialized variable use as error
endif()

# Options
option(BUILD_EXECUTABLES "Build command line utilities" ON)
option(BUILD_TESTING "Build tests" ON)

# Add an option to enable AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
        set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_CXX_FLAGS "-g ${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} ${ASAN_FLAGS}")
    else()
        message(FATAL_ERROR "Unsupported compiler for AddressSanitizer")
    endif()
endif()

add_subdirectory(srec)

if(BUILD_EXECUTABLES)
    add_executable(bin2srec bin2srec.cpp)
    target_link_libraries(bin2srec PUBLIC srec)
    target_include_directories(bin2srec PUBLIC
        "${PROJECT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/srec"
        )

    add_executable(srec2bin srec2bin.cpp)
    target_link_libraries(srec2bin PUBLIC srec)
    target_include_directories(srec2bin PUBLIC
        "${PROJECT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/srec"
        )

    add_executable(sreccheck sreccheck.cpp)
    target_link_libraries(sreccheck PUBLIC srec)
    target_include_directories(sreccheck PUBLIC
        "${PROJECT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/srec"
        )

    # Install executables
    install(TARGETS bin2srec srec2bin sreccheck
        RUNTIME DESTINATION bin)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
    add_test(NAME TestSrec COMMAND test_srec)
endif()
