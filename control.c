#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <string.h>

#include "control.h"

control_t	control;

static int	row, col;

void *control_process(void *arg)
{
	int	ch;

	getmaxyx(stdscr, row, col);
	while (1) {
		if (control.end_flg) {
			break;
		}

		if (control.begin_flg) {
			ch = getch();

			if (ch == 'p' || ch == ' ') {
				if (!control.pause_flg) {
					control.pause_flg = 1;
					mvprintw(row/2, (col - strlen("pausued\n"))/2, "pausued\n");
					refresh();
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
