
// /------------------------|-----------------------\
// |-              Macros and Includes             -|
// \------------------------|-----------------------/

#include <termios.h>
#include <ctype.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define COLUMN_SYMBOL "."
#define VERSION "0.0.1"

#define ABUF_INIT { NULL, 0 }

// /------------------------|-----------------------\
// |-                   Definition                 -|
// \------------------------|-----------------------/

enum KEYS {
    RIGHT = 1000,
    LEFT        ,
    DOWN        ,
    UP          ,
};

struct editorConfig {
    struct termios m_OriginalTermios;
    unsigned int screenRows;
    unsigned int screenCols;
    
    unsigned int cursorX, cursorY;
};

struct ABUF {
    char *buffer;
    int length;
};

int getWindowSize(unsigned int *rows, unsigned int *cols);

void error(const char *errorMessage);

void bufferAppend(struct ABUF *bff, const char *string, int length);
void bufferFree(struct ABUF *bff);

int getCursorPosition(unsigned int *rows, unsigned int *cols);
void moveCursor(int key);

void disableRawMode(void);
void enableRawMode(void);

void drawRows(struct ABUF *bff);
void refreshScreen(void);

void keyPress(void);
int readKey(void);

void init(void);

// /------------------------|-----------------------\
// |-                 Implementation               -|
// \------------------------|-----------------------/


struct editorConfig g_Configuration;


int getWindowSize(unsigned int *rows, unsigned int *cols) {
    struct winsize window_size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1 || window_size.ws_col == 0) {
	if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
	return getCursorPosition(rows, cols);
    }
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

void bufferAppend(struct ABUF *bff, const char *string, int length) {
    char *n = realloc(bff->buffer, bff->length + length);
    if (n ==  NULL) return;
    
    memcpy(&n[bff->length], string, length);
    bff->length += length;
    bff->buffer = n;
    return;
}
void bufferFree(struct ABUF *bff) {
    free(bff->buffer);
    return;
}

int getCursorPosition(unsigned int *rows, unsigned int *cols) {
    char buffer[32]; unsigned int i = 0;
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;
    
    while (i < sizeof(buffer) - 1)
    {
	if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
	if (buffer[i] == 'R') break;
	i++;
    }
    buffer[i] = '\0';
    if (buffer[0] != '\x1b' || buffer[1] != '[')
        return -1;
    if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

void moveCursor(int key) {
    switch(key) {
        case LEFT:
	    g_Configuration.cursorX--;
	    break;
        case RIGHT:
	    g_Configuration.cursorX++;
	    break;
        case DOWN:
	    g_Configuration.cursorY++;
	    break;
        case UP:
	    g_Configuration.cursorY--;
	    break;
    }
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

void drawRows(struct ABUF *bff) {
    for (unsigned int y = 0; y < g_Configuration.screenRows; y++) {
	if (y == g_Configuration.screenRows / 2) {
	    char welcomeMessage[80];
	    unsigned int welcomeLength = snprintf(welcomeMessage, sizeof(welcomeMessage), "Charlie Text Editor %s", VERSION);
	    if (welcomeLength > g_Configuration.screenCols) 
	        welcomeLength = g_Configuration.screenCols;
	    int padding = (g_Configuration.screenCols - (welcomeLength / 2)) / 2;
	    if (padding) {
		bufferAppend(bff, COLUMN_SYMBOL, 1);
		padding--;
	    }
	    while (padding--) bufferAppend(bff, " ", 1);
	    bufferAppend(bff, welcomeMessage, welcomeLength);
	}
	else
	    bufferAppend(bff, COLUMN_SYMBOL, 1);
	
	bufferAppend(bff, "\x1b[K", 3);
	if (y < g_Configuration.screenRows - 1)
	    bufferAppend(bff, "\r\n", 2);
	
    }
    return;
}
void refreshScreen(void) {
    struct ABUF buffer = ABUF_INIT;
    
    bufferAppend(&buffer, "\x1b[?25l", 6);
    bufferAppend(&buffer, "\x1b[H", 3);
    
    drawRows(&buffer);
    
    char cursor[32];
    snprintf(cursor, sizeof(cursor), "\x1b[%d;%dH", g_Configuration.cursorY + 1, g_Configuration.cursorX + 1);
    bufferAppend(&buffer, cursor, strlen(cursor));
    
    bufferAppend(&buffer, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, buffer.buffer, buffer.length);
    bufferFree(&buffer);
    return;
}

void keyPress(void) {
    int c = readKey();
    switch (c) {
        case 27:
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    exit(0);
	    break;
	case RIGHT:
	case LEFT:
	case DOWN:
	case UP:
	    moveCursor(c);
	    break;
    }
    return;
}

int readKey(void) {
    int nread; char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
	if (nread == -1 && errno != EAGAIN)
	    error("read");
    }
    
    if (c == '\x1b') {
	char sequence[3];
	if (read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
	if (read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';
	
	if (sequence[0] == '[') {
	    switch (sequence[1]) {
		// Arrow keys
	        case 'C': return RIGHT;
	        case 'D': return LEFT;
	        case 'B': return DOWN;
	        case 'A': return UP;
	    }
	}
	return '\x1b';
    } else {
	switch (c) {
	    // Emacs-like
	    case CTRL_KEY('f'): return RIGHT;
	    case CTRL_KEY('b'): return LEFT;
	    case CTRL_KEY('n'): return DOWN;
	    case CTRL_KEY('p'): return UP;
	}
    }
    return c;
}

void init(void) {
    g_Configuration.cursorX = 0; g_Configuration.cursorY = 0;
    
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
