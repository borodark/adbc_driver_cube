# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Find FlatBuffers library
# Requires FlatBuffers >= 2.0 with CMake config support
find_package(FlatBuffers REQUIRED
  HINTS
    /usr/local/lib/cmake/flatbuffers              # Source install (CI)
    /usr/lib/x86_64-linux-gnu/cmake/flatbuffers  # Ubuntu/Debian system package
    /opt/homebrew/lib/cmake/flatbuffers           # Homebrew on Apple Silicon
)

if(FlatBuffers_FOUND)
  set(FlatBuffers_LIBRARIES flatbuffers::flatbuffers)
  message(STATUS "FlatBuffers found: ${FlatBuffers_DIR}")
else()
  message(FATAL_ERROR "FlatBuffers >= 2.0 required. Install from https://github.com/google/flatbuffers or use: brew install flatbuffers")
endif()

# Find libpq for PostgreSQL protocol support
if(WIN32)
  # XXX: for now, assume vcpkg
  find_package(PostgreSQL REQUIRED)
  set(LIBPQ_LINK_LIBRARIES PostgreSQL::PostgreSQL wsock32 ws2_32)
  set(LIBPQ_STATIC_LIBRARIES PostgreSQL::PostgreSQL)
  set(LIBPQ_INCLUDE_DIRS)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQ QUIET libpq)
  endif()

  if(NOT LIBPQ_FOUND)
    # Try to find libpq manually
    find_library(LIBPQ_LIBRARY NAMES pq)
    find_path(LIBPQ_INCLUDE_DIR NAMES libpq-fe.h)

    if(LIBPQ_LIBRARY AND LIBPQ_INCLUDE_DIR)
      set(LIBPQ_LINK_LIBRARIES ${LIBPQ_LIBRARY})
      set(LIBPQ_STATIC_LIBRARIES ${LIBPQ_LIBRARY})
      set(LIBPQ_INCLUDE_DIRS ${LIBPQ_INCLUDE_DIR})
      message(STATUS "Found libpq: ${LIBPQ_LIBRARY}")
    else()
      message(WARNING "libpq not found - building without PostgreSQL support")
      set(LIBPQ_LINK_LIBRARIES)
      set(LIBPQ_STATIC_LIBRARIES)
      set(LIBPQ_INCLUDE_DIRS)
    endif()
  endif()
endif()

# Generate FlatBuffer C++ headers from Arrow IPC schemas
set(FLATBUFFER_SCHEMAS
    ${CMAKE_CURRENT_SOURCE_DIR}/format/Schema.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/format/Message.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/format/Tensor.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/format/SparseTensor.fbs)

set(FLATBUFFER_GENERATED_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/format/generated/Schema_generated.h
    ${CMAKE_CURRENT_SOURCE_DIR}/format/generated/Message_generated.h)

add_custom_command(
    OUTPUT ${FLATBUFFER_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/format/generated
    COMMAND flatc --cpp --gen-mutable --gen-object-api
            -o ${CMAKE_CURRENT_SOURCE_DIR}/format/generated
            ${FLATBUFFER_SCHEMAS}
    DEPENDS ${FLATBUFFER_SCHEMAS}
    COMMENT "Generating FlatBuffer C++ headers"
    VERBATIM)

add_custom_target(generate_flatbuffer_headers DEPENDS ${FLATBUFFER_GENERATED_HEADERS})

add_arrow_lib(adbc_driver_cube
              SOURCES
              cube.cc
              database.cc
              connection.cc
              statement.cc
              arrow_reader.cc
              parameter_converter.cc
              cube_types.cc
              metadata.cc
              native_protocol.cc
              native_client.cc
              OUTPUTS
              ADBC_LIBRARIES
              CMAKE_PACKAGE_NAME
              AdbcDriverCube
              PKG_CONFIG_NAME
              adbc-driver-cube
              SHARED_LINK_FLAGS
              ${ADBC_LINK_FLAGS}
              SHARED_LINK_LIBS
              adbc_driver_common
              adbc_driver_framework
              ${LIBPQ_LINK_LIBRARIES}
              ${FlatBuffers_LIBRARIES}
              STATIC_LINK_LIBS
              adbc_driver_common
              adbc_driver_framework
              ${LIBPQ_STATIC_LIBRARIES}
              ${FlatBuffers_LIBRARIES})

foreach(LIB_TARGET ${ADBC_LIBRARIES})
  add_dependencies(${LIB_TARGET} generate_flatbuffer_headers)
  target_compile_definitions(${LIB_TARGET} PRIVATE ADBC_EXPORTING CUBE_DEBUG_LOGGING=0)
  target_include_directories(${LIB_TARGET} SYSTEM
                             PRIVATE ${REPOSITORY_ROOT}/c/ ${REPOSITORY_ROOT}/c/include/
                                     ${REPOSITORY_ROOT}/c/driver ${LIBPQ_INCLUDE_DIRS}
                                     ${FlatBuffers_INCLUDE_DIRS}
                                     ${CMAKE_CURRENT_SOURCE_DIR}/format/generated)

  if(NOT ADBC_DEFINE_COMMON_ENTRYPOINTS)
    target_compile_definitions(${LIB_TARGET} PRIVATE ${ADBC_TARGET_COMPILE_DEFINITIONS})
  endif()
endforeach()

if(ADBC_TEST_LINKAGE STREQUAL "shared")
  set(TEST_LINK_LIBS adbc_driver_cube_shared)
else()
  set(TEST_LINK_LIBS adbc_driver_cube_static)
endif()

if(ADBC_BUILD_TESTS)
  add_test_case(driver_cube_test
                PREFIX
                adbc
                EXTRA_LABELS
                driver-cube
                SOURCES
                cube_test.cc
                EXTRA_LINK_LIBS
                adbc_driver_common
                adbc_validation
                ${TEST_LINK_LIBS})
  target_compile_features(adbc-driver-cube-test PRIVATE cxx_std_17)
  target_include_directories(adbc-driver-cube-test SYSTEM
                             PRIVATE ${REPOSITORY_ROOT}/c/ ${REPOSITORY_ROOT}/c/include/
                                     ${REPOSITORY_ROOT}/c/driver)
  adbc_configure_target(adbc-driver-cube-test)

  add_test_case(driver_cube_types_integration_test
                PREFIX
                adbc
                EXTRA_LABELS
                driver-cube-integration
                SOURCES
                types_integration_test.cc
                EXTRA_LINK_LIBS
                adbc_driver_common
                adbc_driver_manager_shared
                nanoarrow
                ${TEST_LINK_LIBS})
  target_compile_features(adbc-driver-cube-types-integration-test PRIVATE cxx_std_17)
  target_include_directories(adbc-driver-cube-types-integration-test SYSTEM
                             PRIVATE ${REPOSITORY_ROOT}/c/ ${REPOSITORY_ROOT}/c/include/
                                     ${REPOSITORY_ROOT}/c/driver
                                     ${REPOSITORY_ROOT}/c/vendor/nanoarrow)
  adbc_configure_target(adbc-driver-cube-types-integration-test)
endif()
