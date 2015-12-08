# Copyright Louis Dionne 2015
# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
# Setup compiler flags (more can be set on a per-target basis or in
# subdirectories)

# Compiler flags:
include(CheckCXXCompilerFlag)
macro(stack_vector_append_flag testname flag)
    check_cxx_compiler_flag(${flag} ${testname})
    if (${testname})
        add_compile_options(${flag})
    endif()
endmacro()

# Language flag: version of the C++ standard to use
stack_vector_append_flag(STACK_VECTOR_HAS_STDCXX14 -std=c++14)

# PITA warning flags:
stack_vector_append_flag(STACK_VECTOR_HAS_WSHADOW -Wshadow)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNUSED -Wunused)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNUSED_FUNCTION -Wunused-function)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNUSED_LABEL -Wunused-label)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNUSED_PARAMETER -Wunused-parameter)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNUSED_VALUE -Wunused-value)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNUSED_VARIABLE -Wunused-variable)

# Warning flags:
stack_vector_append_flag(STACK_VECTOR_HAS_WALL -Wall)
stack_vector_append_flag(STACK_VECTOR_HAS_WEXTRA -Wextra)
stack_vector_append_flag(STACK_VECTOR_HAS_WDEPRECATED -Wdeprecated)
stack_vector_append_flag(STACK_VECTOR_HAS_WDOCUMENTATION -Wdocumentation)
stack_vector_append_flag(STACK_VECTOR_HAS_WCOMMENT -Wcomment)
stack_vector_append_flag(STACK_VECTOR_HAS_PEDANTIC -Wpedantic)
stack_vector_append_flag(STACK_VECTOR_HAS_WSTACK_PROTECTOR -Wstack-protector)
stack_vector_append_flag(STACK_VECTOR_HAS_WSTRICT_ALIASING "-Wstrict-aliasing=2")
stack_vector_append_flag(STACK_VECTOR_HAS_WSTRICT_OVERFLOW "-Wstrict-overflow=5")
stack_vector_append_flag(STACK_VECTOR_HAS_WDISABLED_OPTIMIZATION -Wdisabled-optimization)
stack_vector_append_flag(STACK_VECTOR_HAS_WINLINE -Winline)
stack_vector_append_flag(STACK_VECTOR_HAS_WRETURN_TYPE -Wreturn-type)
stack_vector_append_flag(STACK_VECTOR_HAS_WCAST_ALIGN -Wcast-align)
stack_vector_append_flag(STACK_VECTOR_HAS_WCAST_QUAL -Wcast-qual)
stack_vector_append_flag(STACK_VECTOR_HAS_WSIGN_COMPARE -Wsign-compare)
stack_vector_append_flag(STACK_VECTOR_HAS_WSIGN_PROMO -Wsign-promo)
stack_vector_append_flag(STACK_VECTOR_HAS_WFORMAT "-Wformat=2")
stack_vector_append_flag(STACK_VECTOR_HAS_WFORMAT_NONLITERAL -Wformat-nonliteral)
stack_vector_append_flag(STACK_VECTOR_HAS_WFORMAT_SECURITY -Wformat-security)
stack_vector_append_flag(STACK_VECTOR_HAS_WFORMAT_Y2K -Wformat-y2k)
stack_vector_append_flag(STACK_VECTOR_HAS_WMISSING_BRACES -Wmissing-braces)
stack_vector_append_flag(STACK_VECTOR_HAS_WMISSING_FIELD_INITIALIZERS -Wmissing-field-initializers)
#stack_vector_append_flag(STACK_VECTOR_HAS_WMISSING_INCLUDE_DIRS -Wmissing-include-dirs)
stack_vector_append_flag(STACK_VECTOR_HAS_WOVERLOADED_VIRTUAL -Woverloaded-virtual)
stack_vector_append_flag(STACK_VECTOR_HAS_WCHAR_SUBSCRIPTS -Wchar-subscripts)
stack_vector_append_flag(STACK_VECTOR_HAS_WFLOAT_EQUAL -Wfloat-equal)
stack_vector_append_flag(STACK_VECTOR_HAS_WPOINTER_ARITH -Wpointer-arith)
stack_vector_append_flag(STACK_VECTOR_HAS_WINVALID_PCH -Winvalid-pch)
stack_vector_append_flag(STACK_VECTOR_HAS_WIMPORT -Wimport)
stack_vector_append_flag(STACK_VECTOR_HAS_WINIT_SELF -Winit-self)
stack_vector_append_flag(STACK_VECTOR_HAS_WREDUNDANT_DECLS -Wredundant-decls)
stack_vector_append_flag(STACK_VECTOR_HAS_WPACKED -Wpacked)
stack_vector_append_flag(STACK_VECTOR_HAS_WPARENTHESES -Wparentheses)
stack_vector_append_flag(STACK_VECTOR_HAS_WSEQUENCE_POINT -Wsequence-point)
stack_vector_append_flag(STACK_VECTOR_HAS_WSWITCH -Wswitch)
stack_vector_append_flag(STACK_VECTOR_HAS_WSWITCH_DEFAULT -Wswitch-default)
stack_vector_append_flag(STACK_VECTOR_HAS_WTRIGRAPHS -Wtrigraphs)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNINITIALIZED -Wuninitialized)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNKNOWN_PRAGMAS -Wunknown-pragmas)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNREACHABLE_CODE -Wunreachable-code)
stack_vector_append_flag(STACK_VECTOR_HAS_WVARIADIC_MACROS -Wvariadic-macros)
stack_vector_append_flag(STACK_VECTOR_HAS_WVOLATILE_REGISTER_VAR -Wvolatile-register-var)
stack_vector_append_flag(STACK_VECTOR_HAS_WWRITE_STRINGS -Wwrite-strings)
stack_vector_append_flag(STACK_VECTOR_HAS_WNO_ATTRIBUTES -Wno-attributes)
stack_vector_append_flag(STACK_VECTOR_HAS_WUNNEEDED_INTERNAL_DECLARATION -Wunneeded-internal-declaration)

