#include "spire.h"
#include <signal.h>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <regex>
#include "console.h"
#include "getopt.h"
#include "spirepool.h"

struct FancyCell
{
	uint8_t bg_color[3];
	uint8_t text_color;
	std::string line1;
	std::string line2;
};

using namespace std;
using namespace std::placeholders;

#if defined(_WIN32) && !defined(_POSIX_THREAD_SAFE_FUNCTIONS)
struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	if(!localtime_s(result, timep))
		return result;
	return 0;
}
#endif

int main(int argc, char **argv)
{
	try
	{
		Spire spire(argc, argv);
		return spire.main();
	}
	catch(const usage_error &e)
	{
		cout << e.what() << endl;
		const char *help = e.help();
		if(help[0])
			cout << help << endl;
		return 1;
	}
	catch(const exception &e)
	{
		cout << "An error occurred: " << e.what() << endl;
	}
}

Spire *Spire::instance;

Spire::Spire(int argc, char **argv):
	n_pools(10),
	prune_interval(0),
	next_prune(0),
	prune_limit(3),
	cross_rate(500),
	foreign_rate(500),
	core_rate(1000),
	heterogeneous(false),
	n_workers(4),
	loops_per_cycle(200),
	cycle(1),
	loops_per_second(0),
	debug_layout(false),
	update_mode(Layout::FAST),
	report_update_mode(Layout::COMPATIBLE),
	numeric_format(false),
	raw_values(false),
	fancy_output(false),
	show_pools(false),
	network(0),
	live(false),
	athome(false),
	connection(0),
	intr_flag(false),
	budget(0),
	core_budget(0),
	core_mutate(Core::ALL_MUTATIONS),
	no_core_downgrade(false),
	income(false),
	towers(false),
	score_func(damage_score)
{
	instance = this;

	unsigned pool_size = 100;
	unsigned n_pools_seen = 0;
	unsigned prune_interval_seen = 0;
	unsigned prune_limit_seen = 0;
	unsigned foreign_rate_seen = 0;
	unsigned floors = 0;
	unsigned floors_seen = 0;
	string upgrades;
	string core;
	string layout_str;
	string preset;
	bool online = false;
	bool exact = false;
	std::string budget_str;
	std::string core_budget_str;
	unsigned keep_core_mods = 0;
	std::string tower_type;
	unsigned towers_seen = 0;

	GetOpt getopt;
	getopt.add_option('b', "budget", budget_str, GetOpt::REQUIRED_ARG).set_help("Maximum amount of runestones to spend", "NUM");
	getopt.add_option('f', "floors", floors, GetOpt::REQUIRED_ARG).set_help("Number of floors in the spire", "NUM").bind_seen_count(floors_seen);
	getopt.add_option('u', "upgrades", upgrades, GetOpt::REQUIRED_ARG).set_help("Set all trap upgrade levels", "NNNN");
	getopt.add_option('c', "core", core, GetOpt::REQUIRED_ARG).set_help("Set spire core description", "DESC");
	getopt.add_option('d', "core-budget", core_budget_str, GetOpt::REQUIRED_ARG).set_help("Maximum amount of spirestones to spend on core", "NUM");
	getopt.add_option('k', "keep-core-mods", keep_core_mods, GetOpt::NO_ARG).set_help("Do not swap core mods");
	getopt.add_option('n', "numeric-format", numeric_format, GetOpt::NO_ARG).set_help("Output layouts in numeric format");
	getopt.add_option('i', "income", income, GetOpt::NO_ARG).set_help("Optimize runestones per second");
	getopt.add_option("towers", tower_type, GetOpt::OPTIONAL_ARG).set_help("Try to use as many towers as possible").bind_seen_count(towers_seen);
	getopt.add_option("online", online, GetOpt::NO_ARG).set_help("Use the online layout database");
	getopt.add_option("live", live, GetOpt::NO_ARG).set_help("Perform a live query to the database");
#ifdef WITH_128BIT
	getopt.add_option("athome", athome, GetOpt::NO_ARG).set_help("Work on random layouts provided by the database");
#endif
	getopt.add_option('t', "preset", preset, GetOpt::REQUIRED_ARG).set_help("Select a preset to base settings on", "NAME");
	getopt.add_option("fancy", fancy_output, GetOpt::NO_ARG).set_help("Produce fancy output");
	getopt.add_option('e', "exact", exact, GetOpt::NO_ARG).set_help("Use exact calculations, at cost of performance");
	getopt.add_option('w', "workers", n_workers, GetOpt::REQUIRED_ARG).set_help("Number of threads to use", "NUM");
	getopt.add_option('l', "loops", loops_per_cycle, GetOpt::REQUIRED_ARG).set_help("Number of loops per cycle", "NUM");
	getopt.add_option('p', "pools", n_pools, GetOpt::REQUIRED_ARG).set_help("Number of population pools", "NUM").bind_seen_count(n_pools_seen);
	getopt.add_option('s', "pool-size", pool_size, GetOpt::REQUIRED_ARG).set_help("Size of each population pool", "NUM");
	getopt.add_option("prune-interval", prune_interval, GetOpt::REQUIRED_ARG).set_help("Interval for pruning pools, in cycles", "NUM").bind_seen_count(prune_interval_seen);
	getopt.add_option("prune-limit", prune_limit, GetOpt::REQUIRED_ARG).set_help("Minimum number of pools to keep", "NUM").bind_seen_count(prune_limit_seen);
	getopt.add_option("heterogeneous", heterogeneous, GetOpt::NO_ARG).set_help("Use heterogeneous pool configurations");
	getopt.add_option('r', "cross-rate", cross_rate, GetOpt::REQUIRED_ARG).set_help("Probability of crossing two layouts (out of 1000)", "NUM");
	getopt.add_option('o', "foreign-rate", foreign_rate, GetOpt::REQUIRED_ARG).set_help("Probability of crossing from another pool (out of 1000)", "NUM").bind_seen_count(foreign_rate_seen);
	getopt.add_option("core-rate", core_rate, GetOpt::REQUIRED_ARG).set_help("Probability of mutating the core (out of 1000)", "NUM");
	getopt.add_option('g', "debug-layout", debug_layout, GetOpt::NO_ARG).set_help("Print detailed information about the layout");
	getopt.add_option("show-pools", show_pools, GetOpt::NO_ARG).set_help("Show population pool contents while running");
	getopt.add_option("raw-values", raw_values, GetOpt::NO_ARG).set_help("Display raw numeric values");
	getopt.add_argument("layout", layout_str, GetOpt::OPTIONAL_ARG).set_help("Layout to start with");
	getopt(argc, argv);

	if(floors_seen && floors<1)
		throw usage_error("Invalid number of floors");
	if(n_workers<1)
		throw usage_error("Invalid number of worker threads");
	if(loops_per_cycle<1)
		throw usage_error("Invalid number of loops per cycle");
	if(pool_size<1)
		throw usage_error("Invalid pool size");
	if(n_pools<1)
		throw usage_error("Invalid number of pools");
	if(prune_limit<1)
		throw usage_error("Invalid prune limit");

	if(athome)
	{
		prune_interval = 0;
		heterogeneous = false;
		online = true;
		live = false;
		towers_seen = false;
		core_mutate = Core::VALUES_ONLY;
	}
	else if(preset=="single")
	{
		if(!n_pools_seen)
			n_pools = 1;
	}
	else if(preset=="diverse")
	{
		if(!n_pools_seen)
			n_pools = 50;
		if(!prune_interval_seen)
			prune_interval = 50000;
		if(!prune_limit_seen)
			prune_limit = 10;
	}
	else if(preset=="advanced")
	{
		if(!prune_interval_seen)
			prune_interval = 100000;
		heterogeneous = true;
		if(!foreign_rate_seen)
			foreign_rate = 1000;
	}
	else if(!preset.empty() && preset!="basic")
		throw usage_error("Invalid preset");

	towers = towers_seen;
	if(towers_seen)
	{
		if(tower_type=="SC" || tower_type=="CS")
			tower_type = "-K";
		else if(tower_type=="SK" || tower_type=="KS")
			tower_type = "-C";
		else if(tower_type=="CK" || tower_type=="KC")
			tower_type = "-S";

		bool exclude = false;
		if(!tower_type.empty() && tower_type[0]=='-')
		{
			exclude = true;
			tower_type.erase(0, 1);
		}

		char tower = 0;
		if(tower_type=="S" || tower_type=="strength")
			tower = 'S';
		else if(tower_type=="C" || tower_type=="condenser")
			tower = 'C';
		else if(tower_type=="K" || tower_type=="knowledge")
			tower = 'K';
		else if(!tower_type.empty())
			throw usage_error("Invalid tower type");

		if(income)
			score_func = get_towers_score_func<income_score>(tower, exclude);
		else
			score_func = get_towers_score_func<damage_score>(tower, exclude);
	}
	else if(income)
		score_func = income_score;

	if(income)
	{
		update_mode = Layout::FULL;
		report_update_mode = Layout::FULL;
	}
	else if(exact)
	{
		update_mode = Layout::EXACT_DAMAGE;
		report_update_mode = Layout::FULL;
	}

	if(keep_core_mods)
	{
		core_mutate = Core::VALUES_ONLY;
		no_core_downgrade = (keep_core_mods>=2);
	}

	if(!n_pools_seen && heterogeneous)
		n_pools = 21;
	if(n_pools==1)
	{
		foreign_rate = 0;
		prune_interval = 0;
	}
	if(n_pools<=prune_limit)
		prune_interval = 0;
	if(prune_interval)
		next_prune = prune_interval;

	if(!athome)
		init_start_layout(parse_layout(layout_str, upgrades, core, floors));
	init_pools(pool_size);

	if(!budget_str.empty())
	{
		try
		{
			budget = parse_value<NumberIO>(budget_str).value;
		}
		catch(const exception &e)
		{
			throw usage_error(format("Invalid argument for --budget (%s)", e.what()));
		}
		if(budget_str[0]=='+')
			budget += start_layout.get_cost();
	}
	else if(!start_layout.get_traps().empty())
		budget = start_layout.get_cost();
	else
		budget = 1000000;

	if(!core_budget_str.empty())
	{
		try
		{
			core_budget = parse_value<NumberIO>(core_budget_str).value;
		}
		catch(const exception &e)
		{
			throw usage_error(format("Invalid argument for --core-budget (%s)", e.what()));
		}
		if(core_budget_str[0]=='+')
			core_budget += start_layout.get_core().cost;
	}

	if(!core_budget)
		core_rate = 0;

	if(online || live)
		init_network(false);
}

