/* tetris-term - Classic Tetris for your terminal.
 *
 * Copyright (C) 2014 Gjum
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tetris.h"
#include "ledwand.h"

TetrisGame *game;
Ledwand ledwand;

void drawBlock(uint8_t  *buffer, int x, int y, char c) { // {{{
	if (c == 0) return;
	int i = x + y * LEDWAND_PIXEL_X * 12/8; // pos in buffer
	if (c == 0x10) {
		for (int j = 0; j < 8; j++)
			buffer[i + j * LEDWAND_PIXEL_X / 8] = 0x18;
		buffer[i + 3 * LEDWAND_PIXEL_X / 8] = 0xff;
		buffer[i + 4 * LEDWAND_PIXEL_X / 8] = 0xff;
	}
	else {
		for (int j = 0; j < 8; j++)
			buffer[i + j * LEDWAND_PIXEL_X / 8] = 0xff;
	}
} // }}}

void printBoard(TetrisGame *game) { // {{{
	const uint32_t buf_len = LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y;
	uint8_t *buffer = calloc(1, buf_len);
	// current board
	for (int y = 0; y < game->height; y++) {
		for (int x = 0; x < game->width; x++) {
			char c = game->board[x + y * game->width];
			if (c == 0) // empty? try falling brick
				c = colorOfBrickAt(&game->brick, x, y);
			drawBlock(buffer, x, y, c);
		}
	}
	// next brick preview
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			char c = colorOfBrickAt(&game->nextBrick, x, y);
			drawBlock(buffer, x + game->width + 1, y, c);
		}
	}
	// frame
	for (int y = 0; y < game->height; y++)
		drawBlock(buffer, game->width, y, 0x10);
	for (int y = 0; y < 4; y++)
		drawBlock(buffer, game->width + 5, y, 0x10);
	for (int x = 0; x < 5; x++)
		drawBlock(buffer, x + game->width + 1, 4, 0x10);
	// score
	for (int x = 0; x < game->score; x++)
		drawBlock(buffer, x + game->width + 1, 5, 1);
	ledwand_draw_image(&ledwand, buffer, buf_len);
	free(buffer);
} // }}}

void welcome() { // {{{
	printf("tetris-term  Copyright (C) 2014  Gjum\n");
	printf("\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; see `LICENSE' for details.\n");
	printf("\n");
	// Tetris logo
	printf("\e[30;40m  \e[31;41m  \e[30;40m  \e[34;44m  \e[34;44m  \e[34;44m  \e[33;43m  \e[30;40m  \e[30;40m  \e[30;40m  \e[37;47m  \e[35;45m  \e[35;45m  \e[35;45m  \e[39;49m\n");
	printf("\e[31;41m  \e[31;41m  \e[31;41m  \e[34;44m  \e[30;40m  \e[35;45m  \e[33;43m  \e[33;43m  \e[33;43m  \e[30;40m  \e[37;47m  \e[35;45m  \e[30;40m  \e[30;40m  \e[39;49m\n");
	printf("\e[30;40m  \e[36;46m  \e[30;40m  \e[35;45m  \e[35;45m  \e[35;45m  \e[32;42m  \e[30;40m  \e[31;41m  \e[31;41m  \e[37;47m  \e[34;44m  \e[34;44m  \e[34;44m  \e[39;49m\n");
	printf("\e[30;40m  \e[36;46m  \e[30;40m  \e[34;44m  \e[30;40m  \e[30;40m  \e[32;42m  \e[30;40m  \e[31;41m  \e[30;40m  \e[37;47m  \e[30;40m  \e[30;40m  \e[34;44m  \e[39;49m\n");
	printf("\e[30;40m  \e[36;46m  \e[36;46m  \e[34;44m  \e[34;44m  \e[34;44m  \e[32;42m  \e[32;42m  \e[31;41m  \e[30;40m  \e[35;45m  \e[35;45m  \e[35;45m  \e[35;45m  \e[39;49m\n");
	printf("\n");
	printf("\e[1mControls:\e[0m\n");
	printf("<Left>  move brick left\n");
	printf("<Right> move brick right\n");
	printf("<Up>    rotate brick clockwise\n");
	printf("<Down>  rotate brick counter-clockwise\n");
	//printf("<?????> drop brick down\n");
	printf("<Space> move brick down by one step\n");
	printf("<p>     pause game\n");
	printf("<q>     quit game\n");
	printf("\n");
} // }}}

void signalHandler(int signal) { // {{{
	switch(signal) {
		case SIGINT:
		case SIGTERM:
		case SIGSEGV:
			game->isRunning = 0;
			break;
		case SIGALRM:
			tick(game);
			game->timer.it_value.tv_usec = game->sleepUsec;
			setitimer(ITIMER_REAL, &game->timer, NULL);
			break;
	}
	return;
} // }}}

int main(int argc, char **argv) { // {{{
	// init ledwand
	if (ledwand_init(&ledwand)) {
		perror("Fehler beim Init\n");
		exit(1);
	}
	srand(getpid());
	welcome();
	game = newTetrisGame(10, 20);
	printBoard(game);
	while (game->isRunning) {
		usleep(50000);
		processInputs(game);
	}
	destroyTetrisGame(game);
	ledwand_clear(&ledwand);
	ledwand_destroy(&ledwand);
	tcsetattr(STDIN_FILENO, TCSANOW, &game->termOrig);
	return 0;
} // }}}

