#ifndef CONSOLE_H_
#define CONSOLE_H_

class Console
{
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
	void clear_current_line();
	void set_text_color(unsigned, unsigned = 0);
	void restore_default_text_color();
};

#endif
