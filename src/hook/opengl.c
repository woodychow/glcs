/**
 * \file hook/opengl.c
 * \brief opengl wrapper adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
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
 * \addtogroup hook
 *  \{
 * \defgroup opengl OpenGL wrapper
 *  \{
 */

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <glc/common/glc.h>
#include <glc/common/core.h>
#include <glc/common/log.h>
#include <glc/common/util.h>
#include <glc/core/scale.h>
#include <glc/core/ycbcr.h>
#include <glc/capture/gl_capture.h>

#include "lib.h"

#define CS_BGR 0
#define CS_YCBCR_420JPEG 1
#define CS_BGRA 2

struct opengl_private_s {
	glc_t *glc;

	gl_capture_t gl_capture;
	ycbcr_t ycbcr;
	scale_t scale;

	ps_buffer_t *unscaled, *buffer;
	size_t unscaled_size;

	void *libGL_handle;
	void (*glXSwapBuffers)(Display *dpy, GLXDrawable drawable);
	void (*glFinish)(void);
	__GLXextFuncPtr (*glXGetProcAddressARB)(const GLubyte *);
	GLXWindow (*glXCreateWindow)(Display *, GLXFBConfig, Window, const int *);

	int capture_glfinish;
	int colorspace;
	double scale_factor;
	GLenum read_buffer;
	double fps;

	int started;
	int capturing;
};

__PRIVATE struct opengl_private_s opengl;

__PRIVATE void get_real_opengl();
__PRIVATE void opengl_capture_current();
__PRIVATE void opengl_draw_indicator();

int opengl_init(glc_t *glc)
{
	int ret = 0;
	unsigned int x, y, w, h;
	char *env_val;

	opengl.glc              = glc;
	opengl.buffer = opengl.unscaled = NULL;
	opengl.started          = 0;
	opengl.scale_factor     = 1.0;
	opengl.capture_glfinish = 0;
	opengl.read_buffer      = GL_FRONT;
	opengl.capturing        = 0;

	glc_log(opengl.glc, GLC_DEBUG, "opengl", "initializing");

	/* initialize gl_capture object */
	if (unlikely((ret = gl_capture_init(&opengl.gl_capture, opengl.glc))))
		return ret;

	/* load environment variables */
	opengl.fps = 30.0;
	if ((env_val = getenv("GLC_FPS")))
		opengl.fps = atof(env_val);
	glc_util_info_fps(opengl.glc, opengl.fps);
	gl_capture_set_fps(opengl.gl_capture, opengl.fps);

	if ((env_val = getenv("GLC_COLORSPACE"))) {
		if (!strcmp(env_val, "420jpeg"))
			opengl.colorspace = CS_YCBCR_420JPEG;
		else if (!strcmp(env_val, "bgr"))
			opengl.colorspace = CS_BGR;
		else if (!strcmp(env_val, "bgra"))
			opengl.colorspace = CS_BGRA;
		else
			glc_log(opengl.glc, GLC_WARN, "opengl",
				 "unknown colorspace '%s'", env_val);
	} else
		opengl.colorspace = CS_YCBCR_420JPEG;

	if ((env_val = getenv("GLC_UNSCALED_BUFFER_SIZE")))
		opengl.unscaled_size = atoi(env_val) * 1024 * 1024;
	else
		opengl.unscaled_size = 1024 * 1024 * 25;

	if ((env_val = getenv("GLC_CAPTURE"))) {
		if (!strcmp(env_val, "front"))
			opengl.read_buffer = GL_FRONT;
		else if (!strcmp(env_val, "back"))
			opengl.read_buffer = GL_BACK;
		else
			glc_log(opengl.glc, GLC_WARN, "opengl",
				 "unknown capture buffer '%s'", env_val);
	}
	gl_capture_set_read_buffer(opengl.gl_capture, opengl.read_buffer);

	if ((env_val = getenv("GLC_CAPTURE_GLFINISH")))
		opengl.capture_glfinish = atoi(env_val);

	if ((env_val = getenv("GLC_SCALE")))
		opengl.scale_factor = atof(env_val);

	if ((env_val = getenv("GLC_TRY_PBO")))
		gl_capture_try_pbo(opengl.gl_capture, atoi(env_val));

	gl_capture_set_pack_alignment(opengl.gl_capture, 8);
	if ((env_val = getenv("GLC_CAPTURE_DWORD_ALIGNED"))) {
		if (!atoi(env_val))
			gl_capture_set_pack_alignment(opengl.gl_capture, 1);
	}

	if ((env_val = getenv("GLC_CROP"))) {
		w = h = x = y = 0;

		/* we need at least 2 values, width and height */
		if (sscanf(env_val, "%ux%u+%u+%u",
			   &w, &h, &x, &y) >= 2)
			gl_capture_crop(opengl.gl_capture, x, y, w, h);
	}

	gl_capture_draw_indicator(opengl.gl_capture, 0);
	if ((env_val = getenv("GLC_INDICATOR")))
		gl_capture_draw_indicator(opengl.gl_capture, atoi(env_val));

	gl_capture_lock_fps(opengl.gl_capture, 0);
	if ((env_val = getenv("GLC_LOCK_FPS")))
		gl_capture_lock_fps(opengl.gl_capture, atoi(env_val));

	get_real_opengl();
	/* Count host app rendering thread and possible filter threads on glcs side */
	glc_account_threads(opengl.glc, 1, (opengl.scale_factor != 1.0) ||
					   opengl.colorspace == CS_YCBCR_420JPEG);
	return 0;
}

