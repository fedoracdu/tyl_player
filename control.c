#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "control.h"

control_t	control;

void *control_process(void *arg)
{
	char ch;

	while (1) {
		if (control.end_flg) {
			break;
		}

		if (control.begin_flg) {
			if (read(STDIN_FILENO, &ch, 1) != 1)
				continue;

			if (ch == 'p' || ch == ' ') {
				if (!control.pause_flg) {
					control.pause_flg = 1;
					fprintf(stderr, "\n  pausued\n");
				} else {
					control.pause_flg = 0;
				}
			}

			if (ch == 'q') {
				control.end_flg = 1;
				break;
			}

			if (ch == '<') {
				control.pre_next_flg = 1;
			}
			if (ch == '>') {
				control.pre_next_flg = 2;
			}

			usleep(20000);
			continue;
		}

		usleep(20000);
	}

	return NULL;
}
