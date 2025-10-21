
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

struct editorConfig {
    struct termios m_OriginalTermios;
    unsigned int screenRows;
    unsigned int screenCols;
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

void disableRawMode(void);
void enableRawMode(void);

void drawRows(struct ABUF *bff);
void refreshScreen(void);

void keyPress(void);
char readKey(void);

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
    
    bufferAppend(&buffer, "\x1b[H", 3);
    bufferAppend(&buffer, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, buffer.buffer, buffer.length);
    bufferFree(&buffer);
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