int opengl_start(ps_buffer_t *buffer)
{
	if (unlikely(opengl.started))
		return EINVAL;

	opengl.buffer = buffer;

	/* init unscaled buffer if it is needed */
	if ((opengl.scale_factor != 1.0) || opengl.colorspace == CS_YCBCR_420JPEG) {
		/* if scaling is enabled, it is faster to capture as GL_BGRA */
		gl_capture_set_pixel_format(opengl.gl_capture, GL_BGRA);

		ps_bufferattr_t attr;
		ps_bufferattr_init(&attr);
		if (glc_log_get_level(opengl.glc) >= GLC_PERF)
			ps_bufferattr_setflags(&attr, PS_BUFFER_STATS);

		ps_bufferattr_setsize(&attr, opengl.unscaled_size);
		opengl.unscaled = (ps_buffer_t *) malloc(sizeof(ps_buffer_t));
		ps_buffer_init(opengl.unscaled, &attr);

		if (opengl.colorspace == CS_YCBCR_420JPEG) {
			ycbcr_init(&opengl.ycbcr, opengl.glc);
			ycbcr_set_scale(opengl.ycbcr, opengl.scale_factor);
			ycbcr_process_start(opengl.ycbcr, opengl.unscaled, buffer);
		} else {
			scale_init(&opengl.scale, opengl.glc);
			scale_set_scale(opengl.scale, opengl.scale_factor);
			scale_process_start(opengl.scale, opengl.unscaled, buffer);
		}

		gl_capture_set_buffer(opengl.gl_capture, opengl.unscaled);
	} else {
		gl_capture_set_pixel_format(opengl.gl_capture,
					    opengl.colorspace==CS_BGR?GL_BGR:GL_BGRA);
		gl_capture_set_buffer(opengl.gl_capture, opengl.buffer);
	}

	opengl.started = 1;
	return 0;
}

int opengl_close()
{
	int ret;
	ps_stats_t stats;
	if (!opengl.started)
		return 0;

	glc_log(opengl.glc, GLC_DEBUG, "opengl", "closing");

	if (opengl.capturing)
		gl_capture_stop(opengl.gl_capture);
	gl_capture_destroy(opengl.gl_capture);

	if (opengl.unscaled) {
		if (lib.running) {
			if (unlikely((ret = glc_util_write_end_of_stream(opengl.glc,
									 opengl.unscaled)))) {
				glc_log(opengl.glc, GLC_ERROR, "opengl",
					"can't write end of stream: %s (%d)",
					strerror(ret), ret);
				return ret;
			}
		} else
			ps_buffer_cancel(opengl.unscaled);

		if (opengl.colorspace == CS_YCBCR_420JPEG) {
			ycbcr_process_wait(opengl.ycbcr);
			ycbcr_destroy(opengl.ycbcr);
		} else {
			scale_process_wait(opengl.scale);
			scale_destroy(opengl.scale);
		}
	} else if (lib.running) {
		if (unlikely((ret = glc_util_write_end_of_stream(opengl.glc, opengl.buffer)))) {
			glc_log(opengl.glc, GLC_ERROR, "opengl",
				"can't write end of stream: %s (%d)", strerror(ret), ret);
			return ret;
		}
	} else
		ps_buffer_cancel(opengl.buffer);

	if (opengl.unscaled) {
		if(!ps_buffer_stats(opengl.unscaled, &stats)) {
			glc_log(opengl.glc, GLC_PERF, "opengl", "unscale buffer stats:");
			ps_stats_text(&stats, glc_log_get_stream(opengl.glc));
		}
		ps_buffer_destroy(opengl.unscaled);
		free(opengl.unscaled);
	}

	return 0;
}

int opengl_push_message(glc_message_header_t *hdr, void *message, size_t message_size)
{
	ps_packet_t packet;
	int ret = 0;
	ps_buffer_t *to;
	if (unlikely(!lib.running))
		return EAGAIN;

	if (opengl.unscaled)
		to = opengl.unscaled;
	else
		to = opengl.buffer;

	if (unlikely((ret = ps_packet_init(&packet, to))))
		goto finish;
	if (unlikely((ret = ps_packet_open(&packet, PS_PACKET_WRITE))))
		goto finish;
	if (unlikely((ret = ps_packet_write(&packet, hdr, sizeof(glc_message_header_t)))))
		goto finish;
	if (unlikely((ret = ps_packet_write(&packet, message, message_size))))
		goto finish;
	if (unlikely((ret = ps_packet_close(&packet))))
		goto finish;
	if (unlikely((ret = ps_packet_destroy(&packet))))
		goto finish;

finish:
	return ret;
}

