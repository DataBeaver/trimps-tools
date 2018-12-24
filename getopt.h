// Adapted from libmspcore
#ifndef MSP_CORE_GETOPT_H_
#define MSP_CORE_GETOPT_H_

#include <list>
#include <stdexcept>
#include <string>
#include <vector>
#include "stringutils.h"

class usage_error: public std::runtime_error
{
private:
	std::string help_;

public:
	usage_error(const std::string &w, const std::string &h = std::string()): std::runtime_error(w), help_(h) { }
	~usage_error() throw() { }

	const char *help() const throw() { return help_.c_str(); }
};


/**
Command line option processor.  Both short and long options are supported, with
optional and required arguments.  Automatic help text generation is also
available.

Short options begin with a single dash and are identified by a single letter.
Multiple short options may be grouped if they take no arguments; for example,
the string "-abc" could be interpreted as having the options 'a', 'b' and 'c'.
If the option takes an argument and there are unused characters in the argv
element, then those characters are interpreted as the argument.  Otherwise the
next element is taken as the argument.  An optional argument must be given in
the same element if it is given.

Long options begin with a double dash and are identified by an arbitrary
string.  An argument can be specified either in the same argv element,
separated by an equals sign, or in the next element.  As with short options,
an optional argument, if given, must be in the same element.

A single option may have both alternative forms, but must always have at least
a long form.  This is to encourage self-documenting options; it's much easier
to remember words than letters.

Positional arguments are also supported.  They are identified by an arbitrary
string, but the identifier is only used in help text and error messages.  Any
number of the final arguments may be optional.

To support applications that take an arbitrary amount of arguments, a single
positional argument list can be specified.  Fixed positional arguments are
allowed together with a list, but they can't be optional.  An application that
wants to do complex processing on the argument list can declare a list of
string arguments.

A built-in --help option is provided and will output a list of options,
arguments and their associated help texts.  An application may override this by
providing its own option with the same name.
*/
class GetOpt
{
public:
	enum ArgType
	{
		NO_ARG,
		OPTIONAL_ARG,
		REQUIRED_ARG
	};

	class Option
	{
	protected:
		Option() { }
	public:
		virtual ~Option() { }

		/// Sets help text for the option.
		virtual Option &set_help(const std::string &) = 0;

		/** Sets help text for the option, with a placeholder metavariable.  The
		metavariable is used to denote the argument in the option list. */
		virtual Option &set_help(const std::string &, const std::string &) = 0;

		virtual Option &bind_seen_count(unsigned &) = 0;

		/// Returns the number of times this option was seen on the command line.
		virtual unsigned get_seen_count() const = 0;
	};

	class Argument
	{
	protected:
		Argument() { }
	public:
		virtual ~Argument() { }

		virtual Argument &set_help(const std::string &) = 0;
	};

private:
	class Store
	{
	protected:
		Store() { }
	public:
		virtual ~Store() { }

		virtual Store *clone() const = 0;

		virtual bool is_list() const = 0;
		virtual void store() = 0;
		virtual void store(const std::string &) = 0;
	};

	class OptionImpl: public Option
	{
	protected:
		char shrt;
		std::string lng;
		ArgType arg_type;
		unsigned seen_count;
		unsigned *ext_seen_count;
		std::string help;
		std::string metavar;
		Store *store;

	public:
		OptionImpl(char, const std::string &, const Store &, ArgType);
		virtual ~OptionImpl();

		virtual OptionImpl &set_help(const std::string &);
		virtual OptionImpl &set_help(const std::string &, const std::string &);
		virtual OptionImpl &bind_seen_count(unsigned &);
		char get_short() const { return shrt; }
		const std::string &get_long() const { return lng; }
		ArgType get_arg_type() const { return arg_type; }
		const std::string &get_help() const { return help; }
		const std::string &get_metavar() const { return metavar; }
		virtual unsigned get_seen_count() const { return seen_count; }
		void process();
		void process(const std::string &);
	};

	class ArgumentImpl: public Argument
	{
	private:
		std::string name;
		ArgType type;
		std::string help;
		Store *store;

	public:
		ArgumentImpl(const std::string &, const Store &, ArgType);
		virtual ~ArgumentImpl();

		virtual ArgumentImpl &set_help(const std::string &);
		const std::string &get_name() const { return name; }
		ArgType get_type() const { return type; }
		const std::string &get_help() const { return help; }
		bool is_list_store() const { return store->is_list(); }
		void process(const std::string &);
	};

