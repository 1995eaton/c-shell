#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "term.h"

static struct termios ts_o;
static struct termios ts_n;
static struct winsize tsize;

static void tsize_refresh(void) {
  ioctl(0, TIOCGWINSZ, &tsize);
}

void tbegin(void) {

  memset(&ts_o, 0, sizeof ts_o);
  tcgetattr(STDIN_FILENO, &ts_o);

  memset(&ts_n, 0, sizeof(struct termios));
  memcpy(&ts_n, &ts_o, sizeof(struct termios));

	/* ts_n.c_iflag &= ~ICRNL; */
	ts_n.c_lflag &= ~(ICANON | ECHO);
	/* ts_n.c_lflag &= ~(ECHOCTL | ISIG | ECHOE | IEXTEN | ECHOK); */
	ts_n.c_cc[VMIN] = 1;
	ts_n.c_cc[VTIME] = 0;
	ts_n.c_cc[VEOF] = 1;

  tcsetattr(STDIN_FILENO, TCSANOW, &ts_n);

}

void tenable(void) { tcsetattr(STDIN_FILENO, TCSANOW, &ts_n); }
void tdisable(void) { tcsetattr(STDIN_FILENO, TCSANOW, &ts_o); }

void tclear(void) {
  printf("\x1b[H\x1b[2J");
}

void tend(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &ts_o);
}

void tsetpos(int x, int y) {
  tinfo.x = x;
  tinfo.y = y;
  printf("\x1b[%d;%dH", tinfo.y, tinfo.x);
}

void tmove(int x, int y) {
  tsize_refresh();
  if (tinfo.x + x > 0) {
    tinfo.x += x;
  } else if (tinfo.x + x < tsize.ws_col && tinfo.y - 1 > 0) {
    tinfo.y--;
    tinfo.x = tsize.ws_col;
  }
  if (tinfo.y + y > 0)
    tinfo.y += y;
  printf("\x1b[%d;%dH", tinfo.y, tinfo.x);
}

int tgetc(void) {
  int c = getchar();
  /* if (c == 13) return '\n'; */
  return c;
}