int opengl_capture_start()
{
	int ret;
	if (opengl.capturing)
		return 0;

	if (likely(!(ret = gl_capture_start(opengl.gl_capture))))
		opengl.capturing = 1;

	return ret;
}

int opengl_capture_stop()
{
	int ret;
	if (!opengl.capturing)
		return 0;

	if (likely(!(ret = gl_capture_stop(opengl.gl_capture))))
		opengl.capturing = 0;

	return ret;
}

int opengl_refresh_color_correction()
{
	return gl_capture_refresh_color_correction(opengl.gl_capture);
}

void get_real_opengl()
{
	if (!lib.dlopen)
		get_real_dlsym();

	opengl.libGL_handle = lib.dlopen("libGL.so.1", RTLD_LAZY);
	if (unlikely(!opengl.libGL_handle))
		goto err;
	opengl.glXSwapBuffers =
	  (void (*)(Display *, GLXDrawable))
	    lib.dlsym(opengl.libGL_handle, "glXSwapBuffers");
	if (unlikely(!opengl.glXSwapBuffers))
		goto err;
	opengl.glFinish =
	  (void (*)(void))
	    lib.dlsym(opengl.libGL_handle, "glFinish");
	if (unlikely(!opengl.glFinish))
		goto err;
	opengl.glXGetProcAddressARB =
	  (__GLXextFuncPtr (*)(const GLubyte *))
	    lib.dlsym(opengl.libGL_handle, "glXGetProcAddressARB");
	if (unlikely(!opengl.glXGetProcAddressARB))
		goto err;
	opengl.glXCreateWindow =
	  (GLXWindow (*)(Display *dpy, GLXFBConfig, Window, const int *))
	    lib.dlsym(opengl.libGL_handle, "glXCreateWindow");
	return;
err:
	fprintf(stderr, "(glc) can't get real OpenGL\n");
	exit(1);
}

__PUBLIC GLXextFuncPtr glXGetProcAddressARB(const GLubyte *proc_name)
{
	return __opengl_glXGetProcAddressARB(proc_name);
}

GLXextFuncPtr __opengl_glXGetProcAddressARB(const GLubyte *proc_name)
{
	INIT_GLC

	GLXextFuncPtr ret = (GLXextFuncPtr) wrapped_func((char *) proc_name);
	if (ret)
		return ret;

	return opengl.glXGetProcAddressARB(proc_name);
}

__PUBLIC void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
	return __opengl_glXSwapBuffers(dpy, drawable);
}

void __opengl_glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
	INIT_GLC

	/* both flags shouldn't be defined */
	if (opengl.read_buffer == GL_FRONT)
		opengl.glXSwapBuffers(dpy, drawable);

	gl_capture_frame(opengl.gl_capture, dpy, drawable);

	if (opengl.read_buffer == GL_BACK)
		opengl.glXSwapBuffers(dpy, drawable);
}

__PUBLIC void glFinish(void)
{
	__opengl_glFinish();
}

void __opengl_glFinish(void)
{
	INIT_GLC

	opengl.glFinish();
	if (opengl.capture_glfinish)
		opengl_capture_current();
}

__PUBLIC GLXWindow glXCreateWindow(Display *dpy, GLXFBConfig config, Window win,
				const int *attrib_list)
{
	return __opengl_glXCreateWindow(dpy, config, win, attrib_list);
}

GLXWindow __opengl_glXCreateWindow(Display *dpy, GLXFBConfig config, Window win,
				const int *attrib_list)
{
	INIT_GLC

	if (unlikely(!opengl.glXCreateWindow)) {
		glc_log(opengl.glc, GLC_ERROR, "opengl",
			"glXCreateWindow() not supported");
		return (GLXWindow) 0;
	}

	start_glc(); /* gl_capture must be properly initialized */
	GLXWindow retWin = opengl.glXCreateWindow(dpy, config, win, attrib_list);
	if (retWin)
		gl_capture_set_attribute_window(opengl.gl_capture, dpy,
						(GLXDrawable) retWin, win);
	return retWin;
}

void opengl_capture_current()
{
	INIT_GLC

	Display *dpy = glXGetCurrentDisplay();
	GLXDrawable drawable = glXGetCurrentDrawable();

	if ((dpy != NULL) && (drawable != None))
		gl_capture_frame(opengl.gl_capture, dpy, drawable);
}


/**  \} */
/**  \} */

