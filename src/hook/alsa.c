/**
 * \file hook/alsa.c
 * \brief alsa wrapper adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
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
 * \defgroup alsa alsa wrapper
 *  \{
 */

#include <dlfcn.h>
#include <elfhacks.h>
#include <alsa/asoundlib.h>

#include <glc/common/util.h>
#include <glc/common/core.h>
#include <glc/common/log.h>
#include <glc/capture/alsa_hook.h>
#include <glc/capture/alsa_capture.h>

#include "lib.h"

struct alsa_capture_stream_s {
	alsa_capture_t capture;
	char *device;
	unsigned int channels;
	unsigned int rate;

	struct alsa_capture_stream_s *next;
};

struct alsa_private_s {
	glc_t *glc;
	alsa_hook_t alsa_hook;
	
	int started;
	int capture;
	int capturing;

	struct alsa_capture_stream_s *capture_stream;

	void *libasound_handle;
	alsa_real_api_t api;
};

__PRIVATE struct alsa_private_s alsa;
__PRIVATE int alsa_loaded = 0;

__PRIVATE void get_real_alsa();

__PRIVATE int alsa_parse_capture_cfg(glc_t *glc, const char *cfg);

int alsa_init(glc_t *glc)
{
	alsa.glc = glc;
	alsa.started = alsa.capturing = 0;
	alsa.capture_stream = NULL;
	alsa.alsa_hook = NULL;
	int ret = 0;
	long int captured_stream_num = 0;
	char *env_var;

	glc_log(alsa.glc, GLC_DEBUG, "alsa", "initializing");

	if ((env_var = getenv("GLC_AUDIO")))
		alsa.capture = atoi(env_var);
	else
		alsa.capture = 1;

	/* initialize audio hook system */
	if (alsa.capture) {
		if (unlikely((ret = alsa_hook_init(&alsa.alsa_hook, alsa.glc))))
			return ret;

		alsa_hook_allow_skip(alsa.alsa_hook, 0);
		if ((env_var = getenv("GLC_AUDIO_SKIP")))
			alsa_hook_allow_skip(alsa.alsa_hook, atoi(env_var));
	}

	if ((env_var = getenv("GLC_AUDIO_RECORD")))
		captured_stream_num = alsa_parse_capture_cfg(glc, env_var);

	get_real_alsa();

	/* make sure libasound.so does not call our hooked functions */
	alsa_unhook_so("*libasound.so*");

	glc_account_threads(alsa.glc, 1+(alsa.capture != 0)+captured_stream_num, 0);

	return 0;
}

int alsa_parse_capture_cfg(glc_t *glc, const char *cfg)
{
	struct alsa_capture_stream_s *stream;
	const char *args, *next, *device = cfg;
	unsigned int channels, rate;
	size_t len;
	int ret = 0;

	while (device != NULL) {
		while (*device == ';')
			device++;
		if (*device == '\0')
			break;

		channels = 2;
		rate = 44100;

		/* check if some args have been given */
		if ((args = strstr(device, "#")))
			sscanf(args, "#%u#%u", &rate, &channels);
		next = strstr(device, ";");

		stream = (struct alsa_capture_stream_s *)
			calloc(1, sizeof(struct alsa_capture_stream_s));

		if (args)
			len = args - device;
		else if (next)
			len = next - device;
		else
			len = strlen(device);

		stream->device = (char *) malloc(sizeof(char) * (len+1));
		memcpy(stream->device, device, len);
		stream->device[len] = '\0';

		stream->channels = channels;
		stream->rate = rate;
		stream->next = alsa.capture_stream;
		alsa.capture_stream = stream;

		glc_log(glc, GLC_INFORMATION, "alsa",
			"capturing device %s with %u channels at %u",
			stream->device, stream->channels, stream->rate);

		device = next;
		++ret;
	}

	return ret;
}