void Spire::parse_numeric_layout(const string& layout, ParsedLayout &parsed)
{
	auto first_plus = layout.find('+');
	auto second_plus = first_plus+5;

	parsed.traps = layout.substr(0, first_plus);
	for(char &c: parsed.traps)
	{
		if(c<'0' || '7'<c)
			throw usage_error(format("Invalid trap '%c'", c));
		c = Layout::traps[c-'0'];
	}
	parsed.upgrades = layout.substr(first_plus+1, 4);
	parsed.floors = parse_value<unsigned>(layout.substr(second_plus+1));
}

void Spire::parse_alpha_layout(const string &layout_in, ParsedLayout &parsed)
{
	parsed.upgrades = layout_in.substr(0, 4);
	parsed.traps = remove_spaces(layout_in.substr(5));
	parsed.floors = parsed.traps.length()/5;
}

Spire::ParsedLayout Spire::parse_layout(const string &layout_in, const string &upgrades_in, const string& core_in, unsigned floors)
{
	ParsedLayout parsed;

	/* Checks for a layout string in Swaq format:
	swaq ::=      [traps] '+' [upgrades] '+' [floors]
	traps ::=     r'(\d{5})+'
	upgrades ::=  r'\d{4}'
	floors ::=    r'\d' */
	static regex numeric_regex("(\\d{5})+\\+\\d{4}\\+\\d{1,2}");

	/* Checks for a layout string in the optimizer format:
	tower ::=     [upgrades] ' ' [traps] | [upgrades] [traps]
	upgrades ::=  r'\d{4}'
	traps ::=     [trap_row] | [trap_row] ' ' [trap_row] | [trap_row] [trap_row]
	trap_row :=   r'[FZPLSCK_]{5}' */
	static regex alpha_regex("\\d{4} ?([FZPLSCK_]{5} ?)*[FZPLSCK_]{5}+");

	if(regex_match(layout_in, numeric_regex))
		parse_numeric_layout(layout_in, parsed);
	else if(regex_match(layout_in, alpha_regex))
		parse_alpha_layout(layout_in, parsed);
	else if(!layout_in.empty())
		throw usage_error("Unknown layout format");
	else
		parsed.floors = 0;

	if(!upgrades_in.empty())
		parsed.upgrades = upgrades_in;

	if(!core_in.empty())
		parsed.core = core_in;

	if(floors!=0)
		parsed.floors = floors;

	return parsed;
}

