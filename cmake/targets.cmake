# Copyright Louis Dionne 2015
# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

##############################################################################
# Setup custom functions to ease the creation of targets
##############################################################################
#   fcvector_target_name_for(<output variable> <source file> [ext])
#
# Return the target name associated to a source file. If the path of the
# source file relative from the root of fcvector is `path/to/source/file.ext`,
# the target name associated to it will be `path.to.source.file`.
#
# The extension of the file should be specified as a last argument. If no
# extension is specified, the `.cpp` extension is assumed.
function(fcvector_target_name_for out file)
    if (NOT ARGV2)
        set(_extension ".cpp")
    else()
        set(_extension "${ARGV2}")
    endif()

    file(RELATIVE_PATH _relative ${fcvector_SOURCE_DIR} ${file})
    string(REPLACE "${_extension}" "" _name ${_relative})
    string(REGEX REPLACE "/" "." _name ${_name})
    set(${out} "${_name}" PARENT_SCOPE)
endfunction()

#   fcvector_list_remove_glob(<list> <GLOB|GLOB_RECURSE> [globbing expressions]...)
#
# Generates a list of files matching the given glob expressions, and remove
# the matched elements from the given <list>.
macro(fcvector_list_remove_glob list glob)
    file(${glob} _bhlrg10321023_avoid_macro_clash_matches ${ARGN})
    list(REMOVE_ITEM ${list} ${_bhlrg10321023_avoid_macro_clash_matches})
endmacro()

#   fcvector_add_packages_to_target(<name> <command> [<arg>...])
#
# Adds packages as a dependency to target
function(fcvector_add_packages_to_target name)
  add_dependencies(${name} fetch_packages)
endfunction()

# The `format` target re-formats/checks all the c++ files in the git repo.
#
# Two targets are added:
# - make format: reformats all files in the git repo.
# - make check-format: check the format of all files in the git repo.
find_program(CLANG_FORMAT NAMES clang-format-4.0 clang-format-3.9 clang-format-3.8 clang-format-3.7 clang-format-3.6 clang-format)
if(CLANG_FORMAT)
  add_custom_command(OUTPUT format-cmd COMMAND
    ${PROJECT_SOURCE_DIR}/cmake/format.py ${CLANG_FORMAT} ${PROJECT_SOURCE_DIR} --apply)
  add_custom_target(format DEPENDS format-cmd WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  
  add_custom_command(OUTPUT check-format-cmd COMMAND
    ${PROJECT_SOURCE_DIR}/cmake/format.py ${CLANG_FORMAT} ${PROJECT_SOURCE_DIR})
  add_custom_target(check-format DEPENDS check-format-cmd WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()

# The `check-tidy` target checks all the c++ files in the git repo.
#
# One target is added:
# - make check-tidy
find_program(CLANG_TIDY NAMES clang-tidy-4.0 clang-tidy-3.9 clang-tidy-3.8 clang-tidy-3.7 clang-tidy-3.6 clang-tidy)
if(CLANG_TIDY)
  add_custom_command(OUTPUT check-tidy-cmd COMMAND
    ${PROJECT_SOURCE_DIR}/cmake/tidy.py ${CLANG_TIDY} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} --verbose)
  add_custom_target(check-tidy DEPENDS check-tidy-cmd fetch_packages check-format WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()

# Target for fetching packages
add_custom_target(fetch_packages)
