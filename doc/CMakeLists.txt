###################################### Manual Pages

option(PDF "Generate pdf format documentation" OFF)

set(INSTALL_PREFIX_SHARE ${CMAKE_INSTALL_PREFIX}/share)
set(INSTALL_PREFIX_MAN   ${INSTALL_PREFIX_SHARE}/man)
set(MAN1_NAMES bbcbasic_to_text.1 dfs.1)
set(MAN5_NAMES bbcbasic.5)
set(MAN1_FILES)
set(MAN5_FILES)

find_program(GZIP gzip)

if (PDF)
  find_program(PDFROFF pdfroff)
endif()

macro(gzip_file SOURCE DESTINATION)
  add_custom_command(
    COMMENT "gzipping ${SOURCE} to make ${DESTINATION}"
    DEPENDS ${SOURCE}
    OUTPUT ${DESTINATION}
    COMMAND ${GZIP} -9 < ${SOURCE} > ${DESTINATION}
    VERBATIM)
endmacro(gzip_file)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/bbcbasic.5 
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

