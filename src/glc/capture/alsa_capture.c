/**
 * \file glc/capture/alsa_capture.c
 * \brief audio capture
 * \author Pyry Haulos <pyry.haulos@gmail.com>
 * \date 2007-2008
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup alsa_capture
 *  \{
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <packetstream.h>
#include <alsa/asoundlib.h>
#include <pthread.h>

#include <glc/common/glc.h>
#include <glc/common/core.h>
#include <glc/common/log.h>
#include <glc/common/state.h>
#include <glc/common/util.h>

#include "alsa_capture.h"
#include "optimization.h"

struct alsa_capture_s {
	glc_t *glc;
	ps_buffer_t *to;
	glc_message_header_t msg_hdr;
	glc_audio_data_header_t hdr;

	glc_state_audio_t state_audio;

	snd_pcm_t *pcm;
	snd_pcm_uframes_t period_size;

	glc_flags_t flags;
	const char *device;
	unsigned int channels;
	unsigned int rate;
	unsigned int min_periods;
	snd_pcm_format_t format;
	ssize_t bytes_per_frame;
	unsigned rate_usec;
	glc_utime_t delay_usec;

	int interrupt_pipe[2];
	struct pollfd *fds;
	nfds_t nfds;
	nfds_t nfds_capacity;

	pthread_t capture_thread;
	int skip_data;
	int stop_capture;
	int thread_running;
};

static int alsa_capture_open(alsa_capture_t alsa_capture);
static int alsa_capture_init_hw(alsa_capture_t alsa_capture, snd_pcm_hw_params_t *hw_params);
static int alsa_capture_init_sw(alsa_capture_t alsa_capture, snd_pcm_sw_params_t *sw_params);
static int alsa_capture_init_fds(alsa_capture_t alsa_capture);
static int alsa_capture_prepare_fds(alsa_capture_t alsa_capture);
static int alsa_capture_check_state(alsa_capture_t alsa_capture);
static int alsa_capture_pcm_error(alsa_capture_t alsa_capture);
static int alsa_capture_process_pcm(alsa_capture_t alsa_capture, ps_packet_t *packet);
static int alsa_capture_read_pcm(alsa_capture_t alsa_capture, char *dma);
static void *alsa_capture_thread(void *argptr);

static glc_audio_format_t alsa_capture_glc_format(snd_pcm_format_t pcm_fmt);

static int alsa_capture_xrun(alsa_capture_t alsa_capture, int err);
static int alsa_capture_stop(alsa_capture_t alsa_capture);

int alsa_capture_init(alsa_capture_t *alsa_capture, glc_t *glc)
{
	*alsa_capture = (alsa_capture_t) calloc(1, sizeof(struct alsa_capture_s));

	(*alsa_capture)->glc = glc;
	(*alsa_capture)->device = "default";
	(*alsa_capture)->channels = 2;
	(*alsa_capture)->rate = 44100;
	(*alsa_capture)->min_periods = 2;
	glc_state_audio_new((*alsa_capture)->glc, &(*alsa_capture)->hdr.id,
			    &(*alsa_capture)->state_audio);
	(*alsa_capture)->skip_data = 1;
	(*alsa_capture)->interrupt_pipe[0] = -1;
	(*alsa_capture)->interrupt_pipe[1] = -1;

	return 0;
}

int alsa_capture_destroy(alsa_capture_t alsa_capture)
{
	if (unlikely(alsa_capture == NULL))
		return EINVAL;

	alsa_capture->stop_capture = 1;

	if (alsa_capture->thread_running) {
		write(alsa_capture->interrupt_pipe[1],"",1);
		pthread_join(alsa_capture->capture_thread, NULL);
	}

	close(alsa_capture->interrupt_pipe[1]);
	close(alsa_capture->interrupt_pipe[0]);

	free(alsa_capture);
	return 0;
}

int alsa_capture_set_buffer(alsa_capture_t alsa_capture, ps_buffer_t *buffer)
{
	alsa_capture->to = buffer;
	return 0;
}

int alsa_capture_set_device(alsa_capture_t alsa_capture, const char *device)
{
	if (unlikely(alsa_capture->pcm))
		return EALREADY;

	alsa_capture->device = device;
	return 0;
}

int alsa_capture_set_rate(alsa_capture_t alsa_capture, unsigned int rate)
{
	if (unlikely(alsa_capture->pcm))
		return EALREADY;

	alsa_capture->rate = rate;
	return 0;
}

int alsa_capture_set_channels(alsa_capture_t alsa_capture, unsigned int channels)
{
	if (unlikely(alsa_capture->pcm))
		return EALREADY;

	alsa_capture->channels = channels;
	return 0;
}

int alsa_capture_start(alsa_capture_t alsa_capture)
{
	int ret;
	if (unlikely(alsa_capture == NULL))
		return EINVAL;

	if (unlikely(alsa_capture->to == NULL))
		return EAGAIN;

	if (unlikely(!alsa_capture->thread_running)) {
		pipe(alsa_capture->interrupt_pipe);
		glc_util_set_nonblocking(alsa_capture->interrupt_pipe[0]);
		pthread_create(&alsa_capture->capture_thread, &attr,
				alsa_capture_thread, (void *) alsa_capture);
		alsa_capture->thread_running = 1;
	}

	if (likely(alsa_capture->skip_data != 0)) {
		glc_log(alsa_capture->glc, GLC_WARNING, "alsa_capture",
			 "device %s already started", alsa_capture->device);
		alsa_capture->skip_data = 0;
		write(alsa_capture->interrupt_pipe[1],"",1);
	} else
		glc_log(alsa_capture->glc, GLC_INFORMATION, "alsa_capture",
			 "starting device %s", alsa_capture->device);

	return 0;
}

int alsa_capture_stop(alsa_capture_t alsa_capture)
{
	if (unlikely(alsa_capture == NULL))
		return EINVAL;

	if (likely(alsa_capture->skip_data == 0)) {
		glc_log(alsa_capture->glc, GLC_INFORMATION, "alsa_capture",
			 "stopping device %s", alsa_capture->device);
		alsa_capture->skip_data = 1;
		write(alsa_capture->interrupt_pipe[1],"",1);
	} else
		glc_log(alsa_capture->glc, GLC_WARNING, "alsa_capture",
			 "device %s already stopped", alsa_capture->device);

	return 0;
}

int alsa_capture_open(alsa_capture_t alsa_capture)
{
	snd_pcm_hw_params_t *hw_params = NULL;
	snd_pcm_sw_params_t *sw_params = NULL;
	ps_packet_t packet;
	int dir, ret = 0;
	glc_message_header_t msg_hdr;
	glc_audio_format_message_t fmt_msg;

	glc_log(alsa_capture->glc, GLC_DEBUG, "alsa_capture", "opening device %s",
		 alsa_capture->device);

	/* open pcm */
	if (unlikely((ret = snd_pcm_open(&alsa_capture->pcm, alsa_capture->device,
		SND_PCM_STREAM_CAPTURE, 0)) < 0))
		goto err;

	/* init hw */
	snd_pcm_hw_params_alloca(&hw_params);
	if (unlikely((ret = -alsa_capture_init_hw(alsa_capture, hw_params))))
		goto err;

	/* set software params */
	snd_pcm_sw_params_alloca(&sw_params);
	if (unlikely((ret = -alsa_capture_init_sw(alsa_capture, sw_params))))
		goto err;

	/* we need period size */
	if (unlikely((ret = snd_pcm_hw_params_get_period_size(hw_params,
					&alsa_capture->period_size, NULL))))
		goto err;
	alsa_capture->bytes_per_frame = snd_pcm_frames_to_bytes(alsa_capture->pcm, 1);
	alsa_capture->hdr.size = alsa_capture->period_size * alsa_capture->bytes_per_frame;

	/* read actual settings */
	if (unlikely((ret = snd_pcm_hw_params_get_format(hw_params, &alsa_capture->format)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_hw_params_get_rate(hw_params,
							&alsa_capture->rate, &dir)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_hw_params_get_channels(hw_params,
							&alsa_capture->channels)) < 0))
		goto err;

	alsa_capture->rate_usec = 1000000000u / alsa_capture->rate;
	alsa_capture->delay_usec = alsa_capture->period_size * alsa_capture->rate_usec;

	alsa_capture->flags = GLC_AUDIO_INTERLEAVED;

	/* prepare packet */
	fmt_msg.id       = alsa_capture->hdr.id;
	fmt_msg.rate     = alsa_capture->rate;
	fmt_msg.channels = alsa_capture->channels;
	fmt_msg.flags    = alsa_capture->flags;
	fmt_msg.format   = alsa_capture_glc_format(alsa_capture->format);

	if (unlikely(!fmt_msg.format)) {
		glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
			"unsupported audio format 0x%02x", alsa_capture->format);
		return ENOTSUP;
	}

	msg_hdr.type = GLC_MESSAGE_AUDIO_FORMAT;
	ps_packet_init(&packet, alsa_capture->to);
	ps_packet_open(&packet, PS_PACKET_WRITE);
	ps_packet_write(&packet, &msg_hdr, sizeof(glc_message_header_t));
	ps_packet_write(&packet, &fmt_msg, sizeof(glc_audio_format_message_t));
	ps_packet_close(&packet);
	ps_packet_destroy(&packet);

	glc_log(alsa_capture->glc, GLC_DEBUG, "alsa_capture",
		 "success (stream=%d, device=%s, rate=%u, channels=%u)", alsa_capture->id,
		 alsa_capture->device, alsa_capture->rate, alsa_capture->channels);

	return 0;
