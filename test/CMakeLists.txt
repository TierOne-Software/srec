Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.6.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(test_srec
  test.cpp
)
target_link_libraries(test_srec PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test_srec PUBLIC srec)
target_include_directories(test_srec PUBLIC
	${PROJECT_BINARY_DIR}
	${PROJECT_SOURCE_DIR}/srec
	${PROJECT_SOURCE_DIR}
)

# Register the test with CTest
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# Only discover tests if not cross-compiling (since cross-compiled binaries can't run on host)
if(NOT CMAKE_CROSSCOMPILING)
    catch_discover_tests(test_srec)
else()
    # For cross-compilation, just add the test manually without discovery
    add_test(NAME test_srec COMMAND test_srec)
endif()

# Add roundtrip test - only on Unix-like systems
if(UNIX)
    add_custom_target(roundtrip_test
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/roundtrip_test.sh
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running roundtrip integration tests"
        DEPENDS bin2srec srec2bin sreccheck
    )

    # Add roundtrip test to CTest
    add_test(NAME roundtrip_test 
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/roundtrip_test.sh
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Custom test target that runs both unit tests and roundtrip tests
add_custom_target(test_all
    COMMAND ${CMAKE_COMMAND} --build . --target test
    COMMAND ${CMAKE_COMMAND} --build . --target roundtrip_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests (unit tests + roundtrip tests)"
)
