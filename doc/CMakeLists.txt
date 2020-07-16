#
#   Copyright 2020 James Youngman
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

###################################### Manual Pages

set(INSTALL_PREFIX_SHARE ${CMAKE_INSTALL_PREFIX}/share)
set(INSTALL_PREFIX_MAN   ${INSTALL_PREFIX_SHARE}/man)
set(MAN1_NAMES bbcbasic_to_text.1 dfs.1)
set(MAN5_NAMES bbcbasic.5)
set(MAN1_FILES)
set(MAN5_FILES)

find_program(GZIP gzip)
if (${GZIP} STREQUAL "GZIP-NOTFOUND")
  # The regression tests need gunzip and assume it is on the path, but
  # the build process also uses this GZIP variable to compress manual
  # pages.  That is still needed even if you don't plan to test
  # anything.
  #
  # It would be possible to arrange to install uncompressed pages if
  # we didn't find gzip, but this kind of environment-dependent silent
  # behaviour change normally isn't very helpful to people who are
  # packaging the code.
  message(FATAL_ERROR "Cannot find a gzip executable")
endif()

find_program(GUNZIP gzip)
if (${GUNZIP} STREQUAL "GUNZIP-NOTFOUND")
  message(WARNING "Cannot find gunzip; the build will work but the tests will likely not")
endif()

if (PDF)
  find_program(PDFROFF pdfroff)
  if (${PDFROFF} STREQUAL "PDFROFF-NOTFOUND")
    message(FATAL_ERROR "Cannot find the pdfroff program, try turning off the PDF option")
  else()
    message(STATUS "Located pdfroff at ${PDFROFF}")
  endif()
endif()

macro(gzip_file SOURCE DESTINATION)
  add_custom_command(
    COMMENT "gzipping ${SOURCE} to make ${DESTINATION}"
    DEPENDS ${SOURCE}
    OUTPUT ${DESTINATION}
    COMMAND ${GZIP} -9 < ${SOURCE} > ${DESTINATION}
    VERBATIM)
endmacro(gzip_file)

# Compress manual pages and arrange for them to be installed.
foreach(m IN LISTS MAN1_NAMES)
  gzip_file(${CMAKE_CURRENT_SOURCE_DIR}/${m} ${CMAKE_CURRENT_BINARY_DIR}/${m}.gz)
  list(APPEND MAN1_FILES ${CMAKE_CURRENT_BINARY_DIR}/${m}.gz)
endforeach()
foreach(m IN LISTS MAN5_NAMES)
  gzip_file(${CMAKE_CURRENT_SOURCE_DIR}/${m} ${CMAKE_CURRENT_BINARY_DIR}/${m}.gz)
  list(APPEND MAN5_FILES ${CMAKE_CURRENT_BINARY_DIR}/${m}.gz)
endforeach()
ADD_CUSTOM_TARGET(man ALL DEPENDS ${MAN1_FILES} ${MAN5_FILES})

if (PDF)
  add_custom_command(
    OUTPUT bbcbasic.pdf
    COMMAND ${PDFROFF} -man --pdf-output=bbcbasic.pdf -t
    ${CMAKE_CURRENT_SOURCE_DIR}/bbcbasic.5
    DEPENDS bbcbasic.5)
  add_custom_command(
    OUTPUT bbcbasic_to_text.pdf
    COMMAND ${PDFROFF} -man --pdf-output=bbcbasic_to_text.pdf -t
    ${CMAKE_CURRENT_SOURCE_DIR}/bbcbasic_to_text.1
    DEPENDS bbcbasic_to_text.1)
  add_custom_command(
    OUTPUT dfs.pdf
    COMMAND ${PDFROFF} -man --pdf-output=dfs.pdf -t
    ${CMAKE_CURRENT_SOURCE_DIR}/dfs.1
    DEPENDS dfs.1)
  add_custom_target(pdf ALL
    DEPENDS bbcbasic.pdf bbcbasic_to_text.pdf dfs.pdf)
endif()

INSTALL(FILES ${MAN1_FILES} DESTINATION  ${INSTALL_PREFIX_MAN}/man1)
INSTALL(FILES ${MAN5_FILES} DESTINATION  ${INSTALL_PREFIX_MAN}/man5)
