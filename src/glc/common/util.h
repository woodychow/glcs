/**
 * \file glc/common/util.h
 * \brief utility functions interface adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
 * \author Olivier Langlois <olivier@trillion01.com>
 * \date 2014

    Copyright 2014 Olivier Langlois

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
 * \defgroup util utility functions
 *  \{
 */

#ifndef _UTIL_H
#define _UTIL_H

#include <packetstream.h>
#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief initialize utilities
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PRIVATE int glc_util_init(glc_t *glc);

/**
 * \brief destroy utilities
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PRIVATE int glc_util_destroy(glc_t *glc);

/**
 * \brief set fps hint for stream information
 * \param glc glc
 * \param fps fps
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_util_info_fps(glc_t *glc, double fps);

/**
 * \brief create stream information
 * \param glc glc
 * \param stream_info returned stream information structure
 * \param info_name returned application name
 * \param info_date returned date
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_util_info_create(glc_t *glc, glc_stream_info_t **stream_info,
				  char **info_name, char *info_date);

/**
 * \brief write version message into log
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_util_log_version(glc_t *glc);

/**
 * \brief write system information into log
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_util_log_info(glc_t *glc);

/**
 * \brief write 'end of stream'-packet into buffer
 * \param glc glc
 * \param to target buffer
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_util_write_end_of_stream(glc_t *glc, ps_buffer_t *to);

/**
 * \brief replace all occurences of string with another string
 * \param str string to manipulate
 * \param find string to find
 * \param replace string to replace occurences with
 * \return new string
 */
__PUBLIC char *glc_util_str_replace(const char *str, const char *find, const char *replace);

/**
 * \brief create filename based on current date, time, app name, pid etc.
 *
 * Available tags in format are:
 *  %app%	binary name without path
 *  %pid%	process id
 *  %capture%	N'th capture (given as argument)
 *  %year%	4-digit year
 *  %month%	2-digit month
 *  %day%	2-digit day
 *  %hour%	2-digit hour
 *  %min%	2-digit minute
 *  %sec%	2-digit second
 * \param format format string
 * \param capture N'th capture
 * \return new filename string
 */
__PUBLIC char *glc_util_format_filename(const char *fmt, unsigned int capture);

__PUBLIC int glc_util_setflag( int fd, int flag );
__PUBLIC int glc_util_clearflag( int fd, int flag );
__PUBLIC int glc_util_set_nonblocking(int fd);
__PUBLIC void glc_util_empty_pipe(int fd);
__PUBLIC const char *glc_util_msgtype_to_str(glc_message_type_t type);

#ifdef __cplusplus
}
#endif

#endif

/**  \} */
/**  \} */
