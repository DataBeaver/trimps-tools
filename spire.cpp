#include "spire.h"
#include <signal.h>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
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
}

Spire *Spire::instance;

Spire::Spire(int argc, char **argv):
	n_pools(10),
	prune_interval(0),
	next_prune(0),
	prune_limit(3),
	cross_rate(500),
	foreign_rate(500),
	heterogeneous(false),
	n_workers(4),
	loops_per_cycle(200),
	cycle(1),
	accuracy(12),
	debug_layout(false),
	numeric_format(false),
	raw_values(false),
	fancy_output(false),
	show_pools(false),
	network(0),
	connection(0),
	intr_flag(false),
	budget(0),
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
	string layout_str;
	string preset;
	bool online = false;
	NumberIO budget_in = budget;

	GetOpt getopt;
	getopt.add_option('b', "budget", budget_in, GetOpt::REQUIRED_ARG).set_help("Maximum amount of runestones to spend", "NUM");
	getopt.add_option('f', "floors", floors, GetOpt::REQUIRED_ARG).set_help("Number of floors in the spire", "NUM").bind_seen_count(floors_seen);
	getopt.add_option('u', "upgrades", upgrades, GetOpt::REQUIRED_ARG).set_help("Set all trap upgrade levels", "NNNN");
	getopt.add_option('n', "numeric-format", numeric_format, GetOpt::NO_ARG).set_help("Output layouts in numeric format");
	getopt.add_option('i', "income", income, GetOpt::NO_ARG).set_help("Optimize runestones per second");
	getopt.add_option("towers", towers, GetOpt::NO_ARG).set_help("Try to use as many towers as possible");
	getopt.add_option("online", online, GetOpt::NO_ARG).set_help("Use the online layout database");
	getopt.add_option('t', "preset", preset, GetOpt::REQUIRED_ARG).set_help("Select a preset to base settings on", "NAME");
	getopt.add_option("fancy", fancy_output, GetOpt::NO_ARG).set_help("Produce fancy output");
	getopt.add_option('a', "accuracy", accuracy, GetOpt::REQUIRED_ARG).set_help("Set simulation accuracy", "NUM");
	getopt.add_option('w', "workers", n_workers, GetOpt::REQUIRED_ARG).set_help("Number of threads to use", "NUM");
	getopt.add_option('l', "loops", loops_per_cycle, GetOpt::REQUIRED_ARG).set_help("Number of loops per cycle", "NUM");
	getopt.add_option('p', "pools", n_pools, GetOpt::REQUIRED_ARG).set_help("Number of population pools", "NUM").bind_seen_count(n_pools_seen);
	getopt.add_option('s', "pool-size", pool_size, GetOpt::REQUIRED_ARG).set_help("Size of each population pool", "NUM");
	getopt.add_option("prune-interval", prune_interval, GetOpt::REQUIRED_ARG).set_help("Interval for pruning pools, in cycles", "NUM").bind_seen_count(prune_interval_seen);
	getopt.add_option("prune-limit", prune_limit, GetOpt::REQUIRED_ARG).set_help("Minimum number of pools to keep", "NUM").bind_seen_count(prune_limit_seen);
	getopt.add_option("heterogeneous", heterogeneous, GetOpt::NO_ARG).set_help("Use heterogeneous pool configurations");
	getopt.add_option('c', "cross-rate", cross_rate, GetOpt::REQUIRED_ARG).set_help("Probability of crossing two layouts (out of 1000)", "NUM");
	getopt.add_option('o', "foreign-rate", foreign_rate, GetOpt::REQUIRED_ARG).set_help("Probability of crossing from another pool (out of 1000)", "NUM").bind_seen_count(foreign_rate_seen);
	getopt.add_option('g', "debug-layout", debug_layout, GetOpt::NO_ARG).set_help("Print detailed information about the layout");
	getopt.add_option("show-pools", show_pools, GetOpt::NO_ARG).set_help("Show population pool contents while running");
	getopt.add_option("raw-values", raw_values, GetOpt::NO_ARG).set_help("Display raw numeric values");
	getopt.add_argument("layout", layout_str, GetOpt::OPTIONAL_ARG).set_help("Layout to start with");
	getopt(argc, argv);

	budget = budget_in.value;

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

	if(preset=="single")
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

	if(income)
		score_func = (towers ? income_towers_score : income_score);
	else if(towers)
		score_func = damage_towers_score;

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

	init_start_layout(layout_str, upgrades, floors);
	init_pools(pool_size);

	if(!budget)
		budget = 1000000;

	if(online)
		init_network(refresh);
}

