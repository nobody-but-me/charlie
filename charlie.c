
// /------------------------|-----------------------\
// |-              Macros and Includes             -|
// \------------------------|-----------------------/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <termios.h>
#include <ctype.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#define COLUMN_SYMBOL "."
#define VERSION "0.0.1"
#define TAB_STOP 8

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT { NULL, 0 }

// /------------------------|-----------------------\
// |-                   Definition                 -|
// \------------------------|-----------------------/

typedef struct editorRow {
    char *render;
    char *chars;
    int rsize;
    int size;
} ROW;

enum KEYS {
    BACKSPACE = 127,
    
    RIGHT = 1000,
    LEFT        ,
    DOWN        ,
    UP          ,
    
    PAGE_DOWN   ,
    PAGE_UP     ,
    
    HOME        ,
    END         ,
};

struct editorConfig {
    struct termios m_OriginalTermios;
    unsigned int screenRows;
    unsigned int screenCols;
    
    unsigned int cursorX, cursorY;
    unsigned int renderX;
    
    unsigned int rowsOff;
    unsigned int colsOff;
    
    unsigned int numberRows;
    
    time_t statusMessageTime;
    char statusMessage[80];
    
    char *filename;
    ROW *rows;
};

struct ABUF {
    char *buffer;
    int length;
};

unsigned int rowCxToRx(ROW *row, unsigned int cursorX);

int getWindowSize(unsigned int *rows, unsigned int *cols);

void error(const char *errorMessage);

void bufferAppend(struct ABUF *bff, const char *string, int length);
void bufferFree(struct ABUF *bff);

int getCursorPosition(unsigned int *rows, unsigned int *cols);
void moveCursor(int key);

void disableRawMode(void);
void enableRawMode(void);

void rowInsertChar(ROW *row, int at, int character);
void rowDeleteChar(ROW *row, int at);
void insertChar(int character);
void deleteChar(void);

void setStatusMessage(const char *formated_string, ...);
void drawStatusMessage(struct ABUF *bff);

void drawStatusBar(struct ABUF *bff);
void drawRows(struct ABUF *bff);
void refreshScreen(void);

void keyPress(void);
int readKey(void);

void appendRow(char *string, size_t length);
void updateRow(ROW *row);

void editorScroll(void);

void editorOpen(const char *file_path);
void init(void);

// /------------------------|-----------------------\
// |-                 Implementation               -|
// \------------------------|-----------------------/


struct editorConfig g_Configuration;

unsigned int rowCxToRx(ROW *row, unsigned int cursorX) {
    unsigned int newRenderX = 0;
    for (unsigned int i = 0; i < cursorX; i++) {
	if (row->chars[i] == '\t')
	newRenderX += (TAB_STOP - 1) - (newRenderX % TAB_STOP);
	newRenderX++;
    }
    return newRenderX;
}

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
    ROW *row = (g_Configuration.cursorY >= g_Configuration.numberRows) ? NULL : &g_Configuration.rows[g_Configuration.cursorY];
    
    switch(key) {
        case LEFT:
	    if (g_Configuration.cursorX != 0) g_Configuration.cursorX--;
	    else if (g_Configuration.cursorY > 0) {
		g_Configuration.cursorY--;
		g_Configuration.cursorX = g_Configuration.rows[g_Configuration.cursorY].size;
	    }
	    break;
        case RIGHT:
	    if (row && g_Configuration.cursorX < (unsigned int)row->size) g_Configuration.cursorX++;
	    else if (g_Configuration.cursorY > 0) {
		g_Configuration.cursorY++;
		g_Configuration.cursorX = 0;
	    }
	    break;
        case DOWN:
	    if (g_Configuration.cursorY < g_Configuration.numberRows) g_Configuration.cursorY++;
	    break;
        case UP:
	    if (g_Configuration.cursorY != 0) g_Configuration.cursorY--;
	    break;
    }
    row = (g_Configuration.cursorY >= g_Configuration.numberRows) ? NULL : &g_Configuration.rows[g_Configuration.cursorY];
    unsigned int rowLength = row ? row->size : 0;
    if (g_Configuration.cursorX > rowLength) g_Configuration.cursorX = rowLength;
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

