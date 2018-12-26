#ifndef CONSOLE_H_
#define CONSOLE_H_

enum Color
{
	BLACK,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE
};

void get_console_size(unsigned &, unsigned &);
void set_cursor_position(unsigned, unsigned);
void clear_screen();
void clear_current_line();
void set_text_color(Color, Color = BLACK);
void restore_default_text_color();

#endif
