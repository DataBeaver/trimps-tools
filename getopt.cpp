// Adapted from libmspcore
#include <algorithm>
#include "getopt.h"

using namespace std;

GetOpt::GetOpt():
	help(false)
{
	add_option("help", help, NO_ARG).set_help("Displays this help");
}

GetOpt::~GetOpt()
{
	for(OptionList::iterator i=opts.begin(); i!=opts.end(); ++i)
		delete *i;
	for(ArgumentList::iterator i=args.begin(); i!=args.end(); ++i)
		delete *i;
}

GetOpt::OptionImpl &GetOpt::add_option(char s, const string &l, const Store &t, ArgType a)
{
	if(l.empty())
		throw invalid_argument("GetOpt::add_option");
	if(t.is_list() && a!=REQUIRED_ARG)
		throw invalid_argument("GetOpt::add_option");

	for(OptionList::iterator i=opts.begin(); i!=opts.end(); )
	{
		if((s!=0 && (*i)->get_short()==s) || (*i)->get_long()==l)
		{
			delete *i;
			opts.erase(i++);
		}
		else
			++i;
	}

	opts.push_back(new OptionImpl(s, l, t, a));
	return *opts.back();
}

GetOpt::ArgumentImpl &GetOpt::add_argument(const string &n, const Store &t, ArgType a)
{
	if(a==NO_ARG)
		throw invalid_argument("GetOpt::add_argument");

	bool have_list = false;
	bool have_optional = false;
	for(ArgumentList::const_iterator i=args.begin(); i!=args.end(); ++i)
	{
		if((*i)->is_list_store())
			have_list = true;
		else if((*i)->get_type()==OPTIONAL_ARG)
			have_optional = true;
	}

	if(have_optional && (t.is_list() || a!=OPTIONAL_ARG))
		throw invalid_argument("GetOpt::add_argument");
	if(have_list && (t.is_list() || a==OPTIONAL_ARG))
		throw invalid_argument("GetOpt::add_argument");

	args.push_back(new ArgumentImpl(n, t, a));
	return *args.back();
}

GetOpt::OptionImpl &GetOpt::get_option(char s)
{
	for(OptionList::iterator i=opts.begin(); i!=opts.end(); ++i)
		if((*i)->get_short()==s)
			return **i;
	throw usage_error(string("Unknown option -")+s);
}

GetOpt::OptionImpl &GetOpt::get_option(const string &l)
{
	for(OptionList::iterator i=opts.begin(); i!=opts.end(); ++i)
		if((*i)->get_long()==l)
			return **i;
	throw usage_error(string("Unknown option --")+l);
}

void GetOpt::operator()(unsigned argc, const char *const *argv)
{
	try
	{
		/* Arguments must first be collected into an array to handle the case
		where a variable-length argument list is followed by fixed arguments. */
		unsigned i = 1;
		for(; i<argc;)
		{
			if(argv[i][0]=='-')
			{
				if(argv[i][1]=='-')
				{
					if(!argv[i][2])
						break;

					i += process_long(argv+i);
				}
				else
					i += process_short(argv+i);
			}
			else
				args_raw.push_back(argv[i++]);
		}

		for(; i<argc; ++i)
			args_raw.push_back(argv[i]);

		i = 0;
		for(ArgumentList::const_iterator j=args.begin(); j!=args.end(); ++j)
		{
			if((*j)->is_list_store())
			{
				unsigned end = args_raw.size();
				for(ArgumentList::const_iterator k=j; ++k!=args.end(); )
					--end;
				if(i==end && (*j)->get_type()==REQUIRED_ARG)
					throw usage_error((*j)->get_name()+" is required");
				for(; i<end; ++i)
					(*j)->process(args_raw[i]);
			}
			else
			{
				if(i<args_raw.size())
					(*j)->process(args_raw[i++]);
				else if((*j)->get_type()==REQUIRED_ARG)
					throw usage_error((*j)->get_name()+" is required");
			}
		}

		// XXX Enable this when get_args() is completely removed
		/*if(i<args_raw.size())
			throw usage_error("Extra positional arguments");*/
	}
	catch(const usage_error &e)
	{
		if(!help)
			throw usage_error(e.what(), "Usage: "+generate_usage(argv[0]));
	}

	if(help)
		throw usage_error(string("Help for ")+argv[0]+":", "\nUsage:\n  "+generate_usage(argv[0], true)+"\n\n"+generate_help());
}