# Template diagnostic flags
stack_vector_append_flag(STACK_VECTOR_HAS_FDIAGNOSTIC_SHOW_TEMPLATE_TREE -fdiagnostics-show-template-tree)
stack_vector_append_flag(STACK_VECTOR_HAS_FTEMPLATE_BACKTRACE_LIMIT "-ftemplate-backtrace-limit=0")
stack_vector_append_flag(STACK_VECTOR_HAS___EXTERN_ALWAYS_INLINE -D__extern_always_inline=inline)

# Compiler flags controlled by CMake options above
if (STACK_VECTOR_ENABLE_WERROR)
  stack_vector_append_flag(STACK_VECTOR_HAS_WERROR -Werror)
  stack_vector_append_flag(STACK_VECTOR_HAS_WX -WX)
endif()

if (STACK_VECTOR_ENABLE_ASAN)
  set(STACK_VECTOR_ASAN_FLAGS "-fsanitize=address,integer,undefined -fno-sanitize-recover=address,integer,undefined -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/sanitize.blacklist")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STACK_VECTOR_ASAN_FLAGS}")
  stack_vector_append_flag(STACK_VECTOR_HAS_NO_OMIT_FRAME_POINTER -fno-omit-frame-pointer)
else()
  stack_vector_append_flag(STACK_VECTOR_HAS_OMIT_FRAME_POINTER -fomit-frame-pointer)
endif()

if (STACK_VECTOR_ENABLE_DEBUG_INFORMATION)
  stack_vector_append_flag(STACK_VECTOR_HAS_G3 -g3)
else()
  stack_vector_append_flag(STACK_VECTOR_HAS_G0 -g0)
endif()

if (NOT STACK_VECTOR_ENABLE_ASSERTIONS)
  stack_vector_append_flag(STACK_VECTOR_HAS_DSTACK_VECTOR_DISABLE_ASSERTIONS -DSTACK_VECTOR_DISABLE_ASSERTIONS)
endif()

if (STACK_VECTOR_ENABLE_COVERAGE)
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "code coverage instrumentation requires CMAKE_BUILD_TYPE=Debug")
  endif()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

# Optimization flags: debug
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  stack_vector_append_flag(STACK_VECTOR_HAS_O0 -O0)
  stack_vector_append_flag(STACK_VECTOR_HAS_NO_INLINE -fno-inline)
  stack_vector_append_flag(STACK_VECTOR_HAS_STACK_PROTECTOR_ALL -fstack-protector-all)
endif()

# Optimization flags: release
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  stack_vector_append_flag(STACK_VECTOR_HAS_OFAST -Ofast)
  stack_vector_append_flag(STACK_VECTOR_HAS_UNDEBUG -UNDEBUG)
  stack_vector_append_flag(STACK_VECTOR_HAS_MARCH_NATIVE "-march=native")
  stack_vector_append_flag(STACK_VECTOR_HAS_MTUNE_NATIVE "-mtune=native")
  stack_vector_append_flag(STACK_VECTOR_HAS_STRICT_ALIASING -fstrict-aliasing)
  stack_vector_append_flag(STACK_VECTOR_HAS_VECTORIZE -fvectorize)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -polly -mllvm -polly-vectorizer=stripmine")

  # If ASan and profile not enabled: omit frame pointer
  if (NOT STACK_VECTOR_ENABLE_ASAN AND NOT STACK_VECTOR_ENABLE_PROFILE)
    stack_vector_append_flag(STACK_VECTOR_HAS_OMIT_FRAME_POINTER -fomit-frame-pointer)
  endif()
endif()

if (NOT STACK_VECTOR_ENABLE_LIKELY)
  stack_vector_append_flag(STACK_VECTOR_HAS_DISABLE_LIKELY -DSTACK_VECTOR_DISABLE_LIKELY_MACROS)
endif()

