#include "console.h"
#include <stdexcept>
#include <cstdint>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstring>
#include <sys/ioctl.h>
#include <termios.h>
#endif

#if defined(_WIN32) && !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
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

inline unsigned swap_bits(unsigned c)
{
	return ((c&4)>>2) | (c&2) | ((c&1)<<2);
}

}

Console::Console():
	stdout_handle(0),
	has_ansi(false),
	has_256color(false),
	width(80),
	height(25),
	top(0)
{
#ifdef _WIN32
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(stdout_handle, &mode);
	has_ansi = SetConsoleMode(stdout_handle, mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	has_256color = has_ansi;
#else
	const char *term = getenv("TERM");
	has_ansi = true;
	has_256color = !strcmp(term, "xterm-256color");
#endif
	update_size();
}

void Console::update_size()
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(stdout_handle, &info);
	width = info.dwSize.X;
	height = info.srWindow.Bottom+1-info.srWindow.Top;
	top = info.srWindow.Top;
#else
	winsize size;
	ioctl(0, TIOCGWINSZ, &size);
	width = size.ws_col;
	height = size.ws_row;
#endif
}

void Console::set_cursor_position(unsigned x, unsigned y)
{
#ifdef _WIN32
	if(!has_ansi)
	{
		COORD pos;
		pos.X = x;
		pos.Y = top+y;
		SetConsoleCursorPosition(stdout_handle, pos);
		return;
	}
#endif

	cout << "\033[" << y+1 << ';' << x+1 << 'H';
}

void Console::clear_screen()
{
#ifdef _WIN32
	if(!has_ansi)
	{
		CONSOLE_SCREEN_BUFFER_INFO info;
		GetConsoleScreenBufferInfo(stdout_handle, &info);
		COORD start_pos;
		start_pos.X = 0;
		start_pos.Y = top;
		unsigned screen_size = width*height;
		DWORD n_filled = 0;
		FillConsoleOutputCharacter(stdout_handle, ' ', screen_size, start_pos, &n_filled);
		FillConsoleOutputAttribute(stdout_handle, info.wAttributes, screen_size, start_pos, &n_filled);
		return;
	}
#endif

	cout << "\033[2J";
}

void Console::clear_current_line()
{
#ifdef _WIN32
	if(!has_ansi)
	{
		CONSOLE_SCREEN_BUFFER_INFO info;
		GetConsoleScreenBufferInfo(stdout_handle, &info);
		COORD start_pos;
		start_pos.X = 0;
		start_pos.Y = info.dwCursorPosition.Y;
		DWORD n_filled = 0;
		FillConsoleOutputCharacter(stdout_handle, ' ', width, start_pos, &n_filled);
		return;
	}
#endif

	cout << "\033[2K";
}

void Console::set_text_color(unsigned fore, unsigned back)
{
	if(fore>216 || back>216)
		throw invalid_argument("set_text_color");

#ifdef _WIN32
	if(!has_ansi)
	{
		fore = colormap[fore];
		back = colormap[back];
		SetConsoleTextAttribute(stdout_handle, fore|(back<<4));
		return;
	}
#endif

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
}

void Console::restore_default_text_color()
{
#ifdef _WIN32
	if(!has_ansi)
		return set_text_color(129, 0);
#endif

	cout << "\033[0m";
}
