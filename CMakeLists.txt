# Copyright Gonzalo Brito Gadeschi 2015
# Copyright Louis Dionne 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

# Setup project
project(stack_vector CXX)
cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Export compilation data-base

# Build system
include(stack_vector_options)
include(stack_vector_compiler_flags)
include(stack_vector_packages)
include(stack_vector_targets)
include(stack_vector_format)
include(stack_vector_tidy)

# Include library
include_directories(${stack_vector_SOURCE_DIR}/include)
enable_testing()

# Setup subdirectories
add_subdirectory(test)
add_subdirectory(site)

# Setup the `check` target to build, check format, and then run all the tests
# and examples.
add_custom_target(check ALL
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS check-format tests test.headers
  COMMENT "Build and then run all the tests and examples.")
