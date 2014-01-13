/**
 * \file glc/common/core.h
 * \brief glc core interface adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
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
 * \defgroup common_core core
 *  \{
 */

#ifndef _CORE_H
#define _CORE_H

#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief get glc version
 * \return glc version string
 */
__PUBLIC const char *glc_version();

/**
 * \brief initialize glc
 *
 * This function initializes core, log and util components.
 * State is not initialized.
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_init(glc_t *glc);

/**
 * \brief destroy glc
 *
 * This cleans up core, log and util. State must be destroyed
 * before calling this function.
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_destroy(glc_t *glc);

/**
 * \brief current time in nanoseconds since initialization
 *
 * the 64 bits glc_utime_t is big enough to store over 500 years in
 * nanoseconds so overflow is not an issue.
 * \param glc glc
 * \return time elapsed since initialization
 */
__PUBLIC glc_utime_t glc_time(glc_t *glc);

/**
 * \brief thread count hint
 *
 * All processing filters that can employ multiple threads use
 * this function to determine how many threads to create. By default
 * this returns number of processors online, but custom value can
 * be set via glc_set_threads_hint().
 * \param glc glc
 * \return thread count hint
 */
__PUBLIC long int glc_threads_hint(glc_t *glc);

/**
 * \brief set thread count hint
 *
 * Default value is number of processors.
 * \param glc glc
 * \param count thread count hint
 * \return 0 on success otherwise an error code
 */
__PUBLIC int glc_set_threads_hint(glc_t *glc, long int count);

__PUBLIC void glc_account_threads(glc_t *glc, long int single, long int multi);

__PUBLIC void glc_compute_threads_hint(glc_t *glc);

#ifdef __cplusplus
}
#endif

#endif

/**  \} */
/**  \} */
