# Look for a .git directory at the project root.
IF (NOT GIT_DIR AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    SET(GIT_DIR ${PROJECT_SOURCE_DIR})
ENDIF (NOT GIT_DIR AND EXISTS "${PROJECT_SOURCE_DIR}/.git")

# Use `git describe` to set a version number. Fall-back to GLCS_VER.
IF (GIT_DIR)
    EXEC_PROGRAM("git" ${GIT_DIR} ARGS "describe --always --dirty"
                 OUTPUT_VARIABLE GLCS_VERSION RETURN_VALUE GIT_RETURN)
    IF (NOT GIT_RETURN EQUAL 0)
        SET(GLCS_VERSION ${GLCS_VER})
    ENDIF (NOT GIT_RETURN EQUAL 0)
ELSE (GIT_DIR)
    SET(GLCS_VERSION ${GLCS_VER})
ENDIF (GIT_DIR)

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/common/version.h.in"
               "${CMAKE_CURRENT_SOURCE_DIR}/common/version.h")


# Set up the compression libraries.
SET(QUICKLZ_SRC)
IF (QUICKLZ)
    SET(QUICKLZ_DIR "${PROJECT_SOURCE_DIR}/support/quicklz")
    SET(QUICKLZ_SRC "${QUICKLZ_DIR}/quicklz.h" "${QUICKLZ_DIR}/quicklz.c")
    INCLUDE_DIRECTORIES(${QUICKLZ_DIR})
    ADD_DEFINITIONS("-D__QUICKLZ")
ENDIF (QUICKLZ)

SET(LZO_SRC)
IF (LZO)
    SET(LZO_DIR "${PROJECT_SOURCE_DIR}/support/minilzo")
    SET(LZO_SRC "${LZO_DIR}/lzoconf.h" "${LZO_DIR}/lzodefs.h"
                "${LZO_DIR}/minilzo.h" "${LZO_DIR}/minilzo.c")
    INCLUDE_DIRECTORIES(${LZO_DIR})
    ADD_DEFINITIONS("-D__MINILZO")
ENDIF (LZO)

SET(LZJB_SRC)
IF (LZJB)
    SET(LZJB_DIR "${PROJECT_SOURCE_DIR}/support/lzjb")
    SET(LZJB_SRC "${LZJB_DIR}/lzjb.h" "${LZJB_DIR}/lzjb.c")
    INCLUDE_DIRECTORIES(${LZJB_DIR})
    ADD_DEFINITIONS("-D__LZJB")
ENDIF (LZJB)


# This is where the library targets are defined.
SET(COMMON_SRC "common/core.h" "common/glc.h" "common/log.h"
    "common/optimization.h" "common/signal.h" "common/state.h"
    "common/thread.h" "common/util.h" "common/version.h" "common/rational.h"
    "common/core.c" "common/log.c" "common/signal.c" "common/state.c"
    "common/thread.c" "common/util.c" "common/rational.c")

# GLCS Core library.
ADD_LIBRARY("glc-core" SHARED ${COMMON_SRC}
    "core/color.h" "core/copy.h" "core/file.h" "core/frame_writers.h"
    "core/info.h" "core/pack.h" "core/pipe.h" "core/rgb.h" "core/scale.h"
    "core/sink.h" "core/source.h" "core/tracker.h" "core/ycbcr.h"
    "core/color.c" "core/copy.c" "core/file.c" "core/frame_writers.c"
    "core/info.c" "core/pack.c" "core/pipe.c" "core/rgb.c" "core/scale.c"
    "core/tracker.c" "core/ycbcr.c" ${QUICKLZ_SRC} ${LZO_SRC} ${LZJB_SRC})
TARGET_LINK_LIBRARIES("glc-core" "m" ${ACKETSTREAM_LIBRARY})
SET_TARGET_PROPERTIES("glc-core" PROPERTIES OUTPUT_NAME "glc-core"
                      VERSION ${GLCS_VER} SOVERSION ${GLCS_SOVER})

# Capture library.
ADD_LIBRARY("glc-capture" SHARED ${COMMON_SRC}
    "capture/alsa_capture.h" "capture/alsa_hook.h" "capture/audio_capture.h"
    "capture/gl_capture.h" "capture/alsa_capture.c" "capture/alsa_hook.c"
    "capture/audio_capture.c" "capture/gl_capture.c")
TARGET_LINK_LIBRARIES("glc-capture" "GL" "dl" "asound" "X11" "Xxf86vm" "glc-core")
SET_TARGET_PROPERTIES("glc-capture" PROPERTIES OUTPUT_NAME "glc-capture"
                      VERSION ${GLCS_VER} SOVERSION ${GLCS_SOVER})

# Player library.
ADD_LIBRARY("glc-play" SHARED ${COMMON_SRC}
    "play/alsa_play.h" "play/demux.h" "play/gl_play.h"
    "play/alsa_play.c" "play/demux.c" "play/gl_play.c")
TARGET_LINK_LIBRARIES("glc-play" "GL" "asound" "X11" "glc-core")
SET_TARGET_PROPERTIES("glc-play" PROPERTIES OUTPUT_NAME "glc-play"
                      VERSION ${GLCS_VER} SOVERSION ${GLCS_SOVER})

ADD_LIBRARY("glc-export" SHARED ${COMMON_SRC}
    "export/img.h" "export/wav.h" "export/yuv4mpeg.h"
    "export/img.c" "export/wav.c" "export/yuv4mpeg.c")
TARGET_LINK_LIBRARIES("glc-export" "png" "glc-core")
SET_TARGET_PROPERTIES("glc-export" PROPERTIES OUTPUT_NAME "glc-export"
                      VERSION ${GLCS_VER} SOVERSION ${GLCS_SOVER})


# Install this stuff!
IF (UNIX)
    INSTALL(TARGETS "glc-core" "glc-capture" "glc-play" "glc-export"
            LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR})
ENDIF (UNIX)