err:
	glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
		 "initialization failed: %s", snd_strerror(ret));
	return -ret;
}

glc_audio_format_t alsa_capture_glc_format(snd_pcm_format_t pcm_fmt)
{
	switch (pcm_fmt) {
	case SND_PCM_FORMAT_S16_LE:
		return GLC_AUDIO_S16_LE;
	case SND_PCM_FORMAT_S24_LE:
		return GLC_AUDIO_S24_LE;
	case SND_PCM_FORMAT_S32_LE:
		return GLC_AUDIO_S32_LE;
	default:
		return 0;
	}
}

int alsa_capture_init_hw(alsa_capture_t alsa_capture, snd_pcm_hw_params_t *hw_params)
{
	snd_pcm_format_mask_t *formats = NULL;
	snd_pcm_uframes_t max_buffer_size;
	unsigned int min_periods;
	int dir, ret;

	if (unlikely((ret = snd_pcm_hw_params_any(alsa_capture->pcm, hw_params)) < 0))
		goto err;

	/* XXX: Possible enhancement could be to use MMAP access */
	if (unlikely((ret = snd_pcm_hw_params_set_access(alsa_capture->pcm,
					hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0))
		goto err;

	formats = (snd_pcm_format_mask_t *) alloca(snd_pcm_format_mask_sizeof());
	snd_pcm_format_mask_none(formats);
	snd_pcm_format_mask_set(formats, SND_PCM_FORMAT_S16_LE);
	snd_pcm_format_mask_set(formats, SND_PCM_FORMAT_S24_LE);
	snd_pcm_format_mask_set(formats, SND_PCM_FORMAT_S32_LE);

	if (unlikely((ret = snd_pcm_hw_params_set_format_mask(alsa_capture->pcm,
						hw_params, formats)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_hw_params_set_channels(alsa_capture->pcm,
						hw_params, alsa_capture->channels)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_hw_params_set_rate(alsa_capture->pcm,
						hw_params, alsa_capture->rate, 0)) < 0))
		goto err;

	if (unlikely((ret = snd_pcm_hw_params_get_buffer_size_max(hw_params,
						&max_buffer_size)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_hw_params_set_buffer_size(alsa_capture->pcm,
						hw_params, max_buffer_size)) < 0))
		goto err;

	if (unlikely((ret = snd_pcm_hw_params_get_periods_min(hw_params,
						&min_periods, &dir)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_hw_params_set_periods(alsa_capture->pcm, hw_params,
						 min_periods < alsa_capture->min_periods ?
						 alsa_capture->min_periods : min_periods,
						 dir)) < 0))
		goto err;

	if (unlikely((ret = snd_pcm_hw_params(alsa_capture->pcm, hw_params)) < 0))
		goto err;
err:
	return -ret;
}

int alsa_capture_init_sw(alsa_capture_t alsa_capture, snd_pcm_sw_params_t *sw_params)
{
	int ret;

	if (unlikely((ret = snd_pcm_sw_params_current(alsa_capture->pcm, sw_params)) < 0))
		goto err;
	if (unlikely((ret = snd_pcm_sw_params(alsa_capture->pcm, sw_params))))
		goto err;
err:
	return -ret;
}

int alsa_capture_init_fds(alsa_capture_t alsa_capture)
{
	alsa_capture->fds = calloc(3, sizeof(struct pollfd));
	if (unlikely(!alsa_capture->fds))
		return -ENOMEM;
	alsa_capture->nfds_capacity = 3;

	alsa_capture->nfds =1;
	alsa_capture->fds[0].fd = alsa_capture->interrupt_pipe[0];
	alsa_capture->fds[0].events = POLLIN;
	return 0;
}

int alsa_capture_prepare_fds(alsa_capture_t alsa_capture)
{
	int pcm_nfds;
	int ret = 0;
	if (!alsa_capture->skip_data) {
		pcm_nfds = snd_pcm_poll_descriptors_count(alsa_capture->pcm);
		if (pcm_nfds+1 > alsa_capture->nfds_capacity) {
			struct pollfd *ptr = realloc(alsa_capture->fds, (pcm_nfds+1)*sizeof(struct pollfd));
			if (!ptr)
				return -ENOMEM;
			alsa_capture->fds = ptr;
			alsa_capture->nfds_capacity = pcm_nfds+1;
		}
		alsa_capture->nfds = pcm_nfds+1;
		ret = snd_pcm_poll_descriptors(alsa_capture->pcm, &alsa_capture->fds[1], pcm_nfds);
	} else
		alsa_capture->nfds = 1;
	return ret;
}

int alsa_capture_check_state(alsa_capture_t alsa_capture)
{
	int ret = 1;
	if (unlikely(alsa_capture->fds[0].revents & (POLLERR|POLLHUP))) {
		glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
			"pipe error");
		alsa_capture->stop_capture = 1;
	} else if (alsa_capture->fds[0].revents & POLLIN) {
		glc_util_empty_pipe(alsa_capture->fds[0].fd);
	}
	if (likely(!alsa_capture->stop_capture)) {
		if (alsa_capture->skip_data) {
			if (alsa_capture->nfds > 1) {
				snd_pcm_drop(alsa_capture->pcm);
				glc_log(alsa_capture->glc, GLC_INFORMATION,
					"alsa_capture", "snd_pcm_drop()");
			} else
				ret = 0;
		} else {
			if (alsa_capture->nfds == 1) {
				snd_pcm_start(alsa_capture->pcm);
				glc_log(alsa_capture->glc, GLC_INFORMATION,
					"alsa_capture", "snd_pcm_start()");
			} else
				ret = 0;
		}
	}
	return ret;
}

int alsa_capture_pcm_error(alsa_capture_t alsa_capture)
{
	snd_pcm_state_t s = snd_pcm_state(alsa_capture->pcm);
	switch(s) {
	case SND_PCM_STATE_XRUN:
		return -EPIPE;
	case SND_PCM_STATE_SUSPENDED:
		return -ESTRPIPE;
	case SND_PCM_STATE_DISCONNECTED:
		return -ENODEV;
	case SND_PCM_STATE_RUNNING:
		return 0;
	default:
		glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture", "unexpected state: %s",
			snd_pcm_state_name(s));
		return -EIO;
	}
}

