#include "console.h"
#include <iostream>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <termios.h>
#endif

using namespace std;

void get_console_size(unsigned &w, unsigned &h)
{
#ifdef _WIN32
	w = 80;
	h = 25;
#else
	winsize size;
	ioctl(0, TIOCGWINSZ, &size);
	w = size.ws_col;
	h = size.ws_row;
#endif
}

void set_cursor_position(unsigned x, unsigned y)
{
	cout << "\033[" << y+1 << ';' << x+1 << 'H';
}

void clear_screen()
{
	cout << "\033[2J";
}

void clear_current_line()
{
	cout << "\033[K";
}

void set_text_color(Color fore, Color back)
{
	cout << "\033[" << 30+fore << ';' << 40+back << 'm';
}

void restore_default_text_color()
{
	cout << "\033[0m";
}