void Spire::init_start_layout(const ParsedLayout &layout)
{
	if(!layout.upgrades.empty())
	{
		try
		{
			start_layout.set_upgrades(layout.upgrades);
		}
		catch(const exception &)
		{
			throw usage_error("Upgrades string must consist of four numbers");
		}
	}

	const TrapUpgrades &start_upgrades = start_layout.get_upgrades();
	if(start_upgrades.fire>8)
		throw usage_error("Invalid fire trap upgrade level");
	if(start_upgrades.frost>8)
		throw usage_error("Invalid frost trap upgrade level");
	if(start_upgrades.poison>9)
		throw usage_error("Invalid poison trap upgrade level");
	if(start_upgrades.lightning>6)
		throw usage_error("Invalid lightning trap upgrade level");

	if(!layout.core.empty())
	{
		try
		{
			Core core = layout.core;
			core.update();
			start_layout.set_core(core);
		}
		catch(const invalid_argument &e)
		{
			throw usage_error(e.what());
		}
	}

	if(!layout.traps.empty())
	{
		string clean_data;
		clean_data.reserve(layout.traps.size());
		for(char c: layout.traps)
		{
			if(c>='0' && c<='7')
				clean_data += Layout::traps[c-'0'];
			else if(c!=' ')
			{
				unsigned i;
				for(i=7; (i>0 && Layout::traps[i]!=c); --i) ;
				clean_data += Layout::traps[i];
			}
		}

		start_layout.set_traps(clean_data, layout.floors);
		start_layout.update(report_update_mode);
	}
	else
	{
		start_layout.set_traps(string(), (layout.floors ? layout.floors : 7));
		start_layout.update(Layout::COST_ONLY);
	}
}

