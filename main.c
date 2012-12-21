#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ncurses.h>

#include "playback.h"
#include "control.h"

static pthread_t	control_thread;

static int init(void)
{
	int	ret = 0;

	initscr();
	raw();
	noecho();
	cbreak();

	init_libav();

	memset(&control, 0, sizeof(control_t));

	ret = pthread_create(&control_thread, NULL, control_process, NULL);
	if (ret != 0) {
		printw("control thread create failure\n");
		refresh();
		goto fail;
	}
	
	init_sndcard_control();

fail:
	return ret;
}

int main(int argc, const char **argv)
{
	int			i;
	int			ret = -1;

	if (argc < 2) {
		printw("Usage: player <media_filename(s)\n");
		refresh();
		return -1;
	}

	ret = init();
	if (ret != 0)
		return -1;

	for (i = 0;; ) {
		if (!control.pre_next_flg)
			i++;

		if (control.pre_next_flg == 1) {
			control.pre_next_flg = 0;
			if (i > 1) 
				i--;
		}
		if (control.pre_next_flg == 2) {
			control.pre_next_flg = 0;
			if (i < argc - 1)
			i++;
		}

		if (i >= argc)
			break;

		play(argv[i]);
	}

	pthread_cancel(control_thread);

	pthread_join(control_thread, NULL);

	endwin();

	return 0;
}