int alsa_start(ps_buffer_t *buffer)
{
	struct alsa_capture_stream_s *stream = alsa.capture_stream;
	int ret;

	if (alsa.started)
		return EINVAL;

	if (alsa.alsa_hook) {
		if (unlikely((ret = alsa_hook_set_buffer(alsa.alsa_hook, buffer))))
			return ret;
	}

	/* start capture streams */
	while (stream != NULL) {
		alsa_capture_init(&stream->capture, alsa.glc, &alsa.api);
		alsa_capture_set_buffer(stream->capture, buffer);
		alsa_capture_set_device(stream->capture, stream->device);
		alsa_capture_set_rate(stream->capture, stream->rate);
		alsa_capture_set_channels(stream->capture, stream->channels);

		stream = stream->next;
	}

	alsa.started = 1;
	return 0;
}

int alsa_close()
{
	struct alsa_capture_stream_s *del;

	if (!alsa.started)
		return 0;

	glc_log(alsa.glc, GLC_DEBUG, "alsa", "closing");

	if (alsa.capture) {
		alsa.capture = 0; /* disable capturing */
		if (alsa.capturing)
			alsa_hook_stop(alsa.alsa_hook);
		alsa_hook_destroy(alsa.alsa_hook);
	}

	while (alsa.capture_stream != NULL) {
		del = alsa.capture_stream;
		alsa.capture_stream = alsa.capture_stream->next;

		if (del->capture)
			alsa_capture_destroy(del->capture);

		free(del->device);
		free(del);
	}

	return 0;
}

int alsa_capture_stop_all()
{
	struct alsa_capture_stream_s *stream = alsa.capture_stream;

	if (!alsa.capturing)
		return 0;

	while (stream != NULL) {
		if (stream->capture)
			alsa_capture_stop(stream->capture);
		stream = stream->next;
	}

	if (alsa.capture)
		alsa_hook_stop(alsa.alsa_hook);

	alsa.capturing = 0;
	return 0;
}

int alsa_capture_start_all()
{
	struct alsa_capture_stream_s *stream = alsa.capture_stream;

	if (alsa.capturing)
		return 0;

	while (stream != NULL) {
		if (stream->capture)
			alsa_capture_start(stream->capture);
		stream = stream->next;
	}

	if (alsa.capture)
		alsa_hook_start(alsa.alsa_hook);

	alsa.capturing = 1;
	return 0;
}

void get_real_alsa()
{
	if (!lib.dlopen)
		get_real_dlsym();

	if (alsa_loaded)
		return;

	alsa.libasound_handle = lib.dlopen("libasound.so.2", RTLD_LAZY);
	if (unlikely(!alsa.libasound_handle))
		goto err;

	alsa.api.snd_pcm_open =
	  (int (*)(snd_pcm_t **, const char *, snd_pcm_stream_t, int))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_open");
	if (unlikely(!alsa.api.snd_pcm_open))
		goto err;

	alsa.api.snd_pcm_hw_params =
	  (int (*)(snd_pcm_t *, snd_pcm_hw_params_t *))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_hw_params");
	if (unlikely(!alsa.api.snd_pcm_hw_params))
		goto err;

	alsa.api.snd_pcm_open_lconf =
	  (int (*)(snd_pcm_t **, const char *, snd_pcm_stream_t, int, snd_config_t *))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_open_lconf");
	if (unlikely(!alsa.api.snd_pcm_open_lconf))
		goto err;

	alsa.api.snd_pcm_close =
	  (int (*)(snd_pcm_t *))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_close");
	if (!alsa.api.snd_pcm_close)
		goto err;

	alsa.api.snd_pcm_writei =
	  (snd_pcm_sframes_t (*)(snd_pcm_t *, const void *, snd_pcm_uframes_t))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_writei");
	if (unlikely(!alsa.api.snd_pcm_writei))
		goto err;

	alsa.api.snd_pcm_writen =
	  (snd_pcm_sframes_t (*)(snd_pcm_t *, void **, snd_pcm_uframes_t))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_writen");
	if (unlikely(!alsa.api.snd_pcm_writen))
		goto err;

	alsa.api.snd_pcm_mmap_writei =
	  (snd_pcm_sframes_t (*)(snd_pcm_t *, const void *, snd_pcm_uframes_t))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_mmap_writei");
	if (unlikely(!alsa.api.snd_pcm_writei))
		goto err;

	alsa.api.snd_pcm_mmap_writen =
	  (snd_pcm_sframes_t (*)(snd_pcm_t *, void **, snd_pcm_uframes_t))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_mmap_writen");
	if (unlikely(!alsa.api.snd_pcm_writen))
		goto err;

	alsa.api.snd_pcm_mmap_begin =
	  (int (*)(snd_pcm_t *, const snd_pcm_channel_area_t **, snd_pcm_uframes_t *,
		   snd_pcm_uframes_t *))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_mmap_begin");
	if (unlikely(!alsa.api.snd_pcm_mmap_begin))
		goto err;

	alsa.api.snd_pcm_mmap_commit =
	  (snd_pcm_sframes_t (*)(snd_pcm_t *, snd_pcm_uframes_t, snd_pcm_uframes_t))
	    lib.dlsym(alsa.libasound_handle, "snd_pcm_mmap_commit");
	if (unlikely(!alsa.api.snd_pcm_mmap_commit))
		goto err;

	alsa_loaded = 1;
	return;
err:
	fprintf(stderr, "(glc) can't get real alsa");
	exit(1);
}

