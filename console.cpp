#include "console.h"
#include <stdexcept>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstring>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#endif

using namespace std;

namespace {

/*
colors = [(i, min((i&4)*32*(1+(i&8)/8), 255), min((i&2)*64*(1+(i&8)/8), 255), min((i&1)*128*(1+(i&8)/8), 255)) for i in range(16)]
nearest = lambda r, g, b: min(((c[0], ((c[1]-r)**2+(c[2]-g)**2+(c[3]-b)**2)**0.5) for c in colors), key=lambda x: x[1])[0]
print(",\n".join(", ".join("{:2}".format(nearest(j/6*51, j%6*51, i*51)) for i in range(6)) for j in range(36)))
*/
uint8_t colormap[] =
{
	 0,  0,  1,  1,  9,  9,
	 0,  0,  1,  1,  9,  9,
	 2,  2,  3,  3,  3,  9,
	 2,  2,  3,  3,  3, 11,
	10, 10,  3,  3, 11, 11,
	10, 10, 10, 11, 11, 11,

	 0,  0,  1,  1,  9,  9,
	 0,  0,  1,  1,  9,  9,
	 6,  6,  7,  7,  7,  9,
	 6,  6,  7,  7,  7, 11,
	 6,  6,  7,  7, 11, 11,
	10, 10,  7,  7, 11, 11,

	 4,  4,  5,  5,  5,  9,
	 4,  4,  5,  5,  5,  9,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7, 15,
	14, 14,  7,  7, 15, 15,

	 4,  4,  5,  5,  5, 13,
	 4,  4,  5,  5,  5, 13,
	 6,  6,  7,  7,  7, 13,
	 6,  6,  7,  7,  7, 15,
	14, 14,  7,  7, 15, 15,
	14, 14, 14, 15, 15, 15,

	12, 12,  5,  5, 13, 13,
	12, 12,  5,  5, 13, 13,
	 6,  6,  7,  7, 13, 13,
	 6,  6,  7,  7, 15, 15,
	14, 14, 14, 15, 15, 15,
	14, 14, 14, 15, 15, 15,

	12, 12, 12, 13, 13, 13,
	12, 12, 12, 13, 13, 13,
	12, 12, 12, 13, 13, 13,
	14, 14, 14, 15, 15, 15,
	14, 14, 14, 15, 15, 15,
	14, 14, 14, 15, 15, 15
};

#ifndef _WIN32
uint8_t colormap_dimonly[] =
{
	 0,  0,  1,  1,  1,  1,
	 0,  0,  1,  1,  1,  1,
	 2,  2,  3,  3,  3,  3,
	 2,  2,  3,  3,  3,  3,
	 2,  2,  3,  3,  3,  3,
	 2,  2,  3,  3,  3,  3,

	 0,  0,  1,  1,  1,  1,
	 0,  0,  1,  1,  1,  1,
	 2,  2,  3,  3,  3,  3,
	 2,  2,  3,  3,  3,  3,
	 2,  2,  3,  3,  3,  3,
	 2,  2,  3,  3,  3,  3,

	 4,  4,  5,  5,  5,  5,
	 4,  4,  5,  5,  5,  5,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,

	 4,  4,  5,  5,  5,  5,
	 4,  4,  5,  5,  5,  5,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,

	 4,  4,  5,  5,  5,  5,
	 4,  4,  5,  5,  5,  5,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,

	 4,  4,  5,  5,  5,  5,
	 4,  4,  5,  5,  5,  5,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7,
	 6,  6,  7,  7,  7,  7
};

bool term_checked = false;
bool has_256color = false;

inline unsigned swap_bits(unsigned c)
{
	return ((c&4)>>2) | (c&2) | ((c&1)<<2);
}
#endif

}

void get_console_size(unsigned &w, unsigned &h)
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	w = info.dwSize.X;
	h = info.srWindow.Bottom+1-info.srWindow.Top;
#else
	winsize size;
	ioctl(0, TIOCGWINSZ, &size);
	w = size.ws_col;
	h = size.ws_row;
#endif
}

void set_cursor_position(unsigned x, unsigned y)
{
#ifdef _WIN32
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(stdout_handle, &info);
	COORD pos;
	pos.X = x;
	pos.Y = info.srWindow.Top+y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
#else
	cout << "\033[" << y+1 << ';' << x+1 << 'H';
#endif
}

void clear_screen()
{
#ifdef _WIN32
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(stdout_handle, &info);
	COORD start_pos;
	start_pos.X = 0;
	start_pos.Y = info.srWindow.Top;
	unsigned screen_size = info.dwSize.X*info.dwSize.Y;
	DWORD n_filled = 0;
	FillConsoleOutputCharacter(stdout_handle, ' ', screen_size, start_pos, &n_filled);
	FillConsoleOutputAttribute(stdout_handle, info.wAttributes, screen_size, start_pos, &n_filled);
#else
	cout << "\033[2J";
#endif
}

void clear_current_line()
{
#ifdef _WIN32
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(stdout_handle, &info);
	COORD start_pos;
	start_pos.X = 1;
	start_pos.Y = info.dwCursorPosition.Y;
	DWORD n_filled = 0;
	FillConsoleOutputCharacter(stdout_handle, ' ', info.dwSize.X, start_pos, &n_filled);
#else
	cout << "\033[2K";
#endif
}

void set_text_color(unsigned fore, unsigned back)
{
	if(fore>216 || back>216)
		throw invalid_argument("set_text_color");

#ifdef _WIN32
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	fore = colormap[fore];
	back = colormap[back];
	SetConsoleTextAttribute(stdout_handle, fore|(back<<4));
#else
	if(!term_checked)
	{
		term_checked = true;
		const char *term = getenv("TERM");
		has_256color = !strcmp(term, "xterm-256color");
	}
	if(has_256color)
		cout << "\033[38;5;" << 16+fore << ";48;5;" << 16+back << 'm';
	else
	{
		fore = colormap[fore];
		cout << "\033[0;" << 30+swap_bits(fore) << ';' << 40+swap_bits(colormap_dimonly[back]);
		if(fore&8)
			cout << ";1";
		cout << 'm';
	}
#endif
}

void restore_default_text_color()
{
#ifdef _WIN32
	set_text_color(129, 0);
#else
	cout << "\033[0m";
#endif
}
