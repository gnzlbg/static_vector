# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
#
# The `format` target re-formats/checks all the c++ files in the git repo.
#
# Two targets are added:
# - make format: reformats all files in the git repo.
# - make check-format: check the format of all files in the git repo.

find_program(CLANG_FORMAT NAMES clang-format-3.8 clang-format-3.7 clang-format-3.6 clang-format)
if(CLANG_FORMAT)
  add_custom_command(OUTPUT format-cmd COMMAND
    ${PROJECT_SOURCE_DIR}/cmake/format.py ${CLANG_FORMAT} ${PROJECT_SOURCE_DIR} --apply)
  add_custom_target(format DEPENDS format-cmd WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  
  add_custom_command(OUTPUT check-format-cmd COMMAND
    ${PROJECT_SOURCE_DIR}/cmake/format.py ${CLANG_FORMAT} ${PROJECT_SOURCE_DIR})
  add_custom_target(check-format DEPENDS check-format-cmd WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
