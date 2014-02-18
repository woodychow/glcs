/**
 * \file glc/core/source.h
 * \brief Abstract ssource interface
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

#ifndef _SOURCE_H
#define _SOURCE_H

#include <packetstream.h>
#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct source_s* source_t;

typedef struct
{
	/**
	 * \brief open file for reading
	 *
	 * \param source source object
	 * \param filename source file
	 * \return 0 on success otherwise an error code
	 */
	int (*open_source)(source_t source, const char *filename);
	/**
	 * \brief close source file
	 * \param source source object
	 * \return 0 on success otherwise an error code
	 */
	int (*close_source)(source_t source);
	/**
	 * \brief read stream information
	 * \note info_name and info_date are allocated but file_destroy()
	 *       won't free them.
	 * \param source source object
	 * \param info info structure
	 * \param info_name app name
	 * \param info_date date
	 * \return 0 on success otherwise an error code
	 */
	int (*read_info)(source_t source, glc_stream_info_t *info,
			char **info_name, char **info_date);
	/**
	 * \brief read stream from file and write it into buffer
	 * \param source source object
	 * \param to buffer
	 * \return 0 on success otherwise an error code
	 */
	int (*read)(source_t source, ps_buffer_t *to);
	int (*destroy)(source_t source);
} source_ops_t;

struct source_s
{
	source_ops_t *ops;
};

#ifdef __cplusplus
}
#endif

#endif

