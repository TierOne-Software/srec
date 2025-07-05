add_library(srec srec.cpp)

# Set properties for the library
set_target_properties(srec PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "srec.h;crc32.h;srec_exceptions.h"
)

# Include directories
target_include_directories(srec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Install library and headers
install(TARGETS srec
    EXPORT srecTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

# Install export targets
install(EXPORT srecTargets
    FILE srecTargets.cmake
    NAMESPACE srec::
    DESTINATION lib/cmake/srec
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    srecConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/srecConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/srecConfig.cmake"
    INSTALL_DESTINATION lib/cmake/srec
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/srecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/srecConfigVersion.cmake"
    DESTINATION lib/cmake/srec
)
