#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <iostream>
#include "types.h"

class Console
{
public:
	enum ClearLineMode
	{
		CLEAR_WHOLE_LINE,
		CLEAR_FROM_START,
		CLEAR_TO_END
	};

private:
	void *stdout_handle;
	bool has_ansi;
	bool has_256color;
	unsigned width;
	unsigned height;
	unsigned top;

public:
	Console();

	void update_size();
	unsigned get_width() const { return width; }
	unsigned get_height() const { return height; }
	void set_cursor_position(unsigned, unsigned);
	void clear_screen();
	void clear_current_line(ClearLineMode = CLEAR_WHOLE_LINE);
	void set_text_color(unsigned, unsigned = 0);
	void restore_default_text_color();
};

template<typename T>
Console &operator<<(Console &c, const T &d)
{ std::cout << d; return c; }

Console &operator<<(Console &c, std::ostream &(*)(std::ostream &));
Console &operator<<(Console &c, Console &(*)(Console &));

Console &clear_to_end(Console &);
Console &endl_clear(Console &);

#endif
