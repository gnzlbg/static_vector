# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#

include(CMakeDependentOption)

option(FCVECTOR_ENABLE_ASAN "Run the unit tests and examples using AddressSanitizer." OFF)
option(FCVECTOR_ENABLE_COVERAGE "Run the unit tests and examples with code coverage instrumentation." OFF)
option(FCVECTOR_ENABLE_WERROR "Fail and stop if a warning is triggered." OFF)
option(FCVECTOR_ENABLE_DEBUG_INFORMATION "Includes debug information in the binaries." OFF)
option(FCVECTOR_ENABLE_ASSERTIONS "Enables assertions." ON)
option(FCVECTOR_ENABLE_LIKELY "Enables branch-prediction hints (FCVECTOR_LIKELY/FCVECTOR_UNLIKELY macros)." ON)
