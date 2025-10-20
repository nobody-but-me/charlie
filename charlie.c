
// /------------------------|-----------------------\
// |-              Macros and Includes             -|
// \------------------------|-----------------------/

#include <termios.h>
#include <ctype.h>

#include <sys/ioctl.h>
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

struct editorConfig {
    struct termios m_OriginalTermios;
    unsigned int screenRows;
    unsigned int screenCols;
};

int getWindowSize(unsigned int *rows, unsigned int *cols);

void error(const char *errorMessage);

void disableRawMode(void);
void enableRawMode(void);

void refreshScreen(void);
void drawRows(void);

void keyPress(void);
char readKey(void);

void init(void);

// /------------------------|-----------------------\
// |-                 Implementation               -|
// \------------------------|-----------------------/


struct editorConfig g_Configuration;


int getWindowSize(unsigned int *rows, unsigned int *cols) {
    struct winsize window_size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1 || window_size.ws_col == 0)
	return -1;
    *cols = window_size.ws_col;
    *rows = window_size.ws_row;
    return 0;
}

void error(const char *errorMessage) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(errorMessage); exit(1);
    return;
}

void disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_Configuration.m_OriginalTermios) == -1)
        error("tcsetattr");
    return;
}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &g_Configuration.m_OriginalTermios) == -1)
        error("tcgetattr");
    atexit(disableRawMode);
    
    struct termios raw = g_Configuration.m_OriginalTermios;
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

void refreshScreen(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    drawRows();
    
    write(STDOUT_FILENO, "\x1b[H", 3);
    return;
}
void drawRows(void) {
    for (unsigned int y = 0; y < g_Configuration.screenRows; y++) {
	write(STDOUT_FILENO, COLUMN_SYMBOL"\r\n", 3);
    }
    return;
}

void keyPress(void) {
    char c = readKey();
    switch (c) {
        case 27:
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    exit(0);
	    break;
    }
    return;
}

char readKey(void) {
    int nread; char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
	if (nread == -1 && errno != EAGAIN)
	    error("read");
    }
    return c;
}

void init(void) {
    if (getWindowSize(&g_Configuration.screenRows, &g_Configuration.screenCols) == -1)
        error("getWindowSize");
    return;
}

int main(void) {
    enableRawMode();
    init();
    
    while (1) {
	refreshScreen();
	keyPress();
    }
    return 0;
}
