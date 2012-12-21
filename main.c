#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#include "playback.h"
#include "control.h"

static struct termios saved_attr;
static pthread_t	control_thread;

static void reset_input_mode(void)
{
	if (tcsetattr(STDIN_FILENO, TCSANOW, &saved_attr))
		tcsetattr(STDIN_FILENO, TCSANOW, &saved_attr);
}

static int init(void)
{
	int	ret = 0;
	struct termios tattr;

	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO)) {
		ret = -1;
		goto fail;
	}

	ret = tcgetattr(STDIN_FILENO, &tattr);
	if (ret) {
		perror("tcgetattr");
		goto fail;
	}
	saved_attr = tattr;
	atexit(reset_input_mode);
	tattr.c_lflag &= ~ECHO;
	tattr.c_lflag &= ~ICANON;
	ret = tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
	if (ret) {
		perror("tcsetattr");
		goto fail;
	}

	init_libav();

	memset(&control, 0, sizeof(control_t));

	init_sndcard_control();

	ret = pthread_create(&control_thread, NULL, control_process, NULL);
	if (ret != 0) {
		fprintf(stderr, "control thread create failure\n");
		goto fail;
	}

fail:
	return ret;
}

int main(int argc, const char **argv)
{
	int			i;
	int			ret = -1;

	if (argc < 2) {
		fprintf(stderr, "Usage: player <media_filename(s)\n");
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

	return 0;
}