void rowInsertChar(ROW *row, int at, int character) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    
    row->chars[at] = character;
    updateRow(row);
    return;
}
void rowDeleteChar(ROW *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    
    updateRow(row);
}

void insertChar(int character) {
    if (g_Configuration.cursorY == g_Configuration.numberRows)
	appendRow("", 0);
    rowInsertChar(&g_Configuration.rows[g_Configuration.cursorY], g_Configuration.cursorX, character);
    g_Configuration.cursorX++;
    return;
}
void deleteChar(void) {
    if (g_Configuration.cursorY == g_Configuration.numberRows)
	return;
    ROW *row = &g_Configuration.rows[g_Configuration.cursorY];
    if (g_Configuration.cursorX > 0) {
	rowDeleteChar(row, g_Configuration.cursorX - 1);
	g_Configuration.cursorX--;
    }
    return;
}

void setStatusMessage(const char *formated_string, ...) {
    va_list parameters;
    va_start(parameters, formated_string);
    vsnprintf(g_Configuration.statusMessage, sizeof(g_Configuration.statusMessage), formated_string, parameters);
    va_end(parameters);
    g_Configuration.statusMessageTime = time(NULL);
    return;
}
void drawStatusMessage(struct ABUF *bff) {
    bufferAppend(bff, "\x1b[7m", 4);
    unsigned length = strlen(g_Configuration.statusMessage);
    if (length > g_Configuration.screenCols) length = g_Configuration.screenCols;
    if (length && time(NULL) - g_Configuration.statusMessageTime < 5) {
	bufferAppend(bff, g_Configuration.statusMessage, length);
    }
    
    while (length < g_Configuration.screenCols) {
	bufferAppend(bff, " ", 1); length++;
    }
    
    bufferAppend(bff, "\x1b[m", 3);
}

void drawStatusBar(struct ABUF *bff) {
    bufferAppend(bff, "\x1b[7m", 4);
    char status[80];
    
    unsigned int length = snprintf(status, sizeof(status), "%.20s [line %u/%u][column %u/%u]", g_Configuration.filename ? g_Configuration.filename : "[New File]", g_Configuration.cursorY, g_Configuration.numberRows, g_Configuration.cursorX, g_Configuration.screenCols);
    
    if (length > g_Configuration.screenCols)
	length = g_Configuration.screenCols;
    bufferAppend(bff, status, length);
    
    while (length < g_Configuration.screenCols) {
	bufferAppend(bff, " ", 1); length++;
    }
    bufferAppend(bff, "\r\n", 2);
    bufferAppend(bff, "\x1b[m", 3);
    return;
}

void drawRows(struct ABUF *bff) {
    for (unsigned int y = 0; y < g_Configuration.screenRows; y++) {
	unsigned int fileRow = y + g_Configuration.rowsOff;
	if (fileRow >= g_Configuration.numberRows) {
	    if (g_Configuration.numberRows == 0 && y == g_Configuration.screenRows / 2) {
		char welcomeMessage[80];
		unsigned int welcomeLength = snprintf(welcomeMessage, sizeof(welcomeMessage), "Charlie Text Editor %s", VERSION);
		if (welcomeLength > g_Configuration.screenCols) welcomeLength = g_Configuration.screenCols;
		
		int padding = (g_Configuration.screenCols - welcomeLength) / 2;
		if (padding) {
		    bufferAppend(bff, COLUMN_SYMBOL, 1);
		    padding--;
		}
		while (padding--) bufferAppend(bff, " ", 1);
		bufferAppend(bff, welcomeMessage, welcomeLength);
	    } else {
		bufferAppend(bff, COLUMN_SYMBOL, 1);
	    }
	} else {
	    unsigned int length = g_Configuration.rows[fileRow].rsize - g_Configuration.colsOff;
	    
	    if (length < 0) length = 0;
	    if (length > g_Configuration.screenCols) length = g_Configuration.screenCols;
	    
	    bufferAppend(bff, &g_Configuration.rows[fileRow].render[g_Configuration.colsOff], length);
	}
	bufferAppend(bff, "\x1b[K", 3);
	bufferAppend(bff, "\r\n", 2);
    }
}

