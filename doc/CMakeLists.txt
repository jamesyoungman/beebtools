###################################### Manual Pages

set(INSTALL_PREFIX_SHARE ${CMAKE_INSTALL_PREFIX}/share)
set(INSTALL_PREFIX_MAN   ${INSTALL_PREFIX_SHARE}/man)
set(MAN1_NAMES bbcbasic_to_text.1 dfs.1)
set(MAN5_NAMES bbcbasic.5)
set(MAN1_FILES)
set(MAN5_FILES)

find_program(GZIP gzip)

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

INSTALL(FILES ${MAN1_FILES} DESTINATION  ${INSTALL_PREFIX_MAN}/man1)
INSTALL(FILES ${MAN5_FILES} DESTINATION  ${INSTALL_PREFIX_MAN}/man5)

