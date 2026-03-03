#ifndef SH_TERMCAP_H
#define SH_TERMCAP_H

int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
int tputs(const char *str, int affcnt, int (*putc_fn)(int));
char *tgoto(const char *cap, int col, int row);

#endif
