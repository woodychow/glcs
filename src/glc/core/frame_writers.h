/**
 * \file glc/core/frame_writers.h
 * \brief Abstract frame_writer interface
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

#ifndef _FRAME_WRITERS_H
#define _FRAME_WRITERS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct frame_writer_s* frame_writer_t;

typedef struct
{
	int (*configure)(frame_writer_t writer, int r_sz, int h);
	int (*write_init)(frame_writer_t writer, char *);
	/*
	 * return value: -1 in case of error where you can consult errno to determine
	 *               the cause or else # of bytes to write left to complete the
	 *               frame write.
	 */
	int (*write)(frame_writer_t writer, int fd);
	int (*destroy)(frame_writer_t writer);
} write_ops_t;

struct frame_writer_s
{
	write_ops_t *ops;
};

int glcs_std_create( frame_writer_t *writer );
int glcs_invert_create( frame_writer_t *writer );

#ifdef __cplusplus
}
#endif

#endif

