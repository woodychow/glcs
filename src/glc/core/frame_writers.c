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

#include <limits.h> // For IOV_MAX
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h> // for writev
#include <glc/common/optimization.h>
#include "frame_writers.h"

typedef struct
{
	struct frame_writer_s writer_base;
	int frame_size;
	int left;
	char *frame_ptr;
} std_frame_writer_t;

static int std_configure(frame_writer_t writer, int r_sz, int h);
static int std_write_init(frame_writer_t writer, char *frame);
static int std_write(frame_writer_t writer, int fd);
static int std_destroy(frame_writer_t writer);

/*
 * opengl buffers have image data from bottom
 * row to top row while video encoders expect the
 * opposite. You can flip the image in the external
 * program but doing it here is more efficient.
 */
typedef struct
{
	struct frame_writer_s writer_base;
	int    frame_size;
	int    left;
	struct iovec *iov;
	size_t iov_capacity;
	unsigned cur_idx;
	int    row_sz;
	int    num_lines;
} invert_frame_writer_t;

static int invert_configure(frame_writer_t writer, int r_sz, int h);
static int invert_write_init(frame_writer_t writer, char *frame);
static int invert_write(frame_writer_t writer, int fd);
static int invert_destroy(frame_writer_t writer);

static write_ops_t std_ops = {
	.configure  = std_configure,
	.write_init = std_write_init,
	.write      = std_write,
	.destroy    = std_destroy,
};

int glcs_std_create( frame_writer_t *writer )
{
	std_frame_writer_t *std_writer = (std_frame_writer_t*)calloc(1,sizeof(std_frame_writer_t));
	*writer = (frame_writer_t)std_writer;
	if (unlikely(!std_writer))
		return ENOMEM;
	std_writer->writer_base.ops = &std_ops;
	return 0;
}

int std_configure(frame_writer_t writer, int r_sz, int h)
{
	std_frame_writer_t *std_writer = (std_frame_writer_t *)writer;
	std_writer->frame_size = r_sz*h;
	return 0;
}

int std_write_init(frame_writer_t writer, char *frame)
{
	std_frame_writer_t *std_writer = (std_frame_writer_t *)writer;
	std_writer->frame_ptr = frame;
	std_writer->left = std_writer->frame_size;
	return std_writer->left;
}

int std_write(frame_writer_t writer, int fd)
{
	std_frame_writer_t *std_writer = (std_frame_writer_t *)writer;
	int ret = write(fd, std_writer->frame_ptr, std_writer->left);
	if (likely(ret >= 0)) {
		std_writer->left      -= ret;
		std_writer->frame_ptr += ret;
		ret = std_writer->left;
	}
	return ret;
}

int std_destroy(frame_writer_t writer)
{
	std_frame_writer_t *std_writer = (std_frame_writer_t *)writer;
	free(std_writer);
	return 0;
}

static write_ops_t invert_ops = {
	.configure  = invert_configure,
	.write_init = invert_write_init,
	.write      = invert_write,
	.destroy    = invert_destroy,
};

int glcs_invert_create( frame_writer_t *writer )
{
	invert_frame_writer_t *invert_writer = (invert_frame_writer_t*)
		calloc(1,sizeof(invert_frame_writer_t));
	*writer = (frame_writer_t)invert_writer;
	if (unlikely(!invert_writer))
		return ENOMEM;
	invert_writer->writer_base.ops = &invert_ops;
	return 0;
}

int invert_configure(frame_writer_t writer, int r_sz, int h)
{
	int i;
	invert_frame_writer_t *invert_writer = (invert_frame_writer_t *)writer;
	if (unlikely(h > invert_writer->iov_capacity)) {
		struct iovec *ptr = (struct iovec *)realloc(invert_writer->iov,
					h*sizeof(struct iovec));
		if (unlikely(!ptr))
			return ENOMEM;
		invert_writer->iov = ptr;
		invert_writer->iov_capacity = h;
	}

	if (unlikely(r_sz != invert_writer->row_sz))
		i = 0;
	else
		i = invert_writer->num_lines;
	for (; i < h; ++i)
		invert_writer->iov[i].iov_len = r_sz;
	invert_writer->row_sz     = r_sz;
	invert_writer->num_lines  = h;
	invert_writer->frame_size = r_sz*h;
	return 0;
}

int invert_write_init(frame_writer_t writer, char *frame)
{
	invert_frame_writer_t *invert_writer = (invert_frame_writer_t *)writer;
	int i;
	frame = &frame[(invert_writer->num_lines-1)*invert_writer->row_sz];
	for (i = 0; i < invert_writer->num_lines; ++i) {
		invert_writer->iov[i].iov_base = frame;
		frame -= invert_writer->row_sz;
	}
	invert_writer->cur_idx = 0;
	invert_writer->left    = invert_writer->frame_size;
	return invert_writer->left;
}

int invert_write(frame_writer_t writer, int fd)
{
	invert_frame_writer_t *invert_writer = (invert_frame_writer_t *)writer;
	int iovcnt;
	int max_write;
	int ret;

	do {
		iovcnt = invert_writer->num_lines - invert_writer->cur_idx;
		if (iovcnt > IOV_MAX)
			iovcnt = IOV_MAX;
		max_write = (iovcnt-1)*invert_writer->row_sz +
			invert_writer->iov[invert_writer->cur_idx].iov_len;
		ret = writev(fd, &invert_writer->iov[invert_writer->cur_idx], iovcnt);

		if (likely(ret >= 0)) {
			int iov_remain;
			int num_written = ret;
			invert_writer->left -= num_written;

			// reset cur_idx iov_len if required.
			if (invert_writer->iov[invert_writer->cur_idx].iov_len !=
			    invert_writer->row_sz &&
			    num_written >= invert_writer->iov[invert_writer->cur_idx].iov_len) {
				num_written -= invert_writer->iov[invert_writer->cur_idx].iov_len;
				invert_writer->iov[invert_writer->cur_idx++].iov_len =
					invert_writer->row_sz;
			}
			if (!invert_writer->left)
				return 0;

			// perform iov adjustments if not over yet.
			invert_writer->cur_idx += num_written/invert_writer->row_sz;
			iov_remain = num_written%invert_writer->row_sz;
			invert_writer->iov[invert_writer->cur_idx].iov_base += iov_remain;
			invert_writer->iov[invert_writer->cur_idx].iov_len  -= iov_remain;
		}
	} while(ret == max_write && invert_writer->left);

	if (ret >= 0)
		ret = invert_writer->left;

	return ret;
}

int invert_destroy(frame_writer_t writer)
{
	invert_frame_writer_t *invert_writer = (invert_frame_writer_t *)writer;
	free(invert_writer->iov);
	free(invert_writer);
	return 0;
}

