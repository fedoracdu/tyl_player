#include <libavutil/samplefmt.h>

#include "sndcard.h"

audio_param_s	audio_params_t;

static pcm_control	sndcard_control;

static inline char litten_endian(void)
{
	int	a = 0x01;
	char *b = (char *)&a;

	return *b;
}

#ifdef CONFIG_ALSA
static snd_pcm_format_t get_alsa_format(enum AVSampleFormat format)
{
	int	flg;
	snd_pcm_format_t ret;

	flg = litten_endian();

	switch (format) {
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		ret = SND_PCM_FORMAT_U8;
		break;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		if (flg)
			ret = SND_PCM_FORMAT_S16_LE;
		else
			ret = SND_PCM_FORMAT_S16_BE;
		break;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		if (flg)
			ret = SND_PCM_FORMAT_S32_LE;
		else
			ret = SND_PCM_FORMAT_S32_BE;
		break;
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
		if (flg)
			ret = SND_PCM_FORMAT_FLOAT_LE;
		else
			ret = SND_PCM_FORMAT_FLOAT_BE;
		break;
	case AV_SAMPLE_FMT_DBL:
	case AV_SAMPLE_FMT_DBLP:
		if (flg)
			ret = SND_PCM_FORMAT_FLOAT64_LE;
		else
			ret = SND_PCM_FORMAT_FLOAT64_BE;
		break;
	default:
		if (flg)
			ret = SND_PCM_FORMAT_S16_LE;
		else
			ret = SND_PCM_FORMAT_S16_BE;
		break;
	}

	return ret;
}
#endif

static enum AVSampleFormat convert_to_no_planar(enum AVSampleFormat format)
{
	enum AVSampleFormat ret;

	switch (format) {
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		ret = AV_SAMPLE_FMT_U8;
		break;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		ret = AV_SAMPLE_FMT_S16;
		break;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		ret = AV_SAMPLE_FMT_S32;
		break;
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
		ret = AV_SAMPLE_FMT_FLT;
		break;
	case AV_SAMPLE_FMT_DBL:
	case AV_SAMPLE_FMT_DBLP:
		ret = AV_SAMPLE_FMT_DBL;
	default:
		ret = AV_SAMPLE_FMT_S16;
	}

	return ret;
}

void init_sndcard_control(void)
{
	memset(&sndcard_control, 0, sizeof(sndcard_control));
#ifdef CONFIG_ALSA
	sndcard_control.write_sndcard = write_to_alsa; 
	sndcard_control.close_sndcard = close_alsa;
	sndcard_control.pause_sndcard = pause_alsa;
#endif
	return ;
}

int init_sndcard(unsigned int sample_rate, unsigned int channels,
				  unsigned int channel_layout, enum AVSampleFormat format)
{
	int	ret;
	if (format == AV_SAMPLE_FMT_NONE)
		return -1;

	memset(&audio_params_t, 0, sizeof(audio_param_s));

	audio_params_t.sample_rate = sample_rate;
	audio_params_t.channel_layout = channel_layout;
	audio_params_t.channels = channels;
	audio_params_t.av_format = convert_to_no_planar(format);
#ifdef CONFIG_ALSA
	audio_params_t.alsa_format = get_alsa_format(format);

	ret = init_alsa(audio_params_t.channels, audio_params_t.sample_rate,
			  audio_params_t.alsa_format);
#endif

	return ret;
}

void close_sndcard(void)
{
	if (sndcard_control.close_sndcard)
		sndcard_control.close_sndcard();
}

void write_sndcard(const void *buf, unsigned int size)
{
	if (sndcard_control.write_sndcard)
		sndcard_control.write_sndcard(buf, size);
}

void pause_sndcard(int enable)
{
	if (sndcard_control.pause_sndcard)
		sndcard_control.pause_sndcard(enable);
}
