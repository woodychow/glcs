/**
 * \file glc/common/log.h
 * \brief glc log interface adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
 * \author Olivier Langlois <olivier@trillion01.com>
 * \date 2014

    This file is part of glcs.

    glcs is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    glcs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with glcs.  If not, see <http://www.gnu.org/licenses/>.

 */

/**
 * \addtogroup common
 *  \{
 * \defgroup log logging
 *  \{
 */

#ifndef _LOG_H
#define _LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief initialize log
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PRIVATE int glc_log_init(glc_t *glc);

/**
 * \brief destroy log
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PRIVATE int glc_log_destroy(glc_t *glc);

/**
 * \brief set log level
 *
 * Messages with level <= current log level will be written
 * into log.
 * \param glc glc
 * \param level log level
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_log_set_level(glc_t *glc, int level);
__PUBLIC int glc_log_get_level(glc_t *glc);

__PUBLIC FILE *glc_log_get_stream(glc_t *glc);

/**
 * \brief open file for log
 * \note this calls glc_log_set_stream()
 * \param glc glc
 * \param filename log file name
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_log_open_file(glc_t *glc, const char *filename);

/**
 * \brief set log stream
 *
 * Default log target is stderr.
 * \param glc glc
 * \param stream log stream
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_log_set_stream(glc_t *glc, FILE *stream);

/**
 * \brief close current log stream
 *
 * Log file is set to stderr.
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_log_close(glc_t *glc);

/**
 * \brief write message to log
 *
 * Message is actually written to log if level is
 * lesser than, or equal to current log verbosity level and
 * logging is enabled.
 * \param glc glc
 * \param level message level
 * \param module module
 * \param format passed to fprintf()
 * \param ... passed to fprintf()
 */
__PUBLIC void glc_log(glc_t *glc, int level, const char *module, const char *format, ...)
	__attribute__((format(printf, 4, 5)));

#ifdef __cplusplus
}
#endif

#endif

/**  \} */
/**  \} */