void Spire::init_pools(unsigned pool_size)
{
	pools.reserve(n_pools);
	for(unsigned i=0; i<n_pools; ++i)
		pools.push_back(new Pool(pool_size, score_func));

	unsigned floors = start_layout.get_traps().size()/5;
	uint8_t downgrade[4] = { };
	for(unsigned i=0; i<pools.size(); ++i)
	{
		if(i==0 && start_layout.get_damage())
		{
			pools[i]->add_layout(start_layout);
			continue;
		}

		TrapUpgrades pool_upgrades = start_layout.get_upgrades();

		unsigned reduce = 0;
		for(unsigned j=0; j<sizeof(downgrade); ++j)
		{
			if(downgrade[j]==1 && pool_upgrades.fire>1)
				--pool_upgrades.fire;
			else if(downgrade[j]==2 && pool_upgrades.frost>1)
				--pool_upgrades.frost;
			else if(downgrade[j]==3 && pool_upgrades.poison>1)
				--pool_upgrades.poison;
			else if(downgrade[j]==4 && pool_upgrades.lightning>1)
				--pool_upgrades.lightning;
			else if(downgrade[j]==5 && reduce+1<floors)
				++reduce;
		}

		Layout empty;
		empty.set_upgrades(pool_upgrades);
		empty.set_core(start_layout.get_core());
		empty.set_traps(string(), floors-reduce);
		pools[i]->add_layout(empty);

		if(heterogeneous)
		{
			++downgrade[0];
			for(unsigned j=1; (j<sizeof(downgrade) && downgrade[j-1]>5); ++j)
			{
				++downgrade[j];
				for(unsigned k=0; k<j; ++k)
					downgrade[k] = downgrade[j];
			}
		}
	}
}

void Spire::init_network(bool reconnect)
{
	if(!network)
		network = new Network;
	try
	{
		connection = network->connect("spiredb.tdb.fi", 8676, bind(&Spire::receive, this, _1, _2));
		if(reconnect)
		{
			if(fancy_output)
			{
				console.set_cursor_position(58, 15);
				console << "Online      ";
			}
			else
				console << "Reconnected to online build database" << endl;
		}
	}
	catch(const exception &e)
	{
		if(!fancy_output && !reconnect)
		{
			console << "Can't connect to online build database:" << endl;
			console << e.what() << endl;
			console << "Connection will be reattempted automatically" << endl;
		}
		reconnect_timeout = chrono::steady_clock::now()+chrono::seconds(30);
	}
}

Spire::~Spire()
{
	for(auto p: pools)
		delete p;
}

int Spire::main()
{
	if(debug_layout)
	{
		start_layout.debug(start_layout.get_damage());
		console << "Threat: " << start_layout.get_threat() << endl;
		console << "Runestones: " << start_layout.get_runestones_per_second() << "/s" << endl;
		return 0;
	}

	if(show_pools || fancy_output)
	{
		console.clear_screen();
		console.set_cursor_position(0, 0);
	}

	best_layout = pools.front()->get_best_layout();
	if(best_layout.get_damage() && !show_pools)
		report(best_layout, "Initial layout");

	if(connection)
	{
		if(!athome && !fancy_output)
			console << "Querying online database for best known layout" << endl;
		if(athome)
			;
		else if(query_network())
		{
			if(!show_pools)
				report(best_layout, "Layout from database");
		}
		else
		{
			if(!fancy_output)
				console << "Database returned no better layout" << endl;

			submit_best();
		}
	}

	signal(SIGINT, sighandler);

	Random random;
	for(unsigned i=0; i<n_workers; ++i)
		workers.push_back(new Worker(*this, random(), athome));

	chrono::steady_clock::time_point period_start_time = chrono::steady_clock::now();
	unsigned period_start_cycle = cycle;
	bool leave_loop = false;
	while(!leave_loop)
	{
		this_thread::sleep_for(chrono::milliseconds(500));

		chrono::steady_clock::time_point current_time = chrono::steady_clock::now();
		unsigned period_end_cycle = cycle;
		loops_per_second = loops_per_cycle*(period_end_cycle-period_start_cycle)/chrono::duration<float>(current_time-period_start_time).count();
		period_start_time = current_time;
		period_start_cycle = period_end_cycle;

		if(intr_flag)
		{
			for(auto w: workers)
				w->interrupt();
			for(auto w: workers)
				w->join();

			leave_loop = true;
		}

		check_reconnect(current_time);
		check_athome_work(current_time);

		lock_guard<mutex> lock(best_mutex);
		bool new_best_found = check_results();
		update_output(new_best_found);
	}

	for(auto w: workers)
		delete w;

	if(show_pools || fancy_output)
	{
		console.set_cursor_position(0, console.get_height()-1);
		cout.flush();
	}

	return 0;
}

bool Spire::query_network()
{
	if(!connection)
		return false;

	unsigned floors = best_layout.get_traps().size()/5;
	string query = format("query upg=%s f=%s rs=%s", best_layout.get_upgrades().str(), floors, budget);
	if(best_layout.get_core().tier>=0)
		query += format(" core=%s", best_layout.get_core().str(true));
	if(core_budget>0)
		query += format(" ss=%s", core_budget);
	if(income)
		query += " income";
	if(towers)
		query += " towers";
	if(live)
		query += " live";
	string reply = network->communicate(connection, query);
	if(reply.empty())
		return false;

	vector<string> parts = split(reply);
	if(parts[0]=="ok")
	{
		Layout layout;
		layout.set_upgrades(best_layout.get_upgrades());
		layout.set_core(best_layout.get_core());

		parts.erase(parts.begin());
		process_network_reply(parts, layout);

		if(score_func(layout)>score_func(best_layout))
		{
			best_layout = layout;
			pools.front()->add_layout(best_layout);
			return true;
		}
	}

	return false;
}

