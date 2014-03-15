FIND_PACKAGE(ELFHACKS)
IF (ELFHACKS_FOUND)
    # TODO: Try to build local copy of library if no system version is found.
    INCLUDE_DIRECTORIES(${ELFHACKS_INCLUDE_DIR})
ENDIF (ELFHACKS_FOUND)

ADD_LIBRARY("glc-hook" SHARED "lib.h" "alsa.c" "main.c" "opengl.c" "x11.c")
TARGET_LINK_LIBRARIES("glc-hook" "glc-core" "glc-capture"
                      ${ELFHACKS_LIBRARY} ${PACKETSTREAM_LIBRARY})
SET_TARGET_PROPERTIES("glc-hook" PROPERTIES OUTPUT_NAME "glc-hook"
                      VERSION ${GLC_VER} SOVERSION ${GLC_SOVER})

IF (UNIX)
    INSTALL(TARGETS "glc-hook" LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR})
ENDIF (UNIX)