void refreshScreen(void) {
    editorScroll();
    
    struct ABUF buffer = ABUF_INIT;
    
    bufferAppend(&buffer, "\x1b[?25l", 6);
    bufferAppend(&buffer, "\x1b[H", 3);
    
    drawRows(&buffer);
    drawStatusBar(&buffer);
    drawStatusMessage(&buffer);
    
    char cursor[32];
    snprintf(cursor, sizeof(cursor), "\x1b[%d;%dH", (g_Configuration.cursorY - g_Configuration.rowsOff) + 1, (g_Configuration.renderX - g_Configuration.colsOff) + 1);
    bufferAppend(&buffer, cursor, strlen(cursor));
    
    bufferAppend(&buffer, "\x1b[?25h", 6);
    write(STDOUT_FILENO, buffer.buffer, buffer.length);
    bufferFree(&buffer);
    return;
}

void keyPress(void) {
    ROW *row = (g_Configuration.cursorY >= g_Configuration.numberRows) ? NULL : &g_Configuration.rows[g_Configuration.cursorY];
    int c = readKey();
    switch (c) {
        case '\r': break;
        case 27:
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[H", 3);
	    exit(0);
	    break;
	
	case HOME:
	    g_Configuration.cursorX = 0;
	    break;
	case END:
	    // g_Configuration.cursorX = g_Configuration.screenCols - 1;
	    row = (g_Configuration.cursorY >= g_Configuration.numberRows) ? NULL : &g_Configuration.rows[g_Configuration.cursorY];
	    unsigned int rowLength = row ? row->size : 0;
	    if (g_Configuration.cursorX < rowLength) g_Configuration.cursorX = rowLength;
	    else                                     g_Configuration.cursorX = g_Configuration.screenCols - 1;
	    break;
	
	case CTRL_KEY('h'):
	case BACKSPACE:
	    deleteChar();
	    break;
	
	case PAGE_DOWN:
	case PAGE_UP:
	    {
		if (c == PAGE_DOWN) {
		    g_Configuration.cursorY = g_Configuration.rowsOff + g_Configuration.screenRows -  1;
		    if (g_Configuration.cursorY > g_Configuration.numberRows)
		        g_Configuration.cursorY = g_Configuration.numberRows;
		}
		else if (c == PAGE_UP) {
		    g_Configuration.cursorY = g_Configuration.rowsOff;
		}
		
		int times = g_Configuration.screenRows;
		while (times--)
		    moveCursor(c == PAGE_UP ? UP : DOWN);
	    }
	    break;
	
	case RIGHT:
	case LEFT:
	case DOWN:
	case UP:
	    moveCursor(c);
	    break;
	
	case CTRL_KEY('l'):
	    break;
	
	default:
	    insertChar(c);
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
	    if (sequence[1] >= '0' && sequence[1] <= '9') {
		if (read(STDIN_FILENO, &sequence[2], 1) != 1) return '\x1b';
		if (sequence[2] == '~') {
		    switch (sequence[1]) {
		        case '6': return PAGE_DOWN;
		        case '5': return PAGE_UP;
			
		        case '1': return HOME;
		        case '7': return HOME;
		        case '4': return END;
		        case '8': return END;
		    }
		}
	    } else {
		switch (sequence[1]) {
		    case 'C': return RIGHT;
		    case 'D': return LEFT;
		    case 'B': return DOWN;
		    case 'A': return UP;
		    
		    case 'H': return HOME;
		    case 'F': return END;
	        }
	    }
	} else if (sequence[0] == '0') {
	    switch (sequence[1]) {
	        case 'H': return HOME;
	        case 'F': return END;
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
	    
	    case CTRL_KEY('a'): return HOME;
	    case CTRL_KEY('e'): return END;
	}
    }
    return c;
}

void appendRow(char *string, size_t length) {
    g_Configuration.rows = realloc(g_Configuration.rows, sizeof(ROW) * (g_Configuration.numberRows + 1));
    int at = g_Configuration.numberRows;
    
    g_Configuration.rows[at].size = length;
    g_Configuration.rows[at].chars = malloc(length + 1);
    memcpy(g_Configuration.rows[at].chars, string, length);
    g_Configuration.rows[at].chars[length] = '\0';
    
    g_Configuration.rows[at].render = NULL;
    g_Configuration.rows[at].rsize = 0;
    updateRow(&g_Configuration.rows[at]);
    
    g_Configuration.numberRows++;
    return;
}
void updateRow(ROW *row) {
    int tabs = 0;
    
    for (int i = 0; i < row->size; i++)
    if (row->chars[i] == '\t')
        tabs++;
    free(row->render);
    
    row->render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);
    int index = 0;
    for (int i = 0; i < row->size; i++) {
	if (row->chars[i] == '\t') {
	    row->render[index++] = ' ';
	    while (index % TAB_STOP != 0)
	        row->render[index++] = ' ';
	} else {
	    row->render[index++] = row->chars[i];
	}
    }
    
    row->render[index] = '\0';
    row->rsize = index;
    return;
}

