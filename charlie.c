
// /------------------------|-----------------------\
// |-              Macros and Includes             -|
// \------------------------|-----------------------/

#include <termios.h>
#include <ctype.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define COLUMN_SYMBOL "."
#define VERSION "0.0.1"

// /------------------------|-----------------------\
// |-                   Definition                 -|
// \------------------------|-----------------------/

void error(const char *errorMessage);

void disableRawMode(void);
void enableRawMode(void);

void refresh_screen(void);
void draw_rows(void);

void key_press(void);
char read_key(void);

// /------------------------|-----------------------\
// |-                 Implementation               -|
// \------------------------|-----------------------/

struct termios gOriginalTermios;

void error(const char *errorMessage) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(errorMessage); exit(1);
    return;
}

void disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &gOriginalTermios) == -1)
        error("tcsetattr");
    return;
}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &gOriginalTermios) == -1)
        error("tcgetattr");
    atexit(disableRawMode);
    
    struct termios raw = gOriginalTermios;
    raw.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    
    raw.c_cc[VTIME] = 1;
    raw.c_cc[VMIN] = 0;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        error("tcsetattr");
    return;
}

void refresh_screen(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    draw_rows();
    
    write(STDOUT_FILENO, "\x1b[H", 3);
    return;
}
void draw_rows(void) {
    for (int y = 0; y < 24; y++) {
	write(STDOUT_FILENO, COLUMN_SYMBOL"\r\n", 3);
    }
    return;
}

void key_press(void) {
    char c = read_key();
    switch (c) {
        case 27:
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    exit(0);
	    break;
    }
    return;
}

char read_key(void) {
    int nread; char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
	if (nread == -1 && errno != EAGAIN)
	    error("read");
    }
    return c;
}

int main(void) {
    enableRawMode();
    while (1) {
	refresh_screen();
	key_press();
    }
    return 0;
}