	template<typename T>
	class SimpleStore: public Store
	{
	private:
		T &data;

	public:
		SimpleStore(T &d): data(d) { }

		virtual SimpleStore *clone() const
		{ return new SimpleStore(data); }

		virtual bool is_list() const { return false; }

		virtual void store() { }

		virtual void store(const std::string &a)
		{ data = parse_value<T>(a); }
	};

	template<typename T>
	class ListStore: public Store
	{
	private:
		T &data;

	public:
		ListStore(T &d): data(d) { }

		virtual ListStore *clone() const
		{ return new ListStore(data); }

		virtual bool is_list() const { return true; }

		virtual void store() { }

		virtual void store(const std::string &a)
		{ data.push_back(parse_value<typename T::value_type>(a)); }
	};

	typedef std::list<OptionImpl *> OptionList;
	typedef std::list<ArgumentImpl *> ArgumentList;

	bool help;
	OptionList opts;
	ArgumentList args;
	std::vector<std::string> args_raw;

public:
	GetOpt();
	~GetOpt();

	/** Returns any non-option arguments encountered during processing.
	Deprecated. */
	const std::vector<std::string> &get_args() const { return args_raw; }

	/** Adds an option with both short and long forms.  Processing depends on
	the type of the destination variable and whether an argument is taken or
	not.  With an argument, the value is lexical_cast to the appropriate type
	and stored in the destination.  Without an argument, a bool will be set to
	true and an unsigned will be incremented; any other type will be ignored. */
	template<typename T>
	Option &add_option(char s, const std::string &l, T &d, ArgType a = NO_ARG)
	{ return add_option(s, l, SimpleStore<T>(d), a); }

	/** Adds an option with both short and long forms.  The option may be
	specified multiple times, and the argument from each occurrence is stored in
	the list.  The argument type must be REQUIRED_ARG. */
	template<typename T>
	Option &add_option(char s, const std::string &l, std::list<T> &d, ArgType a = REQUIRED_ARG)
	{ return add_option(s, l, ListStore<std::list<T> >(d), a); }

	/** Adds an option with only a long form. */
	template<typename T>
	Option &add_option(const std::string &l, T &d, ArgType a)
	{ return add_option(0, l, d, a); }

	/** Adds a positional argument.  The value will be lexical_cast to the
	appropriate type and stored in the destination. */
	template<typename T>
	Argument &add_argument(const std::string &n, T &d, ArgType a = REQUIRED_ARG)
	{ return add_argument(n, SimpleStore<T>(d), a); }

	/** Adds a positional argument list.  If the list is declared as required,
	at least one element must be given; an optional list may be empty.  Only one
	list may be added, and optional fixed arguments can't be used with it. */
	template<typename T>
	Argument &add_argument(const std::string &n, std::list<T> &d, ArgType a = REQUIRED_ARG)
	{ return add_argument(n, ListStore<std::list<T> >(d), a); }

private:
	OptionImpl &add_option(char, const std::string &, const Store &, ArgType);
	ArgumentImpl &add_argument(const std::string &, const Store &, ArgType);

	OptionImpl &get_option(char);
	OptionImpl &get_option(const std::string &);

public:
	/** Processes argc/argv style command line arguments.  The contents of argv
	will be unchanged; use get_args to access non-option arguments. */
	void operator()(unsigned, const char *const *);

private:
	/** Processes a long option.  Returns the number of arguments eaten. */
	unsigned process_long(const char *const *);

	/** Processes short options.  Returns the number of arguments eaten. */
	unsigned process_short(const char *const *);

public:
	/** Generates a single line that describes known options and arguments.  If
	compact is true, the options list is replaced with a placeholder.  This
	provides cleaner output if full help text is printed. */
	std::string generate_usage(const std::string &, bool compact = false) const;

	/** Generates help for known options and arguments in tabular format, one
	item per line.  The returned string will have a linefeed at the end. */
	std::string generate_help() const;
};

template<> inline void GetOpt::SimpleStore<bool>::store()
{ data = true; }

template<> inline void GetOpt::SimpleStore<unsigned>::store()
{ ++data; }

template<> inline void GetOpt::SimpleStore<std::string>::store(const std::string &a)
{ data = a; }

template<> inline void GetOpt::ListStore<std::list<std::string> >::store(const std::string &a)
{ data.push_back(a); }

#endif
