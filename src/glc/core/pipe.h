/**
 * \file glc/core/pipe.h
 * \brief Pipe implementation of the sink interface.
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

#ifndef _PIPE_H
#define _PIPE_H

#include <glc/core/sink.h>

#ifdef __cplusplus
extern "C" {
#endif

__PUBLIC int pipe_sink_init(sink_t *sink, glc_t *glc, const char *exec_file,
			    int invert, unsigned delay_ms,
			    int (*stop_capture_cb)());

#ifdef __cplusplus
}
#endif

#endif