int alsa_unhook_so(const char *soname)
{
	int ret;
	eh_obj_t so;

	if (!alsa_loaded)
		get_real_alsa(); /* make sure we have real functions */

	if (unlikely((ret = eh_find_obj(&so, soname))))
		return ret;

	/* don't look at 'elfhacks'... contains some serious black magic */
	eh_set_rel(&so, "snd_pcm_open", alsa.api.snd_pcm_open);
	eh_set_rel(&so, "snd_pcm_open_lconf", alsa.api.snd_pcm_open_lconf);
	eh_set_rel(&so, "snd_pcm_close", alsa.api.snd_pcm_close);
	eh_set_rel(&so, "snd_pcm_hw_params", alsa.api.snd_pcm_hw_params);
	eh_set_rel(&so, "snd_pcm_writei", alsa.api.snd_pcm_writei);
	eh_set_rel(&so, "snd_pcm_writen", alsa.api.snd_pcm_writen);
	eh_set_rel(&so, "snd_pcm_mmap_writei", alsa.api.snd_pcm_mmap_writei);
	eh_set_rel(&so, "snd_pcm_mmap_writen", alsa.api.snd_pcm_mmap_writen);
	eh_set_rel(&so, "snd_pcm_mmap_begin", alsa.api.snd_pcm_mmap_begin);
	eh_set_rel(&so, "snd_pcm_mmap_commit", alsa.api.snd_pcm_mmap_commit);
	eh_set_rel(&so, "dlsym", lib.dlsym);
	eh_set_rel(&so, "dlvsym", lib.dlvsym);

	eh_destroy_obj(&so);

	return 0;
}

__PUBLIC int snd_pcm_open(snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream, int mode)
{
	return __alsa_snd_pcm_open(pcmp, name, stream, mode);
}

int __alsa_snd_pcm_open(snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream, int mode)
{
	/* it is not necessarily safe to call glc_init() from write funcs
	   especially async mode (initiated from signal) is troublesome */
	INIT_GLC
	int ret = alsa.api.snd_pcm_open(pcmp, name, stream, mode);
	if ((alsa.capture) && (ret == 0))
		alsa_hook_open(alsa.alsa_hook, *pcmp, name, stream, mode);
	return ret;
}

__PUBLIC int snd_pcm_open_lconf(snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream,
				int mode, snd_config_t *lconf)
{
	return __alsa_snd_pcm_open_lconf(pcmp, name, stream, mode, lconf);
}

int __alsa_snd_pcm_open_lconf(snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream,
			      int mode, snd_config_t *lconf)
{
	INIT_GLC
	int ret = alsa.api.snd_pcm_open_lconf(pcmp, name, stream, mode, lconf);
	if ((alsa.capture) && (ret == 0))
		alsa_hook_open(alsa.alsa_hook, *pcmp, name, stream, mode);
	return ret;
}

