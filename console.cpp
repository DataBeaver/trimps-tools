#include "console.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#endif

using namespace std;

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

void set_text_color(Color fore, Color back)
{
#ifdef _WIN32
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(stdout_handle, fore|(back<<4));
#else
	cout << "\033[" << 30+fore << ';' << 40+back << 'm';
#endif
}

void restore_default_text_color()
{
#ifdef _WIN32
	set_text_color(WHITE, BLACK);
#else
	cout << "\033[0m";
#endif
}
