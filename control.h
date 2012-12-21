#ifndef _CONTROL_H_
#define _CONTROL_H_

typedef struct {
	unsigned char begin_flg;
	unsigned char end_flg;
	unsigned char pause_flg;
	unsigned char lrc_flg;
	unsigned char seek_flg;
	unsigned char pre_next_flg;
	unsigned char volume_flg;
} control_t;

control_t	control;

void *control_process(void *arg);

extern void init_sndcard_control(void);

#endif
