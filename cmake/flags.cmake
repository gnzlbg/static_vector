# Copyright Louis Dionne 2015
# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
# Setup compiler flags.

include(CheckCXXCompilerFlag)

# Macro used to append only those flags supported by the compiler:
macro(fcvector_append_flag testname flag)
    check_cxx_compiler_flag(${flag} ${testname})
    if (${testname})
        add_compile_options(${flag})
    endif()
endmacro()

# Language flag: version of the C++ standard to use
fcvector_append_flag(FCVECTOR_HAS_STDCXX1Z -std=c++1z)

# Enable all warnings and make them errors:
fcvector_append_flag(FCVECTOR_HAS_WERROR -Werror)
fcvector_append_flag(FCVECTOR_HAS_WX -WX)
fcvector_append_flag(FCVECTOR_HAS_WALL -Wall)
fcvector_append_flag(FCVECTOR_HAS_WEXTRA -Wextra)
fcvector_append_flag(FCVECTOR_HAS_WEVERYTHING -Weverything)
fcvector_append_flag(FCVECTOR_HAS_PEDANTIC -pedantic)
fcvector_append_flag(FCVECTOR_HAS_PEDANTIC_ERRORS -pedantic-errors)

# Selectively disable warnings with too many falses:
fcvector_append_flag(FCVECTOR_HAS_WNO_CXX98_COMPAT -Wno-c++98-compat)
fcvector_append_flag(FCVECTOR_HAS_WNO_CXX98_COMPAT_PEDANTIC -Wno-c++98-compat-pedantic)
fcvector_append_flag(FCVECTOR_HAS_WNO_PADDED -Wno-padded)
fcvector_append_flag(FCVECTOR_HAS_WNO_WEAK_VTABLES -Wno-weak-vtables)

if (FCVECTOR_ENV_MACOSX)
  fcvector_append_flag(FCVECTOR_HAS_WNO_GLOBAL_CONSTRUCTORS -Wno-global-constructors)
  fcvector_append_flag(FCVECTOR_HAS_WNO_EXIT_TIME_DESTRUCTORS -Wno-exit-time-destructors)
endif()

if (FCVECTOR_CXX_COMPILER_GCC)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "6.0")
    fcvector_append_flag(FCVECTOR_HAS_WNO_STRICT_OVERFLOW -Wno-strict-overflow)
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
      fcvector_append_flag(FCVECTOR_HAS_WNO_MISSING_FIELD_INITIALIZERS -Wno-missing-field-initializers)
    endif()
  endif()
endif()

if (FCVECTOR_ENV_LINUX AND FCVECTOR_CXX_COMPILER_CLANG)
  # On linux libc++ re-exports the system math headers. The ones from libstdc++
  # use the GCC __extern_always_inline intrinsic which is not supported by clang
  # versions 3.6, 3.7, 3.8, 3.9, 4.0, and current trunk 5.0 (as of 2017.04.13).
  #
  # This works around it by replacing __extern_always_inline with inline using a
  # macro:
  fcvector_append_flag(FCVECTOR_HAS_D__EXTERN_ALWAYS_INLINE -D__extern_always_inline=inline)
endif()

# Template diagnostic flags
fcvector_append_flag(FCVECTOR_HAS_FDIAGNOSTIC_SHOW_TEMPLATE_TREE -fdiagnostics-show-template-tree)
fcvector_append_flag(FCVECTOR_HAS_FTEMPLATE_BACKTRACE_LIMIT "-ftemplate-backtrace-limit=0")

# Clang modules support
if (FCVECTOR_MODULES)
  fcvector_append_flag(FCVECTOR_HAS_MODULES -fmodules)
  fcvector_append_flag(FCVECTOR_HAS_MODULE_MAP_FILE "-fmodule-map-file=${PROJECT_SOURCE_DIR}/include/module.modulemap")
  fcvector_append_flag(FCVECTOR_HAS_MODULE_CACHE_PATH "-fmodules-cache-path=${PROJECT_BINARY_DIR}/module.cache")
  if (FCVECTOR_LIBCXX_MODULE)
    fcvector_append_flag(FCVECTOR_HAS_LIBCXX_MODULE_MAP_FILE "-fmodule-map-file=${FCVECTOR_LIBCXX_MODULE}")
  endif()
  if (FCVECTOR_ENV_MACOSX)
    fcvector_append_flag(FCVECTOR_HAS_NO_IMPLICIT_MODULE_MAPS -fno-implicit-module-maps)
  endif()
  if (FCVECTOR_DEBUG_BUILD)
    fcvector_append_flag(FCVECTOR_HAS_GMODULES -gmodules)
  endif()
endif()

# Sanitizer support: detect incompatible sanitizer combinations
if (FCVECTOR_ASAN AND FCVECTOR_MSAN)
  message(FATAL_ERROR "[fcvector error]: AddressSanitizer and MemorySanitizer are both enabled at the same time!")
endif()

