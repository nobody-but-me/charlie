
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)

#define LEFT_COLUMN_SYMBOL "."
#define ABUF_INIT { NULL, 0 }
#define VERSION "0.0.0.1"

struct CONFIG {
    struct termios original_termios;
    int screen_rows;
    int screen_cols;
};
struct ABUF {
    char *buffer;
    int length;
};

void error(const char *s);
void init_editor(void);

int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);

void buffer_append(struct ABUF *buffer, const char *s, int len);
void buffer_free(struct ABUF *buffer);

void disable_raw_mode(void);
void enable_raw_mode(void);

void draw_rows(struct ABUF *buffer);
void refresh_screen(void);

void key_press(void);
char read_key(void);

struct CONFIG g_editor;

// /------------------------------------------------\
// |                 FUNCTIONS                      |
// \------------------------------------------------/


void error(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(s);
    exit(1);
    return;
}

void init_editor(void) {
    if (get_window_size(&g_editor.screen_rows, &g_editor.screen_cols) == -1) {
	error("get window size");
    }
    return;
}

int get_cursor_position(int *rows, int *cols) {
    unsigned int i = 0;
    char buffer[32];
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    while (i < sizeof(buffer) - 1) {
	if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
	if (buffer[i] == 'R') break;
	i++;
    }  
    buffer[i] = '\0';
    if (buffer[0] != '\x1b' || buffer[1] != '[') return -1;
    if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

int get_window_size(int *rows, int *cols) {
    struct winsize window_size;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1 || window_size.ws_col == 0) {
	if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
	return get_cursor_position(rows, cols);
    } else {
	*cols = window_size.ws_col;
	*rows = window_size.ws_row;
	return 0;
    }
}

void buffer_append(struct ABUF *buffer, const char *s, int len) {
    char *new_buffer = realloc(buffer->buffer, buffer->length + len);
    if (new_buffer == NULL) return;
    memcpy(&new_buffer[buffer->length], s, len);
    buffer->buffer = new_buffer;
    buffer->length += len;
    return;
}

void buffer_free(struct ABUF *buffer) {
    free(buffer->buffer);
    return;
}

void disable_raw_mode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_editor.original_termios) == -1) error("tcsetattr");
    return;
}
void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &g_editor.original_termios) == -1) error("tcgetattr");
    atexit(disable_raw_mode);
    
    struct termios raw_mode = g_editor.original_termios;
    
    raw_mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_mode.c_lflag &= ~(ECHO  | ICANON | IEXTEN | ISIG);
    raw_mode.c_oflag &= ~(OPOST);
    raw_mode.c_cflag |=  (CS8);
    
    raw_mode.c_cc[VTIME] = 1;
    raw_mode.c_cc[VMIN] = 0;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode) == -1) error("tcsetattr");
    return;
}

void draw_rows(struct ABUF *buffer) {
    for (int y = 0; y < g_editor.screen_rows; y++) {
	if (y == g_editor.screen_rows / 3) {
	    char welcome[80];
	    int w = snprintf(welcome, sizeof(welcome), "Welcome to Charlie text g_editor [%s]", VERSION);
	    
	    if (w > g_editor.screen_cols) w = g_editor.screen_cols;
	    int padding = (g_editor.screen_cols - w) / 2;
	    if (padding) {
		buffer_append(buffer, LEFT_COLUMN_SYMBOL, 1);
		padding--;
	    }
	    while (padding--) buffer_append(buffer, " ", 1);
	    buffer_append(buffer, welcome, w);
	} else {
	    buffer_append(buffer, LEFT_COLUMN_SYMBOL, 1);
	}
	
	buffer_append(buffer, "\x1b[K", 3);
	if (y < g_editor.screen_rows - 1) {
	    buffer_append(buffer, "\r\n", 2);
	}
    }
    return;
}

void refresh_screen(void) {
    struct ABUF buffer = ABUF_INIT;
    buffer_append(&buffer, "\x1b[?25l", 6);
    buffer_append(&buffer, "\x1b[H", 3);
    draw_rows(&buffer);
    
    buffer_append(&buffer, "\x1b[H", 3);
    buffer_append(&buffer, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, buffer.buffer, buffer.length);
    buffer_free(&buffer);
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
	if (nread == -1 && errno != EAGAIN) error("read");
    }
    return c;
}

int main(void) {
    enable_raw_mode();
    init_editor();
    
    while (1) {
	refresh_screen();
	key_press();
    }
    return 0;
}
