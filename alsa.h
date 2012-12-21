#ifndef _ALSA_H_
#define _ALSA_H_

#define	CONFIG_ALSA	(1)

#include <alsa/asoundlib.h>

int init_alsa(unsigned int channels, unsigned int sample_rate,
			  snd_pcm_format_t format);

int close_alsa(void);

int write_to_alsa(const void *buf, unsigned int size);

int pause_alsa(int enable);

#endif