int alsa_capture_read_pcm(alsa_capture_t alsa_capture, char *dma)
{
	ssize_t r;
	size_t result = 0;
	size_t count  = alsa_capture->period_size;

	while (count > 0) {
		r = snd_pcm_readi(alsa_capture->pcm,dma,count);
		if (unlikely(r == -EPIPE || r == -ESTRPIPE)) {
			r = -alsa_capture_xrun(alsa_capture,r);
			if (unlikely(r < 0))
				glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
					"xrun recovery failed: %s", snd_strerror(r));
			return r;
		} else if (unlikely(r < 0)) {
			if (unlikely(r != -EINTR))
				return r;
		} else {
			result += r;
			count  -= r;
			dma    += r * alsa_capture->bytes_per_frame;
			if (unlikely(count))
				glc_log(alsa_capture->glc, GLC_WARNING, "alsa_capture",
					"read %ld, expected %zd",
				 	read * alsa_capture->bytes_per_frame,
				 	alsa_capture->hdr.size);
		}
	}
	return result;
}

int alsa_capture_process_pcm(alsa_capture_t alsa_capture, ps_packet_t *packet)
{
	snd_pcm_sframes_t read, avail;
	int ret = 0;
	char *dma;
	unsigned short revents;

	if (unlikely(snd_pcm_poll_descriptors_revents(alsa_capture->pcm, &alsa_capture->fds[1],
						alsa_capture->nfds-1,&revents))) {
		glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
			"snd_pcm_poll_descriptors_revents()");
		return 0;
	}
	if (unlikely(revents & (POLLERR|POLLNVAL))) {
		return alsa_capture_xrun(alsa_capture, alsa_capture_pcm_error(alsa_capture));
	}
	else if (revents & POLLIN) {
		while (unlikely((avail = snd_pcm_avail(alsa_capture->pcm)) == -EINTR));
		if (unlikely(avail < 0))
			return -alsa_capture_xrun(alsa_capture,avail);

		alsa_capture->hdr.time = glc_state_time(alsa_capture->glc);

		if (likely(alsa_capture->delay_usec <= alsa_capture->hdr.time))
			alsa_capture->hdr.time -= alsa_capture->delay_usec;

		if (unlikely((ret = ps_packet_open(&packet, PS_PACKET_WRITE))))
			goto cancel;
		if (unlikely((ret = ps_packet_write(&packet, &alsa_capture->msg_hdr,
					sizeof(glc_message_header_t)))))
			goto cancel;
		if (unlikely((ret = ps_packet_write(&packet, &alsa_capture->hdr,
					sizeof(glc_audio_data_header_t)))))
			goto cancel;
		if (unlikely((ret = ps_packet_dma(&packet, (void *) &dma,
					alsa_capture->hdr.size, PS_ACCEPT_FAKE_DMA))))
			goto cancel;

		if (unlikely((read = alsa_capture_read_pcm(alsa_capture,dma)) != alsa_capture->hdr.size)) {
			if (unlikely(read < 0))
				alsa_capture->stop_capture = 1;
			ret = read;
			goto cancel;
		}

		if (unlikely((ret = ps_packet_setsize(&packet,
						sizeof(glc_message_header_t) +
						sizeof(glc_audio_data_header_t) +
						alsa_capture->hdr.size))))
			goto cancel;

		/* TODO: Once packet size is set I think that you cannot cancel the msg anymore */
		if (unlikely((ret = ps_packet_close(&packet))))
			goto cancel;

		/* just check for xrun */
		if ((ret = snd_pcm_delay(alsa_capture->pcm, &avail)) < 0) {
			alsa_capture_xrun(alsa_capture, ret);
			break;
		}
		return 0;

cancel:
		glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
			"%s (%d)", strerror(ret), ret);
		ps_packet_cancel(&packet)
	}
	return ret;
}