unsigned GetOpt::process_long(const char *const *argp)
{
	// Skip the --
	const char *arg = argp[0]+2;

	// See if the argument contains an =
	unsigned equals = 0;
	for(; arg[equals] && arg[equals]!='='; ++equals) ;

	OptionImpl &opt = get_option(string(arg, equals));

	if(arg[equals])
		// Process the part after the = as option argument
		opt.process(arg+equals+1);
	else if(opt.get_arg_type()==REQUIRED_ARG)
	{
		if(!argp[1])
			throw usage_error("--"+string(arg)+" requires an argument");

		// Process the next argument as option argument
		opt.process(argp[1]);
		return 2;
	}
	else
		opt.process();

	return 1;
}

unsigned GetOpt::process_short(const char *const *argp)
{
	// Skip the -
	const char *arg = argp[0]+1;

	// Loop through all characters in the argument
	for(; *arg; ++arg)
	{
		OptionImpl &opt = get_option(*arg);

		if(arg[1] && opt.get_arg_type()!=NO_ARG)
		{
			// Need an option argument and we have characters left - use them
			opt.process(arg+1);
			return 1;
		}
		else if(opt.get_arg_type()==REQUIRED_ARG)
		{
			if(!argp[1])
				throw usage_error("-"+string(1, *arg)+" requires an argument");

			// Use the next argument as option argument
			opt.process(argp[1]);
			return 2;
		}
		else
			opt.process();
	}

	return 1;
}

string GetOpt::generate_usage(const string &argv0, bool compact) const
{
	string result = argv0;
	if(compact)
		result += " [options]";
	else
	{
		for(OptionList::const_iterator i=opts.begin(); i!=opts.end(); ++i)
		{
			result += " [";
			if((*i)->get_short())
			{
				result += format("-%c", (*i)->get_short());
				if(!(*i)->get_long().empty())
					result += '|';
				else if((*i)->get_arg_type()==OPTIONAL_ARG)
					result += format("[%s]", (*i)->get_metavar());
				else if((*i)->get_arg_type()==REQUIRED_ARG)
					result += format(" %s", (*i)->get_metavar());
			}
			if(!(*i)->get_long().empty())
			{
				result += format("--%s", (*i)->get_long());

				if((*i)->get_arg_type()==OPTIONAL_ARG)
					result += format("[=%s]", (*i)->get_metavar());
				else if((*i)->get_arg_type()==REQUIRED_ARG)
					result += format("=%s", (*i)->get_metavar());
			}
			result += ']';
		}
	}

	for(ArgumentList::const_iterator i=args.begin(); i!=args.end(); ++i)
	{
		result += ' ';
		if((*i)->get_type()==OPTIONAL_ARG)
			result += '[';
		result += format("<%s>", (*i)->get_name());
		if((*i)->is_list_store())
			result += " ...";
		if((*i)->get_type()==OPTIONAL_ARG)
			result += ']';
	}

	return result;
}

