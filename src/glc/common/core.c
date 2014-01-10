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
	long int single_process_num;
	long int multi_process_num;
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

	glc->core->threads_hint = 1; /* safe conservative default value */

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

void glc_account_threads(glc_t *glc, long int single, long int multi)
{
	glc->core->single_process_num += single;
	glc->core->multi_process_num  += multi;
}

void glc_compute_threads_hint(glc_t *glc)
{
	if (unlikely(!glc->core->multi_process_num))
		glc->core->multi_process_num = 1; /* Avoid division by 0 */
	glc->core->threads_hint  = sysconf(_SC_NPROCESSORS_ONLN) - glc->core->single_process_num;
	glc->core->threads_hint /= glc->core->multi_process_num;
	if (unlikely(glc->core->threads_hint <  1))
		glc->core->threads_hint = 1;
	glc_log(glc, GLC_INFORMATION, "core",
		"single process num %ld multi process num %d threads num per multi process",
		glc->core->single_process_num, glc->core->multi_process_num,
		glc->core->threads_hint);
}

/**  \} */
