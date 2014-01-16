/**
 * \file glc/common/log.c
 * \brief logging adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
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
 * \addtogroup log
 *  \{
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "core.h"
#include "log.h"
#include "optimization.h"

struct glc_log_s {
	int level;
	FILE *stream;
	FILE *default_stream;
	pthread_mutex_t log_mutex;
};

static void glc_log_write_prefix(glc_t *glc, FILE *stream, int level, const char *module);

int glc_log_init(glc_t *glc)
{
	glc->log = (glc_log_t) calloc(1, sizeof(struct glc_log_s));

	pthread_mutex_init(&glc->log->log_mutex, NULL);
	glc->log->default_stream = stderr;
	glc->log->stream = glc->log->default_stream;

	return 0;
}

int glc_log_destroy(glc_t *glc)
{
	pthread_mutex_destroy(&glc->log->log_mutex);
	free(glc->log);
	return 0;
}

int glc_log_open_file(glc_t *glc, const char *filename)
{
	int ret;
	FILE *stream = fopen(filename, "w");
	if (unlikely(!stream))
		return errno;

	/* line buffered like stderr */
	setvbuf( stream, NULL, _IOLBF, 0 );

	if (unlikely((ret = glc_log_set_stream(glc, stream)))) {
		fclose(stream);
		return ret;
	}

	glc_log(glc, GLC_INFORMATION, "log", "opened %s for log", filename);
	return 0;
}

int glc_log_set_stream(glc_t *glc, FILE *stream)
{
	/** \todo check that stream is good */
	if (unlikely(!stream))
		return EINVAL;
	glc->log->stream = stream;
	return 0;
}

int glc_log_set_level(glc_t *glc, int level)
{
	if (unlikely(level < 0))
		return EINVAL;
	glc->log->level = level;
	return 0;
}

int glc_log_get_level(glc_t *glc)
{
	return glc->log->level;
}

FILE *glc_log_get_stream(glc_t *glc)
{
	return glc->log->stream;
}

int glc_log_close(glc_t *glc)
{
	glc_log(glc, GLC_INFORMATION, "log", "log closed");
	if (unlikely(fclose(glc->log->stream)))
		return errno;
	glc->log->stream = glc->log->default_stream;

	return 0;
}

void glc_log(glc_t *glc, int level, const char *module, const char *format, ...)
{
	va_list ap;

	if (level > glc->log->level)
		return;

	va_start(ap, format);

	/* this is highly threaded application and we want
	   non-corrupted logs */
	pthread_mutex_lock(&glc->log->log_mutex);
	glc_log_write_prefix(glc, glc->log->stream, level, module);
	vfprintf(glc->log->stream, format, ap);
	fputc('\n', glc->log->stream);

	pthread_mutex_unlock(&glc->log->log_mutex);

	va_end(ap);
}

void glc_log_write_prefix(glc_t *glc, FILE *stream, int level, const char *module)
{
	const char *level_str = NULL;

	/* human-readable msg level */
	switch (level) {
		case GLC_ERROR:
			level_str = "error";
			break;
		case GLC_WARNING:
			level_str = "warning";
			break;
		case GLC_PERFORMANCE:
			level_str = "perf";
			break;
		case GLC_INFORMATION:
			level_str = "info";
			break;
		case GLC_DEBUG:
			level_str = "dbg";
			break;
		default:
			level_str = "unknown";
			break;
	}

	fprintf(stream, "[%7.2fs %10s %5s ] ",
		(double) glc_time(glc) / 1000000000.0, module, level_str);
}

/**  \} */