if (FCVECTOR_MSAN AND FCVECTOR_ENV_MACOSX)
  message(FATAL_ERROR "[fcvector error]: MemorySanitizer is not supported on MacOSX!")
endif()

# AddressSanitizer support
if (FCVECTOR_ASAN)
  # This policy enables passing the linker flags to the linker when trying to
  # test the features, which is required to successfully link ASan binaries
  cmake_policy(SET CMP0056 NEW)
  set (ASAN_FLAGS "")
  if (FCVECTOR_ENV_MACOSX) # LeakSanitizer not supported on MacOSX
    set (ASAN_FLAGS "-fsanitize=address,integer,undefined,nullability")
  else()
    if (FCVECTOR_CXX_COMPILER_CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.0")
      set (ASAN_FLAGS "-fsanitize=address")
    else()
      set (ASAN_FLAGS "-fsanitize=address,integer,undefined,leak,nullability")
    endif()
  endif()
  fcvector_append_flag(FCVECTOR_HAS_ASAN "${ASAN_FLAGS}")
  if (FCVECTOR_HAS_ASAN) #ASAN flags must be passed to the linker:
    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${ASAN_FLAGS}")
  endif()
  fcvector_append_flag(FCVECTOR_HAS_SANITIZE_NO_RECOVER "-fno-sanitize-recover=all")
  fcvector_append_flag(FCVECTOR_HAS_NO_OMIT_FRAME_POINTER -fno-omit-frame-pointer)
endif()

# MemorySanitizer support
if (FCVECTOR_MSAN)
  # This policy enables passing the linker flags to the linker when trying to
  # compile the examples, which is required to successfully link MSan binaries
  cmake_policy(SET CMP0056 NEW)
  fcvector_append_flag(FCVECTOR_HAS_MSAN "-fsanitize=memory")
  fcvector_append_flag(FCVECTOR_HAS_MSAN_TRACK_ORIGINS -fsanitize-memory-track-origins)
  fcvector_append_flag(FCVECTOR_HAS_SANITIZE_RECOVER_ALL "-fno-sanitize-recover=all")
  fcvector_append_flag(FCVECTOR_HAS_NO_OMIT_FRAME_POINTER -fno-omit-frame-pointer)
endif()

# Build types:
if (FCVECTOR_DEBUG_BUILD AND FCVECTOR_RELEASE_BUILD)
  message(FATAL_ERROR "[fcvector error] Cannot simultaneously generate debug and release builds!")
endif()

if (FCVECTOR_DEBUG_BUILD)
  fcvector_append_flag(FCVECTOR_HAS_O0 -O0)
  fcvector_append_flag(FCVECTOR_HAS_NO_INLINE -fno-inline)
  fcvector_append_flag(FCVECTOR_HAS_STACK_PROTECTOR_ALL -fstack-protector-all)
  fcvector_append_flag(FCVECTOR_HAS_NO_STRICT_ALIASING -fno-strict-aliasing)
  fcvector_append_flag(FCVECTOR_HAS_G3 -g3)
  # Clang can generate debug info tuned for LLDB or GDB
  if (FCVECTOR_CXX_COMPILER_CLANG)
    if (FCVECTOR_ENV_MACOSX)
      fcvector_append_flag(FCVECTOR_HAS_GLLDB -glldb)
    elseif(FCVECTOR_ENV_LINUX)
      fcvector_append_flag(FCVECTOR_HAS_GGDB -ggdb)
    endif()
  endif()
endif()

if (FCVECTOR_RELEASE_BUILD)
  if (NOT FCVECTOR_ASSERTIONS)
    fcvector_append_flag(FCVECTOR_HAS_DNDEBUG -DNDEBUG)
  endif()
  if (NOT FCVECTOR_ASAN AND NOT FCVECTOR_MSAN)
    # The quality of ASan and MSan error messages suffers if we disable the
    # frame pointer, so leave it enabled when compiling with either of them:
    fcvector_append_flag(FCVECTOR_HAS_OMIT_FRAME_POINTER -fomit-frame-pointer)
  endif()

  fcvector_append_flag(FCVECTOR_HAS_OFAST -Ofast)
  fcvector_append_flag(FCVECTOR_HAS_STRICT_ALIASING -fstrict-aliasing)
  if (NOT FCVECTOR_CXX_COMPILER_CLANGC2)
    fcvector_append_flag(FCVECTOR_HAS_STRICT_VTABLE_POINTERS -fstrict-vtable-pointers)
  endif()
  fcvector_append_flag(FCVECTOR_HAS_FAST_MATH -ffast-math)
  fcvector_append_flag(FCVECTOR_HAS_VECTORIZE -fvectorize)

  if (NOT FCVECTOR_ENV_MACOSX)
    # Sized deallocation is not available in MacOSX:
    fcvector_append_flag(FCVECTOR_HAS_SIZED_DEALLOCATION -fsized-deallocation)
  endif()

  if (FCVECTOR_LLVM_POLLY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -polly -mllvm -polly-vectorizer=stripmine")
  endif()

  if (FCVECTOR_CXX_COMPILER_CLANG AND (NOT (FCVECTOR_INLINE_THRESHOLD EQUAL -1)))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -inline-threshold=${FCVECTOR_INLINE_THRESHOLD}")
  endif()
endif()

if (FCVECTOR_NATIVE)
  fcvector_append_flag(FCVECTOR_HAS_MARCH_NATIVE "-march=native")
  fcvector_append_flag(FCVECTOR_HAS_MTUNE_NATIVE "-mtune=native")
endif()

if (FCVECTOR_VERBOSE_BUILD)
  message("[fcvector]: C++ flags: ${CMAKE_CXX_FLAGS}")
  message("[fcvector]: C++ debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
  message("[fcvector]: C++ Release Flags: ${CMAKE_CXX_FLAGS_RELEASE}")
  message("[fcvector]: C++ Compile Flags: ${CMAKE_CXX_COMPILE_FLAGS}")
  message("[fcvector]: Compile options: ${COMPILE_OPTIONS_}")
  message("[fcvector]: C Flags: ${CMAKE_C_FLAGS}")
  message("[fcvector]: C Compile Flags: ${CMAKE_C_COMPILE_FLAGS}")
  message("[fcvector]: EXE Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
  message("[fcvector]: C++ Linker flags: ${CMAKE_CXX_LINK_FLAGS}")
  message("[fcvector]: MODULE Linker flags: ${CMAKE_MODULE_LINKER_FLAGS}")
  get_directory_property(CMakeCompDirDefs COMPILE_DEFINITIONS)
  message("[fcvector]: Compile Definitions: ${CmakeCompDirDefs}")
endif()


# Template diagnostic flags
fcvector_append_flag(FCVECTOR_HAS_FDIAGNOSTIC_SHOW_TEMPLATE_TREE -fdiagnostics-show-template-tree)
fcvector_append_flag(FCVECTOR_HAS_FTEMPLATE_BACKTRACE_LIMIT "-ftemplate-backtrace-limit=0")
fcvector_append_flag(FCVECTOR_HAS___EXTERN_ALWAYS_INLINE -D__extern_always_inline=inline)


if (FCVECTOR_ENABLE_ASAN)
  set(FCVECTOR_ASAN_FLAGS "-fsanitize=address,integer,undefined -fno-sanitize-recover=address,integer,undefined -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/sanitize.blacklist")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FCVECTOR_ASAN_FLAGS}")
  fcvector_append_flag(FCVECTOR_HAS_NO_OMIT_FRAME_POINTER -fno-omit-frame-pointer)
else()
  fcvector_append_flag(FCVECTOR_HAS_OMIT_FRAME_POINTER -fomit-frame-pointer)
endif()

if (FCVECTOR_ENABLE_DEBUG_INFORMATION)
  fcvector_append_flag(FCVECTOR_HAS_G3 -g3)
else()
  fcvector_append_flag(FCVECTOR_HAS_G0 -g0)
endif()

if (NOT FCVECTOR_ENABLE_ASSERTIONS)
  fcvector_append_flag(FCVECTOR_HAS_DFCVECTOR_DISABLE_ASSERTIONS -DFCVECTOR_DISABLE_ASSERTIONS)
endif()

if (FCVECTOR_ENABLE_COVERAGE)
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "code coverage instrumentation requires CMAKE_BUILD_TYPE=Debug")
  endif()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

# Optimization flags: debug
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  fcvector_append_flag(FCVECTOR_HAS_O0 -O0)
  fcvector_append_flag(FCVECTOR_HAS_NO_INLINE -fno-inline)
  fcvector_append_flag(FCVECTOR_HAS_STACK_PROTECTOR_ALL -fstack-protector-all)
endif()

# Optimization flags: release
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  fcvector_append_flag(FCVECTOR_HAS_OFAST -Ofast)
  fcvector_append_flag(FCVECTOR_HAS_UNDEBUG -UNDEBUG)
  fcvector_append_flag(FCVECTOR_HAS_MARCH_NATIVE "-march=native")
  fcvector_append_flag(FCVECTOR_HAS_MTUNE_NATIVE "-mtune=native")
  fcvector_append_flag(FCVECTOR_HAS_STRICT_ALIASING -fstrict-aliasing)
  fcvector_append_flag(FCVECTOR_HAS_VECTORIZE -fvectorize)
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -polly -mllvm -polly-vectorizer=stripmine")

  # If ASan not enabled: omit frame pointer
  if (NOT FCVECTOR_ENABLE_ASAN)
    fcvector_append_flag(FCVECTOR_HAS_OMIT_FRAME_POINTER -fomit-frame-pointer)
  endif()
endif()

if (NOT FCVECTOR_ENABLE_LIKELY)
  fcvector_append_flag(FCVECTOR_HAS_DISABLE_LIKELY -DFCVECTOR_DISABLE_LIKELY_MACROS)
endif()