void Spire::init_start_layout(const string &layout_in, const string &upgrades_in, unsigned floors)
{
	string traps;
	string upgrades = upgrades_in;

	if(!layout_in.empty())
	{
		string::size_type plus = layout_in.find('+');
		traps = layout_in.substr(0, plus);
		if(plus!=string::npos && upgrades.empty())
		{
			upgrades = layout_in.substr(plus+1);
			plus = upgrades.find('+');
			if(plus!=string::npos)
				upgrades.erase(plus);
		}
	}

	if(!upgrades.empty())
	{
		try
		{
			start_layout.set_upgrades(upgrades);
		}
		catch(const exception &)
		{
			throw usage_error("Upgrades string must consist of four numbers");
		}
	}

	const TrapUpgrades &start_upgrades = start_layout.get_upgrades();
	if(start_upgrades.fire>8)
		throw usage_error("Invalid fire trap upgrade level");
	if(start_upgrades.frost>6)
		throw usage_error("Invalid frost trap upgrade level");
	if(start_upgrades.poison>7)
		throw usage_error("Invalid poison trap upgrade level");
	if(start_upgrades.lightning>6)
		throw usage_error("Invalid lightning trap upgrade level");

	if(!traps.empty())
	{
		string clean_data;
		clean_data.reserve(traps.size());
		for(char c: traps)
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

		start_layout.set_traps(clean_data, floors);
		start_layout.update((income || debug_layout) ? Layout::FULL : Layout::COMPATIBLE, accuracy);

		if(!budget)
			budget = start_layout.get_cost();
	}
	else
		start_layout.set_traps(string(), (floors ? floors : 7));
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

void Spire::init_network(unsigned refresh)
{
	network = new Network;
	try
	{
		connection = network->connect("spiredb.tdb.fi", 8676);
		if(refresh)
		{
			refresh_interval = chrono::duration<unsigned>(refresh);
			next_refresh = chrono::steady_clock::now()+refresh_interval;
		}
	}
	catch(const exception &e)
	{
		cout << "Can't connect to online build database:" << endl;
		cout << e.what() << endl;
		cout << "Continuing in offline mode" << endl;
		delete network;
		network = 0;
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
		cout << "Threat: " << start_layout.get_threat() << endl;
		cout << "Runestones: " << start_layout.get_runestones_per_second() << "/s" << endl;
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

	if(network)
	{
		if(!fancy_output)
			cout << "Querying online database for best known layout" << endl;
		if(query_network())
		{
			if(!show_pools)
				report(best_layout, "Layout from database");
		}
		else
		{
			if(!fancy_output)
				cout << "Database returned no better layout" << endl;

			if(score_func(best_layout)>0)
				network->send_message(connection, format("submit %s %s", best_layout.get_upgrades().str(), best_layout.get_traps()));
		}
	}

	signal(SIGINT, sighandler);

	Random random;
	for(unsigned i=0; i<n_workers; ++i)
		workers.push_back(new Worker(*this, random()));

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
	if(!network)
		return false;

	unsigned floors = best_layout.get_traps().size()/5;
	string reply = network->communicate(connection, format("query %s %s %s %s", best_layout.get_upgrades().str(), floors, budget, (income ? "income" : "damage")));
	vector<string> parts = split(reply);
	if(parts[0]=="ok")
	{
		Layout layout;
		layout.set_upgrades(best_layout.get_upgrades());
		layout.set_traps(parts[2], floors);
		layout.update(income ? Layout::FULL : Layout::COMPATIBLE, accuracy);

		if(score_func(layout)>score_func(best_layout))
		{
			best_layout = layout;
			pools.front()->add_layout(best_layout);
			return true;
		}
	}

	return false;
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
		best_layout.update(income ? Layout::FULL : Layout::COMPATIBLE, accuracy);
		if(network)
			network->send_message(connection, format("submit %s %s", best_layout.get_upgrades().str(), best_layout.get_traps()));
	}

	if(next_prune && cycle>=next_prune)
		prune_pools();

	return new_best;
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
					cout << endl;
				}
			}
		}

		console.clear_current_line();
		cout << loops_per_second << " loops/sec" << endl;
	}
	else
	{
		if(new_best_found)
			report(best_layout, "New best layout found");
		else
		{
			console.set_cursor_position(69, 13);
			cout << cycle;
			console.set_cursor_position(69, 14);
			cout << NumberIO(loops_per_second) << "    ";
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
	for(unsigned i=0; i<n_pools; ++i)
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

void Spire::report(const Layout &layout, const string &message)
{
	if(fancy_output)
	{
		console.set_cursor_position(0, 0);
		console.clear_current_line();
	}

	time_t t = chrono::system_clock::to_time_t(chrono::system_clock::now());
	struct tm lt;
	cout << '[' << put_time(localtime_r(&t, &lt), "%Y-%m-%d %H:%M:%S") << "] " << message;
	if(!fancy_output)
		cout << " (" << print_num(layout.get_damage()) << " damage, " << layout.get_threat() << " threat, "
			<< print_num(layout.get_runestones_per_second()) << " Rs/s, cost " << print_num(layout.get_cost())
			<< " Rs, cycle " << layout.get_cycle() << "):";
	cout << endl << "  ";
	unsigned count = 1;
	print(layout, count);

	if(fancy_output)
	{
		print_fancy(layout);
		console.set_cursor_position(58, 4);
		cout << "Mode:   " << (income ? "income" : "damage");
		if(towers)
			cout << "+towers";
		console.set_cursor_position(58, 5);
		cout << "Budget: " << print_num(budget) << " Rs";
		console.set_cursor_position(58, 7);
		cout << "Damage: " << print_num(layout.get_damage()) << "    ";
		console.set_cursor_position(58, 8);
		cout << "Threat: " << layout.get_threat();
		console.set_cursor_position(58, 9);
		cout << "Income: " << print_num(layout.get_runestones_per_second()) << " Rs/s    ";
		console.set_cursor_position(58, 10);
		cout << "Cost:   " << print_num(layout.get_cost()) << " Rs    ";
		console.set_cursor_position(58, 11);
		cout << "Cycle:  " << layout.get_cycle();
		console.set_cursor_position(58, 13);
		cout << "Cycle now: " << cycle;
		console.set_cursor_position(58, 14);
		cout << "Speed:     " << NumberIO(loops_per_second);
		cout << endl;
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
		cout << descr << ' ' << score_func(layout) << ' ' << layout.get_cost() << ' ' << layout.get_cycle() << endl;
	}
	else
		cout << descr << endl;

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
		cout << topleft;
		for(unsigned i=0; i<5; ++i)
			cout << topline;
		cout << endl;
	}

	for(unsigned i=floors*lines_per_floor; i-->0; )
	{
		console.set_text_color(border_color, 0);
		cout << vsep;

		unsigned line = lines_per_floor-1-i%lines_per_floor;
		for(unsigned j=0; j<5; ++j)
		{
			const FancyCell &fc = fancy[(i/lines_per_floor)*5+j];
			if(line==2)
			{
				console.set_text_color(border_color, fc.bg_color[line]);
				cout << hsep;
			}
			else
			{
				console.set_text_color(fc.text_color, fc.bg_color[line]);
				cout << (line==0 ? fc.line1 : fc.line2);
				console.set_text_color(border_color, fc.bg_color[line]);
				cout << vsep;
			}
		}

		cout << endl;
	}

	console.restore_default_text_color();

	if(lines_per_floor<3)
		cout  << "Note: incerase window height for even fancier output!" << endl;
}

Spire::PrintNum Spire::print_num(Number num) const
{
	return PrintNum(num, raw_values);
}

Number Spire::damage_score(const Layout &layout)
{
	return layout.get_damage();
}

Number Spire::damage_towers_score(const Layout &layout)
{
	return damage_score(layout)*get_towers_multiplier(layout);
}

Number Spire::income_score(const Layout &layout)
{
	return layout.get_runestones_per_second();
}

Number Spire::income_towers_score(const Layout &layout)
{
	return income_score(layout)*get_towers_multiplier(layout);
}

unsigned Spire::get_towers_multiplier(const Layout &layout)
{
	unsigned mul = 2;
	for(char c: layout.get_traps())
		if(c=='S' || c=='C' || c=='K')
			mul = mul*3/2;
	return mul;
}

void Spire::sighandler(int)
{
	instance->intr_flag = true;
}


Spire::Worker::Worker(Spire &s, unsigned e):
	spire(s),
	random(e),
	intr_flag(false),
	thread(&Worker::main, this)
{
}

void Spire::Worker::interrupt()
{
	intr_flag = true;
}

void Spire::Worker::join()
{
	thread.join();
}

void Spire::Worker::main()
{
	unique_lock<mutex> pools_lock(spire.pools_mutex, defer_lock);
	while(!intr_flag)
	{
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

			mutated.update(spire.income ? Layout::FULL : Layout::FAST, spire.accuracy);
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