__PUBLIC int snd_pcm_close(snd_pcm_t *pcm)
{
	return __alsa_snd_pcm_close(pcm);
}

int __alsa_snd_pcm_close(snd_pcm_t *pcm)
{
	INIT_GLC
	int ret = alsa.api.snd_pcm_close(pcm);
	if ((alsa.capture) && (ret == 0))
		alsa_hook_close(alsa.alsa_hook, pcm);
	return ret;
}

__PUBLIC int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return __alsa_snd_pcm_hw_params(pcm, params);
}

int __alsa_snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	INIT_GLC
	int ret = alsa.api.snd_pcm_hw_params(pcm, params);
	if ((alsa.capture) && (ret == 0))
		alsa_hook_hw_params(alsa.alsa_hook, pcm, params);
	return ret;
}

__PUBLIC snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	return __alsa_snd_pcm_writei(pcm, buffer, size);
}

snd_pcm_sframes_t __alsa_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	INIT_GLC
	snd_pcm_sframes_t ret = alsa.api.snd_pcm_writei(pcm, buffer, size);
	if ((alsa.capture) && (ret > 0) && alsa.capturing)
		alsa_hook_writei(alsa.alsa_hook, pcm, buffer, ret);
	return ret;
}

__PUBLIC snd_pcm_sframes_t snd_pcm_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	return __alsa_snd_pcm_writen(pcm, bufs, size);
}

snd_pcm_sframes_t __alsa_snd_pcm_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	INIT_GLC
	snd_pcm_sframes_t ret = alsa.api.snd_pcm_writen(pcm, bufs, size);
	if (alsa.capture && (ret > 0))
		alsa_hook_writen(alsa.alsa_hook, pcm, bufs, ret);
	return ret;
}

__PUBLIC snd_pcm_sframes_t snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	return __alsa_snd_pcm_mmap_writei(pcm, buffer, size);
}

snd_pcm_sframes_t __alsa_snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	INIT_GLC
	snd_pcm_sframes_t ret = alsa.api.snd_pcm_mmap_writei(pcm, buffer, size);
	if (alsa.capture && (ret > 0))
		alsa_hook_writei(alsa.alsa_hook, pcm, buffer, ret);
	return ret;
}

__PUBLIC snd_pcm_sframes_t snd_pcm_mmap_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	return __alsa_snd_pcm_mmap_writen(pcm, bufs, size);
}

snd_pcm_sframes_t __alsa_snd_pcm_mmap_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	INIT_GLC
	snd_pcm_sframes_t ret = alsa.api.snd_pcm_mmap_writen(pcm, bufs, size);
	if ((alsa.capture) && (ret > 0))
		alsa_hook_writen(alsa.alsa_hook, pcm, bufs, ret);
	return ret;
}

__PUBLIC int snd_pcm_mmap_begin(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas,
				snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames)
{
	return __alsa_snd_pcm_mmap_begin(pcm, areas, offset, frames);
}

int __alsa_snd_pcm_mmap_begin(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas,
				snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames)
{
	INIT_GLC
	int ret = alsa.api.snd_pcm_mmap_begin(pcm, areas, offset, frames);
	if (alsa.capture && (ret >= 0))
		alsa_hook_mmap_begin(alsa.alsa_hook, pcm, *areas, *offset, *frames);
	return ret;
}

__PUBLIC snd_pcm_sframes_t snd_pcm_mmap_commit(snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
{
	return __alsa_snd_pcm_mmap_commit(pcm, offset, frames);
}

snd_pcm_sframes_t __alsa_snd_pcm_mmap_commit(snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames)
{
	INIT_GLC
	snd_pcm_uframes_t ret;
	if (alsa.capture)
		alsa_hook_mmap_commit(alsa.alsa_hook, pcm, offset,  frames);

	ret = alsa.api.snd_pcm_mmap_commit(pcm, offset, frames);
	if (ret != frames)
		glc_log(alsa.glc, GLC_WARNING, "alsa", "frames=%lu, ret=%ld", frames, ret);
	return ret;
}

/**  \} */
/**  \} */