void *alsa_capture_thread(void *argptr)
{
	alsa_capture_t alsa_capture = argptr;
	ps_packet_t packet;
	int ret;

	glc_util_block_signals();

	ps_packet_init(&packet, alsa_capture->to);
	alsa_capture->msg_hdr.type = GLC_MESSAGE_AUDIO_DATA;

	if (unlikely(alsa_capture_open(alsa_capture) < 0))
		goto end;

	if (unlikely(alsa_capture_init_fds(alsa_capture) < 0))
		goto end;

	while (!alsa_capture->stop_capture) {

		if (unlikely(alsa_capture_prepare_fds(alsa_capture) < 0))
			break;

		if (unlikely((ret = poll(alsa_capture->fds, alsa_capture->nfds, -1)) < 0 &&
				errno != EINTR)) {
			glc_log(alsa_capture->glc, GLC_ERROR, "alsa_capture",
				"poll() error: %s (%d)", strerror(errno), errno);
			break;
		}

		if (ret > 0) {
			if (alsa_capture_check_state(alsa_capture))
				continue;
			if (alsa_capture->nfds > 1) {
				alsa_capture_process_pcm(alsa_capture, &packet, &msg_hdr);
			}
		}
	}
end:
	free(alsa_capture->fds);
	/** TODO: snd_pcm_drain() ?
	 */
	snd_pcm_close(alsa_capture->pcm);
	ps_packet_destroy(&packet);
	return NULL;
}

/*
 * The EINTR error is handled outside this function.
 */
int alsa_capture_xrun(alsa_capture_t alsa_capture, int err)
{
	if (err == -EPIPE) {
		glc_log(alsa_capture->glc, GLC_WARNING, "alsa_capture", "overrun");
		if ((err = snd_pcm_prepare(alsa_capture->pcm)) < 0)
			return -err;
		if ((err = snd_pcm_start(alsa_capture->pcm)) < 0)
			return -err;
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(alsa_capture->pcm)) == -EAGAIN)
			sched_yield();
		if (err < 0) {
			if ((err = snd_pcm_prepare(alsa_capture->pcm)) < 0)
				return -err;
			if ((err = snd_pcm_start(alsa_capture->pcm)) < 0)
				return -err;
			return 0;
		}
	}

	return -err;
}

/**  \} */
