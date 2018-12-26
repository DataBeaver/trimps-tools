#include "console.h"
#include <iostream>

using namespace std;

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
