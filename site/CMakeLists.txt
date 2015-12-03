# Copyright Louis Dionne 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

##############################################################################
# Setup the documentation
##############################################################################
if (NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen was not found; the 'doc' and the various "
                    "'gh-pages.*' targets will be unavailable.")
    return()
endif()

configure_file(Doxyfile.in Doxyfile @ONLY)
add_custom_target(doc
    COMMAND ! ${DOXYGEN_EXECUTABLE} Doxyfile 2>&1 | grep "warning"
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
)

if (NOT GIT_FOUND)
    message(WARNING "Git was not found; the various 'gh-pages.*' targets will be unavailable.")
    return()
endif()

##############################################################################
# Setup the URL to the official repository on GitHub.
#
# If there is a GH_TOKEN environment variable, we assume this token allows
# us to push to the GitHub repository and we use that in the URL. This token
# will give us push access for updating the 'gh-pages' branch and doing other
# useful things e.g. from Travis. Otherwise, if there's no such environment
# variable, the URL just refers to the GitHub repository without authentication.
# In this case, we'll be asked for our username and password when/if we try to
# push to the repository.
##############################################################################
if (DEFINED ENV{GH_TOKEN})
  set(STACK_VECTOR_ORIGIN_URL "https://gnzlbg:$ENV{GH_TOKEN}@github.com/gnzlbg/stack_vector.git")
else()
  set(STACK_VECTOR_ORIGIN_URL "https://github.com/gnzlbg/stack_vector")
endif()

add_custom_target(gh-pages.clone
    COMMAND [ -e ${CMAKE_SOURCE_DIR}/site/gh-pages ] || ${GIT_EXECUTABLE} clone ${STACK_VECTOR_ORIGIN_URL} --depth 1 --branch=gh-pages site/gh-pages
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Cloning the gh-pages branch into the site/gh-pages directory"
    VERBATIM
)

add_custom_target(gh-pages.clean
    COMMAND ${CMAKE_COMMAND} -E remove *.png *.css *.js *.html
    COMMAND ${CMAKE_COMMAND} -E remove_directory search
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/gh-pages
    DEPENDS gh-pages.clone
    COMMENT "Cleaning up site/gh-pages"
    VERBATIM
)

add_custom_target(gh-pages.copy
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/html ${CMAKE_CURRENT_LIST_DIR}/gh-pages
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/gh-pages
    COMMENT "Copying the documentation from ${CMAKE_CURRENT_BINARY_DIR}/html to site/gh-pages"
    DEPENDS doc gh-pages.clean
    VERBATIM
)

add_custom_target(gh-pages.update
    COMMAND ${GIT_EXECUTABLE} add --all .
    COMMAND ${GIT_EXECUTABLE} commit --allow-empty -m "Update to ${STACK_VECTOR_CURRENT_COMMIT}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/gh-pages
    COMMENT "Updating the gh-pages clone with freshly built documentation"
    DEPENDS gh-pages.copy
    VERBATIM
)

add_custom_target(gh-pages.push
    COMMAND ${GIT_EXECUTABLE} push origin gh-pages
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/gh-pages
    COMMENT "Pushing the gh-pages branch to GitHub"
    DEPENDS gh-pages.update
    VERBATIM
)
