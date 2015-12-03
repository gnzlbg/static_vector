# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
# The `tidy` target tidy/checks all the c++ files in the git repo.
#
# Two targets are added:
# - make tidy: tidy all files in the git repo.
# - make check-tidy: check if all files in the git repo are tidy.

find_program(CLANG_TIDY NAMES clang-tidy-3.8 clang-tidy-3.7 clang-tidy-3.6 clang-tidy)
if(CLANG_TIDY)
# This is still too buggy:
#  add_custom_command(OUTPUT tidy-cmd COMMAND
#    ${PROJECT_SOURCE_DIR}/cmake/tidy.py ${CLANG_TIDY} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} --apply --verbose)
#  add_custom_target(tidy DEPENDS tidy-cmd fetch_packages WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  
  add_custom_command(OUTPUT check-tidy-cmd COMMAND
    ${PROJECT_SOURCE_DIR}/cmake/tidy.py ${CLANG_TIDY} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} --verbose)
  add_custom_target(check-tidy DEPENDS check-tidy-cmd fetch_packages check-format WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