void Spire::process_network_reply(const vector<string> &args, Layout &layout)
{
	unsigned floors = layout.get_traps().size()/5;

	Core received_core;

	for(const auto &arg: args)
	{
		if(!arg.compare(0, 4, "upg="))
			layout.set_upgrades(arg.substr(4));
		else if(!arg.compare(0, 2, "t="))
			layout.set_traps(arg.substr(2), floors);
		else if(!arg.compare(0, 5, "core="))
		{
			received_core = arg.substr(5);
			received_core.update();
		}
		else if(athome)
		{
			if(!arg.compare(0, 3, "rs="))
				budget = parse_value<NumberIO>(arg.substr(3));
			else if(!arg.compare(0, 3, "ss="))
				core_budget = parse_value<NumberIO>(arg.substr(3));
			else if(arg=="income")
				income = true;
			else if(arg=="damage")
				income = false;
			else if(arg=="towers")
				towers = true;
		}
	}

	layout.update(report_update_mode);
	if(received_core.tier>=0 && (athome || check_better_core(layout, received_core)))
	{
		layout.set_core(received_core);
		layout.update(report_update_mode);
	}
}

bool Spire::check_better_core(const Layout &layout, const Core &core)
{
	if(!validate_core(core))
		return false;

	Layout with_core = layout;
	with_core.set_core(core);
	with_core.update(report_update_mode);
	return score_func(with_core)>score_func(layout);
}

bool Spire::validate_core(const Core &core)
{
	if(core.cost>core_budget)
		return false;

	if(core_mutate==Core::ALL_MUTATIONS)
		return true;

	const Core &start_core = start_layout.get_core();
	if(no_core_downgrade)
	{
		for(unsigned j=0; j<Core::N_MODS; ++j)
			if(core.get_mod(j)<start_core.get_mod(j))
				return false;
	}
	else
	{
		for(unsigned j=0; j<Core::N_MODS; ++j)
			if((core.get_mod(j)!=0) != (start_core.get_mod(j)!=0))
				return false;
	}

	return true;
}

void Spire::check_reconnect(const chrono::steady_clock::time_point &current_time)
{
	if(!network || connection || current_time<reconnect_timeout)
		return;

	init_network(true);
	if(connection)
	{
		lock_guard<mutex> lock(best_mutex);
		if(athome)
			;
		else if(query_network())
		{
			if(!show_pools)
				report(best_layout, "New best layout from database");
		}
		else
			submit_best();
	}
}

void Spire::check_athome_work(const chrono::steady_clock::time_point &current_time)
{
	if(!athome || !connection || current_time<next_work_timeout)
		return;

	network->send_message(connection, "getwork");
	next_work_timeout = current_time+chrono::minutes(1);
}

bool Spire::check_results()
{
	bool new_best = false;
	for(auto *p: pools)
	{
		if(p->get_best_layout(best_layout))
			new_best = true;
		if(heterogeneous)
			break;
	}

	if(new_best)
	{
		best_layout.update(report_update_mode);
		next_work_timeout = chrono::steady_clock::now()+chrono::minutes(1);
		submit_best();
	}

	if(next_prune && cycle>=next_prune)
		prune_pools();

	return new_best;
}

void Spire::submit_best()
{
	if(!network || !score_func(best_layout))
		return;

	string submit = format("submit upg=%s t=%s", best_layout.get_upgrades().str(), best_layout.get_traps());
	if(best_layout.get_core().tier>=0)
		submit += format(" core=%s", best_layout.get_core().str(true));
	network->send_message(connection, submit);
}

void Spire::update_output(bool new_best_found)
{
	if(show_pools)
	{
		console.update_size();
		console.set_cursor_position(0, 0);
		unsigned n_print = (console.get_height()-2)/n_pools-1;
		for(unsigned i=0; i<n_pools; ++i)
		{
			unsigned count = n_print;
			pools[i]->visit_layouts(bind(&Spire::print, this, _1, ref(count)));
			if(n_print>1)
			{
				for(++count; count>0; --count)
				{
					console.clear_current_line();
					console << endl;
				}
			}
		}

		console << loops_per_second << " loops/sec" << endl_clear;
	}
	else
	{
		if(new_best_found)
			report(best_layout, "New best layout found");
		else if(fancy_output)
		{
			console.set_cursor_position(69, 13);
			console << cycle;
			console.set_cursor_position(69, 14);
			console << NumberIO(loops_per_second) << clear_to_end;
			cout.flush();
		}
	}
}

unsigned Spire::get_next_cycle()
{
	return cycle.fetch_add(1U, memory_order_relaxed);
}

