/**
 * \file glc/common/signal.h
 * \brief glcs signal related functions.
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

#ifndef _GLCS_SIGNAL_H
#define _GLCS_SIGNAL_H

#include <sys/types.h> // for pid_t
#include <time.h>      // for struct timespec
#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Configure the calling thread signal dispositions
 *        in order to make the others functions of this module
 *        work properly.
 */
__PUBLIC int glcs_signal_init_thread_disposition(glc_t *glc);

__PUBLIC void glcs_signal_reset();

/*
 * Must have called glcs_signal_init_thread_disposition() first from the thread calling
 * this function.
 */
__PUBLIC int glcs_signal_timed_waitpid(glc_t *glc, pid_t pid, int *stat_loc,
				const struct timespec *ts);

__PUBLIC void glcs_signal_pr_exit(glc_t *glc, pid_t pid, int status);

#ifdef __cplusplus
}
#endif

#endif

