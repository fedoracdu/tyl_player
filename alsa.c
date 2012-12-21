#include "alsa.h"

#define	ALSA_BUFFER_SIZE_MAX	(65536)

static snd_pcm_t	*playback_handle;
static snd_pcm_uframes_t buffer_size, period_size;
static unsigned int audio_channels;
static unsigned int audio_sample_rate;
static snd_pcm_format_t audio_format;

static int xrun_recover(int err)
{
	if (err == -EPIPE) {
		err = snd_pcm_prepare(playback_handle);
		if (err < 0) {
			fprintf(stderr, "can NOT recover from underrun (%s)\n",
					snd_strerror(err));
			err = -EIO;
		}
	} else if (err == -ESTRPIPE) {
		fprintf(stderr, "-ESTRPIPE... unsupported!\n");
		err = -1;
	}

	return err;
}

static inline unsigned int get_bytes_per_sample(snd_pcm_format_t format)
{
	unsigned int	ret = 0;

	switch (format) {
	case SND_PCM_FORMAT_U8:
		ret = 1;
		break;
	case SND_PCM_FORMAT_S32_LE:
	case SND_PCM_FORMAT_S32_BE:
		ret = 4;
		break;
	default:
		ret = 2;
		break;
	}

	return ret;
}

int init_alsa(unsigned int channels, unsigned sample_rate,
				 snd_pcm_format_t format)
{
	int	ret;
	snd_pcm_hw_params_t *hw_params;

	playback_handle = NULL;
	ret = snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK,
			0);
	if (ret < 0) {
		fprintf(stderr, "can NOT open soundcard\n");
		goto fail;
	}

	ret = snd_pcm_hw_params_malloc(&hw_params);
	if (ret < 0) {
		fprintf(stderr, "can NOT allocate hardware paramter structure (%s)\n",
				snd_strerror(ret));
		goto fail;
	}

	ret = snd_pcm_hw_params_any(playback_handle, hw_params);
	if (ret < 0) {
		fprintf(stderr, "can NOT initialize hardware paramter structure (%s)\n",
				snd_strerror(ret));
		goto fail;
	}

	ret = snd_pcm_hw_params_set_access(playback_handle, hw_params,
									   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) {
		fprintf(stderr, "can NOT set access type (%s)\n", snd_strerror(ret));
		goto fail;
	}

	ret = snd_pcm_hw_params_set_format(playback_handle, hw_params, format);
	if (ret < 0) {
		fprintf(stderr, "can NOT set sample format (%s)\n", snd_strerror(ret));
		goto fail;
	}

	ret = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params,
										  &sample_rate, 0);
	if (ret < 0) {
		fprintf(stderr, "can NOT set sample rate (%s)\n", snd_strerror(ret));
		goto fail;
	}

	ret = snd_pcm_hw_params_set_channels(playback_handle, hw_params, channels);
	if (ret < 0) {
		fprintf(stderr, "can NOT set channels (%s)\n", snd_strerror(ret));
		goto fail;
	}

	snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_size);
	buffer_size = buffer_size < ALSA_BUFFER_SIZE_MAX ?
				  buffer_size : ALSA_BUFFER_SIZE_MAX;
	ret = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params,
												 &buffer_size);
	if (ret < 0) {
		fprintf(stderr, "can NOT set alsa buffer size (%s)\n", snd_strerror(ret));
		goto fail;
	}

	snd_pcm_hw_params_get_period_size_min(hw_params, &period_size, NULL);
	if (!period_size)
		period_size = buffer_size / 4;
	ret = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params,
												 &period_size, NULL);
	if (ret < 0) {
		fprintf(stderr, "can NOT set alsa period size (%s)\n", snd_strerror(ret));
		goto fail;
	}

	ret = snd_pcm_hw_params(playback_handle, hw_params);
	if (ret < 0) {
		fprintf(stderr, "can NOT set parameters (%s)\n", snd_strerror(ret));
		goto fail;
	}
	
	snd_pcm_hw_params_free(hw_params);

	audio_channels = channels;
	audio_sample_rate = sample_rate;
	audio_format = format;

	ret = 0;

	return ret;
fail:
	if (playback_handle) {
		snd_pcm_close(playback_handle);
		playback_handle = NULL;
	}
	return ret;
}

int close_alsa(void)
{
	if (playback_handle) {
		snd_pcm_close(playback_handle);
		playback_handle = NULL;
	}

	return 0;
}

int write_to_alsa(const void *buf, unsigned int size)
{
	int	ret = 0;

	while (size > 0) {
		ret = snd_pcm_writei(playback_handle, buf, size);
		if (ret == -EAGAIN)
			continue;
		if (ret < 0) {
			if (xrun_recover(ret) < 0) {
				fprintf(stderr, "alsa write error: (%s)\n", snd_strerror(ret));
				break;
			}
		}

		buf += audio_channels * get_bytes_per_sample(audio_format) * ret;
		size -= ret;
	}

	return ret;
}

int pause_alsa(int enable)
{
	if (enable) {
		snd_pcm_drop(playback_handle);
	 } else {
		snd_pcm_prepare(playback_handle);
	 }

	return 0;
}