void Spire::prune_pools()
{
	if(n_pools<=1)
		return;

	lock_guard<mutex> lock(pools_mutex);
	unsigned lowest = 0;
	if(heterogeneous)
		++lowest;
	Number score = pools[lowest]->get_best_score();
	for(unsigned i=lowest+1; i<n_pools; ++i)
	{
		Number s = pools[i]->get_best_score();
		if(s<score)
		{
			lowest = i;
			score = s;
		}
	}

	if(lowest+1<n_pools)
		swap(pools[lowest], pools[n_pools-1]);
	--n_pools;

	if(n_pools>prune_limit)
		next_prune += prune_interval;
	else
	{
		if(n_pools==1)
			foreign_rate = 0;
		next_prune = 0;
	}
}

void Spire::receive(Network::ConnectionTag, const string &message)
{
	if(message.empty())
	{
		if(fancy_output)
		{
			console.set_cursor_position(58, 15);
			console << "Disconnected";
		}
		else
			console << "Connection to online database lost" << endl;
		reconnect_timeout = chrono::steady_clock::now()+chrono::seconds(30);
		connection = 0;
		return;
	}

	vector<string> parts = split(message);
	string cmd = parts.front();
	parts.erase(parts.begin());

	if(cmd=="push")
	{
		Layout layout;
		{
			lock_guard<mutex> lock_best(best_mutex);
			layout.set_core(best_layout.get_core());
		}

		process_network_reply(parts, layout);

		lock_guard<mutex> lock_best(best_mutex);
		if(score_func(layout)>score_func(best_layout))
		{
			best_layout = layout;
			report(best_layout, "New best layout from database");

			lock_guard<mutex> lock_pools(pools_mutex);
			pools.front()->add_layout(layout);
		}
	}
	else if(cmd=="work")
	{
		budget = 0;
		core_budget = 0;
		income = false;

		Layout layout;
		process_network_reply(parts, layout);

		pause_workers();

		Layout empty;
		empty.set_upgrades(layout.get_upgrades());
		empty.set_core(layout.get_core());
		empty.set_traps(string(), layout.get_traps().size()/5);

		if(towers)
			score_func = (income ? &towers_score<income_score, 0x40404> : &towers_score<damage_score, 0x40404>);
		else
			score_func = (income ? &income_score : &damage_score);
		for(auto i=pools.begin(); i!=pools.end(); ++i)
		{
			(*i)->reset(score_func);
			(*i)->add_layout(i==pools.begin() ? layout : empty);
		}

		{
			lock_guard<mutex> lock(best_mutex);
			best_layout = layout;
		}
		budget = max(budget, layout.get_cost());

		resume_workers();

		report(layout, "New spire@home work from database");
	}
}

void Spire::pause_workers()
{
	for(auto w: workers)
		w->set_paused(true);
	for(auto w: workers)
		w->wait_paused();
}

void Spire::resume_workers()
{
	for(auto w: workers)
		w->set_paused(false);
}

void Spire::report(const Layout &layout, const string &message)
{
	if(fancy_output)
	{
		console.set_cursor_position(0, 0);
		console.clear_current_line();
	}

	time_t t = chrono::system_clock::to_time_t(chrono::system_clock::now());
	struct tm lt;
	console << '[' << put_time(localtime_r(&t, &lt), "%Y-%m-%d %H:%M:%S") << "] " << message;
	if(!fancy_output)
		console << " (" << print_num(layout.get_damage()) << " damage, " << layout.get_threat() << " threat, "
			<< print_num(layout.get_runestones_per_second()) << " Rs/s, cost " << print_num(layout.get_cost())
			<< " Rs, cycle " << layout.get_cycle() << "):";
	console << endl << "  ";
	unsigned count = 1;
	print(layout, count);
	if((core_budget || fancy_output) && layout.get_core().tier>=0)
		console << "  Core: " << layout.get_core().str() << endl_clear;

	if(fancy_output)
	{
		print_fancy(layout);
		console.set_cursor_position(58, 4);
		console << "Mode:   " << (income ? "income" : "damage");
		if(towers)
			console << "+towers";
		console.set_cursor_position(58, 5);
		console << "Budget: " << print_num(budget) << " Rs" << clear_to_end;
		console.set_cursor_position(58, 7);
		console << "Damage: " << print_num(layout.get_damage()) << clear_to_end;
		console.set_cursor_position(58, 8);
		console << "Threat: " << layout.get_threat() << clear_to_end;
		console.set_cursor_position(58, 9);
		console << "Income: " << print_num(layout.get_runestones_per_second()) << " Rs/s" << clear_to_end;
		console.set_cursor_position(58, 10);
		console << "Cost:   " << print_num(layout.get_cost()) << " Rs" << clear_to_end;
		console.set_cursor_position(58, 11);
		console << "Cycle:  " << layout.get_cycle() << clear_to_end;
		console.set_cursor_position(58, 13);
		console << "Cycle now: " << cycle;
		console.set_cursor_position(58, 14);
		console << "Speed:     " << NumberIO(loops_per_second) << clear_to_end;
		if(network)
		{
			console.set_cursor_position(58, 15);
			console << (connection ? "Online      " : "Disconnected");
			if(athome)
			{
				console.set_cursor_position(58, 16);
				console << "Spire @ home";
			}
		}
		cout.flush();
	}
}

