#ifndef _SNDCARD_H_
#define _SNDCARD_H_

#include "alsa.h"

typedef struct {
	unsigned int		sample_rate;
	unsigned int		channel_layout;
	unsigned int		channels;
	enum AVSampleFormat av_format;
#ifdef CONFIG_ALSA
	snd_pcm_format_t	alsa_format;
#endif
} audio_param_s;

typedef int (*pause_resume)(int enable);
typedef int (*write_pcm)(const void *buf, unsigned int size);
typedef int (*close_pcm)(void);

typedef struct {
	pause_resume	pause_sndcard;
	write_pcm		write_sndcard;
	close_pcm		close_sndcard;
} pcm_control;

audio_param_s	audio_params_t;

int init_sndcard(unsigned int sample_rate, unsigned int channels,
				  unsigned int channel_layout, enum AVSampleFormat format);

void write_sndcard(const void *buf, unsigned int size);
void close_sndcard(void);
void pause_sndcard(int enable);

#endif

