#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <string>
#include <iostream>
#include <sstream>

typedef std::ostream& (*STRFUNC)(std::ostream&);

class Console
{
private:
	void *stdout_handle;
	bool has_ansi;
	bool has_256color;
	unsigned width;
	unsigned height;
	unsigned top;
	unsigned written;

public:
	Console();

	void update_size();
	unsigned get_width() const { return width; }
	unsigned get_height() const { return height; }
	void set_cursor_position(unsigned, unsigned);
	void clear_screen();
	void clear_current_line();
	void set_text_color(unsigned, unsigned = 0);
	void restore_default_text_color();

	template <typename Tdata>
	Console& operator<<(const Tdata& data)
	{
		std::stringstream temp;
		temp << data;
		return stream_manip(temp);
	}
	Console& operator<< (STRFUNC func);

private:
	Console& stream_manip(const std::stringstream&);
	void handle_newlines(const std::string&);
};

#endif