bool Spire::print(const Layout &layout, unsigned &count)
{
	const string &traps = layout.get_traps();
	unsigned cells = traps.size();
	string descr;
	if(numeric_format)
	{
		descr.reserve(cells+8);
		for(unsigned i=0; i<cells; ++i)
			for(unsigned j=0; Layout::traps[j]; ++j)
				if(traps[i]==Layout::traps[j])
					descr += '0'+j;
		descr += '+';
		descr += layout.get_upgrades().str();
		descr += '+';
		descr += stringify(cells/5);
	}
	else
	{
		descr.reserve(5+cells+cells/5-1);
		descr += layout.get_upgrades().str();
		for(unsigned i=0; i<cells; i+=5)
		{
			descr += ' ';
			descr.append(traps.substr(i, 5));
		}
	}

	if(show_pools)
	{
		console.clear_current_line();
		console << descr << ' ' << score_func(layout) << ' ' << layout.get_cost() << ' ' << layout.get_cycle() << endl;
	}
	else if(fancy_output && athome)
		console << descr << endl_clear;
	else
		console << descr << endl;

	return (count && --count);
}

void Spire::print_fancy(const Layout &layout)
{
	static const uint8_t border_color = 172;
	static const uint8_t trap_colors[] = {   0, 108,   9,  13, 126 , 78,  43,  38 };
	static const uint8_t text_colors[] = {  86, 215, 215, 215,   0,   0, 215, 215 };
	const string &traps = layout.get_traps();
	unsigned floors = traps.size()/5;

	console.update_size();
	unsigned lines_per_floor = min((console.get_height()-4)/floors, 3U);

	Number max_hp = layout.get_damage();
	vector<CellInfo> cells;
	layout.build_cell_info(cells, max_hp);

	vector<FancyCell> fancy;
	fancy.reserve(cells.size());
	Number hp = max_hp;
	for(const auto &c: cells)
	{
		unsigned j;
		for(j=0; Layout::traps[j]!=c.trap; ++j) ;

		FancyCell fc;
		for(unsigned k=0; k<3; ++k)
			fc.bg_color[k] = trap_colors[j];
		fc.text_color = text_colors[j];

		fc.line1.reserve(10);
		fc.line1 += ' ';
		fc.line1 += (c.trap=='_' ? ' ' : c.trap);
		string damage_str = stringify(NumberIO(c.damage_taken));
		fc.line1.append(7-damage_str.size(), ' ');
		fc.line1 += damage_str;
		fc.line1 += ' ';

		fc.line2.reserve(10);
		fc.line2 += ' ';
		fc.line2 += (c.steps>1 ? '0'+c.steps : ' ');
		fc.line2.append(c.shocked_steps, '!');
		fc.line2.append(3-c.shocked_steps, ' ');
		string hp_pct_str = stringify(hp*100/max_hp);
		fc.line2.append(3-hp_pct_str.size(), ' ');
		fc.line2 += hp_pct_str;
		fc.line2 += "% ";

		fancy.push_back(fc);
		hp = c.hp_left;
	}

#ifdef _WIN32
	static const char topleft[] = "_";
	static const char topline[] = "___________";
	static const char hsep[] = "__________|";
	static const char vsep[] = "|";
#else
	static const char topleft[] = "▗";
	static const char topline[] = "▄▄▄▄▄▄▄▄▄▄▄";
	static const char hsep[] = "▄▄▄▄▄▄▄▄▄▄▟";
	static const char vsep[] = "▐";
#endif

	if(lines_per_floor>=3)
	{
		for(unsigned i=0; i<cells.size(); ++i)
		{
			if(cells[i].trap=='S')
			{
				unsigned b = i-i%5;
				for(unsigned j=0; j<5; ++j)
					if(cells[b+j].trap=='F')
						fancy[b+j].bg_color[2] = trap_colors[5];
			}
			else if(cells[i].trap=='Z' && i>0 && cells[i-1].trap=='P')
				fancy[i-1].bg_color[2] = trap_colors[2];
			else if(cells[i].trap=='P' && i>0 && cells[i-1].trap=='P')
			{
				fancy[i-1].bg_color[0] = 6;
				fancy[i].bg_color[0] = 6;
			}
		}

		console.set_text_color(border_color, 0);
		console << topleft;
		for(unsigned i=0; i<5; ++i)
			console << topline;
		console << endl;
	}

	for(unsigned i=floors*lines_per_floor; i-->0; )
	{
		console.set_text_color(border_color, 0);
		console << vsep;

		unsigned line = lines_per_floor-1-i%lines_per_floor;
		for(unsigned j=0; j<5; ++j)
		{
			const FancyCell &fc = fancy[(i/lines_per_floor)*5+j];
			if(line==2)
			{
				console.set_text_color(border_color, fc.bg_color[line]);
				console << hsep;
			}
			else
			{
				console.set_text_color(fc.text_color, fc.bg_color[line]);
				console << (line==0 ? fc.line1 : fc.line2);
				console.set_text_color(border_color, fc.bg_color[line]);
				console << vsep;
			}
		}

		console << endl;
	}

	console.restore_default_text_color();

	if(athome)
	{
		for(unsigned i=floors; i<20; ++i)
			for(unsigned j=0; j<lines_per_floor; ++j)
				console << endl_clear;
	}

	if(lines_per_floor<3)
		cout  << "Note: increase window height for even fancier output!" << endl;
}