void editorScroll(void) {
    g_Configuration.renderX = 0;
    if (g_Configuration.cursorY < g_Configuration.numberRows)
	g_Configuration.renderX = rowCxToRx(&g_Configuration.rows[g_Configuration.cursorY], g_Configuration.cursorX);
    
    if (g_Configuration.cursorY < g_Configuration.rowsOff) g_Configuration.rowsOff = g_Configuration.cursorY;
    if (g_Configuration.cursorY >= g_Configuration.rowsOff + g_Configuration.screenRows) g_Configuration.rowsOff = g_Configuration.cursorY - g_Configuration.screenRows + 1;
    
    if (g_Configuration.renderX < g_Configuration.colsOff) g_Configuration.colsOff = g_Configuration.renderX;
    if (g_Configuration.renderX >= g_Configuration.colsOff + g_Configuration.screenCols) g_Configuration.colsOff = g_Configuration.renderX - g_Configuration.screenCols + 1;
    return;
}

void editorOpen(const char *file_path) {
    free(g_Configuration.filename); g_Configuration.filename = strdup(file_path);
    FILE *file = fopen(file_path, "r");
    if (!file)
        error("fopen");
    
    size_t capacity = 0;
    char *line = NULL;
    ssize_t length;
    
    while ((length = getline(&line, &capacity, file)) != -1) {
	while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
	    length--;
	appendRow(line, length);
    }
    free(line);
    fclose(file);
    return;
}
void init(void) {
    g_Configuration.cursorX = 0; g_Configuration.cursorY = 0;
    g_Configuration.renderX = 0;
    
    g_Configuration.rowsOff = 0;
    g_Configuration.colsOff = 0;
    
    g_Configuration.numberRows = 0;
    g_Configuration.rows = NULL;
    
    g_Configuration.statusMessage[0] = '\0';
    g_Configuration.statusMessageTime = 0;

    g_Configuration.filename = NULL;
    
    if (getWindowSize(&g_Configuration.screenRows, &g_Configuration.screenCols) == -1)
        error("getWindowSize");
    g_Configuration.screenRows -= 2;
    return;
}

int main(void) {
    enableRawMode();
    
    init(); editorOpen("../../charlie.c");
    
    setStatusMessage("Hello from Charlie!");
    while (1) {
	refreshScreen();
	keyPress();
    }
    return 0;
}
