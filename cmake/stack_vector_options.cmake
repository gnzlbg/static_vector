# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
# CMake options

include(CMakeDependentOption)

option(STACK_VECTOR_ENABLE_ASAN "Run the unit tests and examples using AddressSanitizer." OFF)
option(STACK_VECTOR_ENABLE_COVERAGE "Run the unit tests and examples with code coverage instrumentation." OFF)
option(STACK_VECTOR_ENABLE_WERROR "Fail and stop if a warning is triggered." OFF)
option(STACK_VECTOR_ENABLE_DEBUG_INFORMATION "Includes debug information in the binaries." OFF)
option(STACK_VECTOR_ENABLE_ASSERTIONS "Enables assertions." OFF)
option(STACK_VECTOR_ENABLE_LIKELY "Enables branch-prediction hints (STACK_VECTOR_LIKELY/STACK_VECTOR_UNLIKELY macros)." ON)