Spire::PrintNum Spire::print_num(Number num) const
{
	return PrintNum(num, raw_values);
}

Number Spire::damage_score(const Layout &layout)
{
	return layout.get_damage();
}

Number Spire::income_score(const Layout &layout)
{
	return layout.get_runestones_per_second();
}

template<Pool::ScoreFunc *base_func, uint32_t towers_mask>
Number Spire::towers_score(const Layout &layout)
{
	Number count = 0;
	for(const char c: layout.get_traps())
		if(towers_mask&(1<<(c-'A')))
			++count;
	return (base_func(layout)>>6) + (count<<(sizeof(Number)*8-6));
}

template<Pool::ScoreFunc *base_func>
Pool::ScoreFunc *Spire::get_towers_score_func(char tower, bool exclude)
{
	if(tower=='S')
	{
		if(exclude)
			return &towers_score<base_func, 0x00404>;
		else
			return &towers_score<base_func, 0x40000>;
	}
	else if(tower=='C')
	{
		if(exclude)
			return &towers_score<base_func, 0x40400>;
		else
			return &towers_score<base_func, 0x00004>;
	}
	else if(tower=='K')
	{
		if(exclude)
			return &towers_score<base_func, 0x40004>;
		else
			return &towers_score<base_func, 0x00400>;
	}
	else
		return &towers_score<base_func, 0x40404>;
}

void Spire::sighandler(int)
{
	instance->intr_flag = true;
}


Spire::Worker::Worker(Spire &s, unsigned e, bool p):
	spire(s),
	random(e),
	state(p ? PAUSED : WORKING),
	thread(&Worker::main, this)
{
}

void Spire::Worker::interrupt()
{
	lock_guard<mutex> lock(state_mutex);
	state = INTERRUPT;
	state_cond.notify_one();
}

void Spire::Worker::set_paused(bool p)
{
	lock_guard<mutex> lock(state_mutex);
	if(p && state==WORKING)
		state = PAUSE_PENDING;
	else if(!p && (state==PAUSE_PENDING || state==PAUSED))
		state = WORKING;
	state_cond.notify_one();
}

void Spire::Worker::wait_paused()
{
	unique_lock<mutex> state_lock(state_mutex);
	while(state!=PAUSED && state!=INTERRUPT)
		state_cond.wait(state_lock);
}

void Spire::Worker::join()
{
	thread.join();
}

void Spire::Worker::main()
{
	unique_lock<mutex> pools_lock(spire.pools_mutex, defer_lock);
	while(1)
	{
		unique_lock<mutex> state_lock(state_mutex);
		if(state==PAUSE_PENDING)
		{
			state = PAUSED;
			state_cond.notify_one();
		}
		while(state==PAUSED)
			state_cond.wait(state_lock);
		if(state==INTERRUPT)
			break;
		state = WORKING;
		state_lock.unlock();

		unsigned cycle = spire.get_next_cycle();

		pools_lock.lock();
		unsigned pool_index = random()%spire.n_pools;
		Pool &pool = *spire.pools[pool_index];
		Layout base_layout = pool.get_random_layout(random);

		Layout cross_layout;
		bool do_cross = (random()%1000<spire.cross_rate);
		if(do_cross || spire.heterogeneous)
		{
			Pool *cross_pool = &pool;
			bool do_foreign = (random()%1000<spire.foreign_rate);
			if(do_foreign)
			{
				unsigned cross_index = random()%(spire.n_pools-1);
				if(cross_index==pool_index)
					++cross_index;
				cross_pool = spire.pools[cross_index];
			}
			pools_lock.unlock();

			if(do_cross || do_foreign)
			{
				cross_layout = cross_pool->get_random_layout(random);
				if(!do_cross)
					base_layout.set_traps(cross_layout.get_traps(), base_layout.get_traps().size()/5);
			}
		}
		else
			pools_lock.unlock();

		for(unsigned i=0; i<spire.loops_per_cycle; ++i)
		{
			Layout mutated = base_layout;
			if(do_cross)
				mutated.cross_from(cross_layout, random);

			unsigned cells = mutated.get_traps().size();
			unsigned mut_count = 1+random()%cells;
			mut_count = max((mut_count*mut_count)/cells, 1U);
			mutated.mutate(static_cast<Layout::MutateMode>(random()%3), mut_count, random, cycle);
			if(!mutated.is_valid())
				continue;

			mutated.update(Layout::COST_ONLY);
			if(mutated.get_cost()>spire.budget)
				continue;

			if(spire.core_rate && random()%1000<spire.core_rate)
			{
				Core core = mutated.get_core();
				core.mutate(spire.core_mutate, 1+random()%5, random);
				core.update();

				if(spire.validate_core(core))
					mutated.set_core(core);
			}

			mutated.update(spire.update_mode);
			pool.add_layout(mutated);
		}
	}
}


ostream &operator<<(ostream &os, const Spire::PrintNum &pn)
{
	if(pn.raw)
		return (os << pn.num);
	else
		return (os << NumberIO(pn.num));
}