string GetOpt::generate_help() const
{
	bool any_short = false;
	for(OptionList::const_iterator i=opts.begin(); (!any_short && i!=opts.end()); ++i)
		any_short = (*i)->get_short();

	string::size_type maxw = 0;
	list<string> switches;
	for(OptionList::const_iterator i=opts.begin(); i!=opts.end(); ++i)
	{
		string swtch;
		if((*i)->get_short())
		{
			swtch += format("-%c", (*i)->get_short());
			if(!(*i)->get_long().empty())
				swtch += ", ";
			else if((*i)->get_arg_type()==OPTIONAL_ARG)
				swtch += format("[%s]", (*i)->get_metavar());
			else if((*i)->get_arg_type()==REQUIRED_ARG)
				swtch += format(" %s", (*i)->get_metavar());
		}
		else if(any_short)
			swtch += "    ";
		if(!(*i)->get_long().empty())
		{
			swtch += format("--%s", (*i)->get_long());

			if((*i)->get_arg_type()==OPTIONAL_ARG)
				swtch += format("[=%s]", (*i)->get_metavar());
			else if((*i)->get_arg_type()==REQUIRED_ARG)
				swtch += format("=%s", (*i)->get_metavar());
		}
		switches.push_back(swtch);
		maxw = max(maxw, swtch.size());
	}

	list<string> pargs;
	for(ArgumentList::const_iterator i=args.begin(); i!=args.end(); ++i)
	{
		string parg = format("<%s>", (*i)->get_name());
		pargs.push_back(parg);
		maxw = max(maxw, parg.size());
	}

	string result;
	result += "Options:\n";
	list<string>::const_iterator j = switches.begin();
	for(OptionList::const_iterator i=opts.begin(); i!=opts.end(); ++i, ++j)
		result += format("  %s%s%s\n", *j, string(maxw+2-j->size(), ' '), (*i)->get_help());
	if(!pargs.empty())
	{
		result += "\nArguments:\n";
		j = pargs.begin();
		for(ArgumentList::const_iterator i=args.begin(); i!=args.end(); ++i, ++j)
			result += format("  %s%s%s\n", *j, string(maxw+2-j->size(), ' '), (*i)->get_help());
	}

	return result;
}


GetOpt::OptionImpl::OptionImpl(char s, const std::string &l, const Store &t, ArgType a):
	shrt(s),
	lng(l),
	arg_type(a),
	seen_count(0),
	ext_seen_count(0),
	metavar("ARG"),
	store(t.clone())
{ }

GetOpt::OptionImpl::~OptionImpl()
{
	delete store;
}

GetOpt::OptionImpl &GetOpt::OptionImpl::set_help(const string &h)
{
	help = h;
	return *this;
}

GetOpt::OptionImpl &GetOpt::OptionImpl::set_help(const string &h, const string &m)
{
	help = h;
	metavar = m;
	return *this;
}

GetOpt::OptionImpl &GetOpt::OptionImpl::bind_seen_count(unsigned &c)
{
	ext_seen_count = &c;
	return *this;
}

void GetOpt::OptionImpl::process()
{
	if(arg_type==REQUIRED_ARG)
		throw usage_error("--"+lng+" requires an argument");

	++seen_count;
	if(ext_seen_count)
		*ext_seen_count = seen_count;

	try
	{
		store->store();
	}
	catch(const exception &e)
	{
		throw usage_error("Invalid argument for --"+lng+" ("+e.what()+")");
	}
}

void GetOpt::OptionImpl::process(const string &arg)
{
	if(arg_type==NO_ARG)
		throw usage_error("--"+lng+" takes no argument");

	++seen_count;
	if(ext_seen_count)
		*ext_seen_count = seen_count;

	try
	{
		store->store(arg);
	}
	catch(const exception &e)
	{
		throw usage_error("Invalid argument for --"+lng+" ("+e.what()+")");
	}
}


GetOpt::ArgumentImpl::ArgumentImpl(const string &n, const Store &t, ArgType a):
	name(n),
	type(a),
	store(t.clone())
{ }

GetOpt::ArgumentImpl::~ArgumentImpl()
{
	delete store;
}

GetOpt::ArgumentImpl &GetOpt::ArgumentImpl::set_help(const string &h)
{
	help = h;
	return *this;
}

void GetOpt::ArgumentImpl::process(const string &arg)
{
	try
	{
		store->store(arg);
	}
	catch(const exception &e)
	{
		throw usage_error("Invalid "+name+" ("+e.what()+")");
	}
}
