# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

set(XDNA_TARGET xrt_driver_xdna)
set(XRT_CORE_TARGET xrt_core)
set(XRT_COREUTIL_TARGET xrt_coreutil)

set_target_properties(${XRT_COREUTIL_TARGET} PROPERTIES OUTPUT_NAME "shim_${XRT_COREUTIL_TARGET}")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} MAIN_SOURCES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/kmq KMQ_SOURCES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/umq UMQ_SOURCES)
add_library(${XDNA_TARGET} SHARED
  ${MAIN_SOURCES}
  ${KMQ_SOURCES}
  ${UMQ_SOURCES}
  )

file(GLOB _headers "${CMAKE_CURRENT_LIST_DIR}/*.h")
target_sources(${XDNA_TARGET} PUBLIC
  FILE_SET HEADERS
  FILES ${_headers})

set_target_properties(${XDNA_TARGET} PROPERTIES
  VERSION ${XRT_PLUGIN_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION}
  )

target_compile_definitions(${XDNA_TARGET} PRIVATE
  # below macros is required so that i/f defined in ishim.h is
  # consistent with native xrt implementation
  XRT_ENABLE_AIE
  XRT_AIE_BUILD
  )

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  add_definitions(-DXDNA_SHIM_DEBUG)
endif()

if(${UMQ_HELLO_TEST})
  add_definitions(-DUMQ_HELLO_TEST)
endif()

target_compile_options(${XDNA_TARGET} PRIVATE
  "-fPIC"
  )

target_include_directories(${XDNA_TARGET} PRIVATE
  ${XRT_SOURCE_DIR}/src/runtime_src
  ${XRT_SOURCE_DIR}/src/runtime_src/core/include
  ${XRT_SOURCE_DIR}/src/runtime_src/core/common/gsl/include
  ${XRT_BINARY_DIR}/src/gen
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/uapi
  )

# driver plugin will be loaded while xrt_core is still being
# dlopen'ed, symbols against libxrt_core.so can't be solved
# at that time, so need to be fully solved here.
target_link_libraries(${XDNA_TARGET} PUBLIC
  ${XRT_COREUTIL_TARGET}
  uuid
  )

target_link_options(${XDNA_TARGET} PRIVATE
  "-Wl,-z,defs"
  )

install(
  TARGETS ${XDNA_TARGET} ${XRT_COREUTIL_TARGET}
  EXPORT ShimXDNATargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/shim_xdna
)

install(EXPORT ShimXDNATargets
  FILE ShimXDNATargets.cmake
  DESTINATION lib/cmake/ShimXDNA
  NAMESPACE ShimXDNA::
)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfig.cmake"
  INSTALL_DESTINATION "lib/cmake/ShimXDNA"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfigVersion.cmake"
  VERSION "${XRT_PLUGIN_VERSION_STRING}"
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfigVersion.cmake
  DESTINATION lib/cmake/ShimXDNA
)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfig.cmake"
  INSTALL_DESTINATION "lib/cmake/ShimXDNA"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfigVersion.cmake"
  VERSION "${XRT_PLUGIN_VERSION_STRING}"
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ShimXDNAConfigVersion.cmake
  DESTINATION lib/cmake/ShimXDNA
)