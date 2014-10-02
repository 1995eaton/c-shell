#ifndef TERM_H
#define TERM_H

void tbegin(void);
void tend(void);
void tclear(void);
void tmove(int x, int y);
void tsetpos(int x, int y);
void tenable(void);
void tdisable(void);
int tgetc(void);
struct {
  int x, y;
} tinfo;

#endif
