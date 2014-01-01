/**
 * \file glc/common/core.c
 * \brief glc core
 * \author Pyry Haulos <pyry.haulos@gmail.com>
 * \date 2007-2008
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup common_core
 *  \{
 */

#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "glc.h"
#include "core.h"
#include "log.h"
#include "util.h"
#include "optimization.h"

struct glc_core_s {
	struct timespec init_time;
	long int threads_hint;
};

const char *glc_version()
{
	return GLC_VERSION;
}

int glc_init(glc_t *glc)
{
	int ret = 0;

	/* clear 'em */
	glc->core  = NULL;
	glc->state = NULL;
	glc->util  = NULL;
	glc->log   = NULL;

	glc->core = (glc_core_t) calloc(1, sizeof(struct glc_core_s));

	clock_getttime(CLOCK_MONOTONIC, &glc->core->init_time);

	/*
	 * threads_hint sets the number of threads used in filters
	 * for modifying the video during playback. I see at least 2
	 * problems with that:
	 *
	 * 1. For the play pipeline, you end up with 4*number of cores threads.
	 *    IMHO, it is not a good practice to have much more threads than
	 *    you have cores. This is especially true for CPU bounded processing
	 *    like what you find in the glc filters.
	 * 2. I see no synchronization to ensure that packets processed in parallel
	 *    in a given pipeline stage are put back in the correct order in the next
	 *    ring buffer. I have experienced playback freeze and I suspect this being
	 *    the cause:
	 *
	 *    In gl_play.c, you have pic_hdr->time > time + gl_play->sleep_threshold
	 *    You end up with an underflow if this condition is true and
	 *    pic_hdr->time < time.
	 */
//	glc->core->threads_hint = sysconf(_SC_NPROCESSORS_ONLN);
	glc->core->threads_hint = 1;

	if (unlikely((ret = glc_log_init(glc))))
		return ret;
	ret = glc_util_init(glc);
	return ret;
}

int glc_destroy(glc_t *glc)
{
	glc_util_destroy(glc);
	glc_log_destroy(glc);

	free(glc->core);

	/* and clear */
	glc->core = NULL;
	glc->state = NULL;
	glc->util = NULL;
	glc->log = NULL;

	return 0;
}

glc_utime_t glc_time(glc_t *glc)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	ts.tv_sec -= glc->core->init_time.tv_sec;
	ts.tv_nsec -= glc->core->init_time.tv_nsec;

	if (ts.tv_usec < 0) {
		ts.tv_sec--;
		ts.tv_nsec += 1000000000;
	}

	return (glc_utime_t) ts.tv_sec * (glc_utime_t) 1000000000 + (glc_utime_t) ts.tv_usec;
}

long int glc_threads_hint(glc_t *glc)
{
	return glc->core->threads_hint;
}

int glc_set_threads_hint(glc_t *glc, long int count)
{
	if (unlikely(count <= 0))
		return EINVAL;
	glc->core->threads_hint = count;
	return 0;
}

/**  \} */
