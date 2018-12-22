#include <signal.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include "getopt.h"
#include "network.h"
#include "spirelayout.h"

class Pool 
{
public:
	typedef std::uint64_t ScoreFunc(const Layout &);

private:
	unsigned max_size;
	ScoreFunc *score_func;
	std::list<Layout> layouts;
	mutable std::mutex layouts_mutex;

public:
	Pool(unsigned, ScoreFunc *);

	void add_layout(const Layout &);
	Layout get_best_layout() const;
	bool get_best_layout(Layout &) const;
	Layout get_random_layout(Layout::Random &) const;
	std::uint64_t get_best_score() const;

	template<typename F>
	void visit_layouts(const F &) const;
};

class Spire
{
private:
	class Worker
	{
	private:
		Spire &spire;
		Layout::Random random;
		bool intr_flag;
		std::thread thread;

	public:
		Worker(Spire &, unsigned);

		void interrupt();
		void join();

	private:
		void main();
	};

	unsigned n_pools;
	std::vector<Pool *> pools;
	std::mutex pools_mutex;
	unsigned prune_interval;
	unsigned next_prune;
	unsigned prune_limit;
	unsigned cross_rate;
	unsigned foreign_rate;
	bool heterogeneous;
	unsigned n_workers;
	std::list<Worker *> workers;
	unsigned loops_per_cycle;
	std::atomic<unsigned> cycle;
	bool debug_layout;
	bool numeric_format;
	bool show_pools;
	Network *network;
	Network::ConnectionTag connection;
	bool intr_flag;

	std::uint64_t budget;
	bool income;
	Pool::ScoreFunc *score_func;
	Layout start_layout;

	static Spire *instance;

public:
	Spire(int, char **);
	~Spire();

	int main();
private:
	unsigned get_next_cycle();
	void prune_pools();
	void report(const Layout &, const std::string &);
	bool print(const Layout &, unsigned &);
	static std::uint64_t damage_score(const Layout &);
	static std::uint64_t runestones_score(const Layout &);
	static void sighandler(int);
};

using namespace std;
using namespace std::placeholders;

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
	debug_layout(false),
	numeric_format(false),
	show_pools(false),
	network(0),
	connection(0),
	intr_flag(false),
	budget(1000000),
	income(false),
	score_func(damage_score)
{
	instance = this;

	unsigned pool_size = 100;
	unsigned n_pools_seen = 0;
	unsigned prune_interval_seen = 0;
	unsigned prune_limit_seen = 0;
	unsigned foreign_rate_seen = 0;
	unsigned floors = 7;
	unsigned floors_seen = 0;
	unsigned budget_seen = 0;
	string upgrades;
	string preset;
	bool online = false;

	GetOpt getopt;
	getopt.add_option('b', "budget", budget, GetOpt::REQUIRED_ARG).set_help("Maximum amount of runestones to spend", "NUM").bind_seen_count(budget_seen);
	getopt.add_option('f', "floors", floors, GetOpt::REQUIRED_ARG).set_help("Number of floors in the spire", "NUM").bind_seen_count(floors_seen);
	getopt.add_option("fire", start_layout.upgrades.fire, GetOpt::REQUIRED_ARG).set_help("Set fire trap upgrade level", "LEVEL");
	getopt.add_option("frost", start_layout.upgrades.frost, GetOpt::REQUIRED_ARG).set_help("Set frost trap upgrade level", "LEVEL");
	getopt.add_option("poison", start_layout.upgrades.poison, GetOpt::REQUIRED_ARG).set_help("Set poison trap upgrade level", "LEVEL");
	getopt.add_option("lightning", start_layout.upgrades.lightning, GetOpt::REQUIRED_ARG).set_help("Set lightning trap upgrade level", "LEVEL");
	getopt.add_option('u', "upgrades", upgrades, GetOpt::REQUIRED_ARG).set_help("Set all trap upgrade levels", "NNNN");
	getopt.add_option('n', "numeric-format", numeric_format, GetOpt::NO_ARG).set_help("Output layouts in numeric format");
	getopt.add_option('i', "income", income, GetOpt::NO_ARG).set_help("Optimize runestones per second");
	getopt.add_option("online", online, GetOpt::NO_ARG).set_help("Use the online build database");
	getopt.add_option('t', "preset", preset, GetOpt::REQUIRED_ARG).set_help("Select a preset to base settings on");
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
	getopt.add_argument("layout", start_layout.data, GetOpt::OPTIONAL_ARG).set_help("Layout to start with");
	getopt(argc, argv);

	if(floors<1)
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

	if(!n_pools_seen && heterogeneous)
		n_pools = 21;
	if(income)
		score_func = runestones_score;
	pools.reserve(n_pools);
	for(unsigned i=0; i<n_pools; ++i)
		pools.push_back(new Pool(pool_size, score_func));
	if(n_pools==1)
	{
		foreign_rate = 0;
		prune_interval = 0;
	}
	if(n_pools<=prune_limit)
		prune_interval = 0;
	if(prune_interval)
		next_prune = prune_interval;

	if(!start_layout.data.empty())
	{
		string::size_type plus = start_layout.data.find('+');
		if(plus!=string::npos)
		{
			upgrades = start_layout.data.substr(plus+1);
			start_layout.data.erase(plus);
			plus = upgrades.find('+');
			if(plus!=string::npos)
				upgrades.erase(plus);
		}
	}

	if(!upgrades.empty())
	{
		try
		{
			start_layout.upgrades = upgrades;
		}
		catch(const exception &)
		{
			throw usage_error("Upgrades string must consist of four numbers");
		}
	}

	if(start_layout.upgrades.fire>8)
		throw usage_error("Invalid fire trap upgrade level");
	if(start_layout.upgrades.frost>6)
		throw usage_error("Invalid frost trap upgrade level");
	if(start_layout.upgrades.poison>7)
		throw usage_error("Invalid poison trap upgrade level");
	if(start_layout.upgrades.lightning>6)
		throw usage_error("Invalid lightning trap upgrade level");

	if(!start_layout.data.empty())
	{
		string clean_data;
		clean_data.reserve(start_layout.data.size());
		for(char c: start_layout.data)
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
		start_layout.data = clean_data;

		if(!floors_seen)
			floors = max<unsigned>((start_layout.data.size()+4)/5, 1U);

		start_layout.data.resize(floors*5, '_');
		start_layout.update(income ? Layout::FULL : Layout::COMPATIBLE);
		pools.front()->add_layout(start_layout);

		if(!budget_seen)
			budget = start_layout.cost;
	}

	uint8_t downgrade[4] = { };
	for(unsigned i=0; i<pools.size(); ++i)
	{
		Layout empty;
		empty.upgrades = start_layout.upgrades;

		unsigned reduce = 0;
		for(unsigned j=0; j<sizeof(downgrade); ++j)
		{
			if(downgrade[j]==1 && empty.upgrades.fire>1)
				--empty.upgrades.fire;
			else if(downgrade[j]==2 && empty.upgrades.frost>1)
				--empty.upgrades.frost;
			else if(downgrade[j]==3 && empty.upgrades.poison>1)
				--empty.upgrades.poison;
			else if(downgrade[j]==4 && empty.upgrades.lightning>1)
				--empty.upgrades.lightning;
			else if(downgrade[j]==5 && reduce+1<floors)
				++reduce;
		}

		empty.data = string((floors-reduce)*5, '_');
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

	if(online)
	{
		network = new Network;
		try
		{
			connection = network->connect("spiredb.tdb.fi", 8676);
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
		vector<Step> steps;
		start_layout.build_steps(steps);
		start_layout.simulate(steps, start_layout.damage, true);
		cout << "Threat: " << start_layout.threat << endl;
		cout << "Runestones: " << start_layout.rs_per_sec << "/s" << endl;
		return 0;
	}

	Layout best_layout = pools.front()->get_best_layout();
	if(best_layout.damage && !show_pools)
		report(best_layout, "Initial layout");

	if(network)
	{
		bool submit = true;

		cout << "Querying online build database for best known layout" << endl;
		unsigned floors = best_layout.data.size()/5;
		string reply = network->communicate(connection, format("query %s %s %s %s", best_layout.upgrades.str(), floors, budget, (income ? "income" : "damage")));
		vector<string> parts = split(reply);
		if(parts[0]=="ok")
		{
			Layout layout;
			layout.upgrades = best_layout.upgrades;
			layout.data = parts[2];
			layout.data.resize(floors*5, '_');
			layout.update(income ? Layout::FULL : Layout::COMPATIBLE);

			submit = (score_func(best_layout)>score_func(layout) || best_layout.cost<layout.cost);
			if(layout.damage>best_layout.damage)
			{
				best_layout = layout;
				pools.front()->add_layout(best_layout);
				if(!show_pools)
					report(best_layout, "Layout from database");
			}
			else
				cout << "No better layout found" << endl;
		}

		if(submit)
			network->send_message(connection, format("submit %s %s", best_layout.upgrades.str(), best_layout.data));
	}

	signal(SIGINT, sighandler);

	Layout::Random random;
	for(unsigned i=0; i<n_workers; ++i)
		workers.push_back(new Worker(*this, random()));

	if(show_pools)
		cout << "\033[1;1H\033[2J";

	chrono::steady_clock::time_point period_start_time = chrono::steady_clock::now();
	unsigned period_start_cycle = cycle;
	while(!intr_flag)
	{
		this_thread::sleep_for(chrono::milliseconds(500));

		if(intr_flag)
		{
			for(auto w: workers)
				w->interrupt();
			for(auto w: workers)
				w->join();
		}

		uint64_t best_score = score_func(best_layout);
		for(auto *p: pools)
		{
			p->get_best_layout(best_layout);
			if(heterogeneous)
				break;
		}

		if(score_func(best_layout)>best_score)
		{
			best_layout.update(income ? Layout::FULL : Layout::COMPATIBLE);
			if(!show_pools)
				report(best_layout, "New best layout found");
			if(network)
				network->send_message(connection, format("submit %s %s", best_layout.upgrades.str(), best_layout.data));
		}

		if(show_pools)
		{
			chrono::steady_clock::time_point period_end_time = chrono::steady_clock::now();
			unsigned period_end_cycle = cycle;
			unsigned loops_per_sec = loops_per_cycle*(period_end_cycle-period_start_cycle)/chrono::duration<float>(period_end_time-period_start_time).count();
			period_start_time = period_end_time;
			period_start_cycle = period_end_cycle;

			cout << "\033[1;1H";
			unsigned n_print = 100/n_pools-1;
			for(unsigned i=0; i<n_pools; ++i)
			{
				unsigned count = n_print;
				pools[i]->visit_layouts(bind(&Spire::print, this, _1, ref(count)));
				if(n_print>1)
				{
					for(++count; count>0; --count)
						cout << "\033[K" << endl;
				}
			}

			cout << "\033[K" << loops_per_sec << " loops/sec" << endl;
		}

		if(next_prune && cycle>=next_prune)
			prune_pools();
	}

	for(auto w: workers)
		delete w;

	return 0;
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
	uint64_t score = pools[lowest]->get_best_score();
	for(unsigned i=0; i<n_pools; ++i)
	{
		uint64_t s = pools[i]->get_best_score();
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
	cout << message << " (" << layout.damage << " damage, " << layout.threat << " threat, " << layout.rs_per_sec << " Rs/s, cost " << layout.cost << " Rs, cycle " << layout.cycle << "):" << endl;
	unsigned count = 1;
	print(layout, count);
}

bool Spire::print(const Layout &layout, unsigned &count)
{
	unsigned cells = layout.data.size();
	string descr;
	if(numeric_format)
	{
		descr.reserve(cells+8);
		for(unsigned i=0; i<cells; ++i)
			for(unsigned j=0; Layout::traps[j]; ++j)
				if(layout.data[i]==Layout::traps[j])
					descr += '0'+j;
		descr += '+';
		descr += layout.upgrades.str();
		descr += '+';
		descr += stringify(cells/5);
	}
	else
	{
		descr.reserve(5+cells+cells/5-1);
		descr += layout.upgrades.str();
		for(unsigned i=0; i<cells; i+=5)
		{
			descr += ' ';
			descr.append(layout.data.substr(i, 5));
		}
	}

	if(show_pools)
		cout << "\033[K" << descr << ' ' << score_func(layout) << ' ' << layout.cost << ' ' << layout.cycle << endl;
	else
		cout << descr << endl;

	return (count && --count);
}

uint64_t Spire::damage_score(const Layout &layout)
{
	return layout.damage;
}

uint64_t Spire::runestones_score(const Layout &layout)
{
	return layout.rs_per_sec;
}

void Spire::sighandler(int)
{
	instance->intr_flag = true;
}


Pool::Pool(unsigned s, ScoreFunc *f):
	max_size(s),
	score_func(f)
{ }

void Pool::add_layout(const Layout &layout)
{
	lock_guard<mutex> lock(layouts_mutex);

	if(layouts.size()>=max_size && score_func(layout)<score_func(layouts.back()))
		return;

	uint64_t score = score_func(layout);
	auto i = layouts.begin();
	for(; (i!=layouts.end() && score_func(*i)>score); ++i)
		if(i->cost<=layout.cost)
			return;
	if(i!=layouts.end() && score_func(*i)==score)
	{
		*i = layout;
		++i;
	}
	else
		layouts.insert(i, layout);

	while(i!=layouts.end())
	{
		if(i->cost>=layout.cost)
			i = layouts.erase(i);
		else
			++i;
	}

	if(layouts.size()>max_size)
		layouts.pop_back();
}

Layout Pool::get_best_layout() const
{
	lock_guard<mutex> lock(layouts_mutex);
	return layouts.front();
}

bool Pool::get_best_layout(Layout &layout) const
{
	lock_guard<mutex> lock(layouts_mutex);
	if(score_func(layout)>=score_func(layouts.front()))
		return false;
	layout = layouts.front();
	return true;
}

Layout Pool::get_random_layout(Layout::Random &random) const
{
	lock_guard<mutex> lock(layouts_mutex);

	uint64_t total = 0;
	for(const auto &l: layouts)
		total += score_func(l);

	if(!total)
	{
		auto i = layouts.begin();
		advance(i, random()%layouts.size());
		return *i;
	}

	uint64_t p = ((static_cast<uint64_t>(random())<<32)+random())%total;
	for(const auto &l: layouts)
	{
		uint64_t score = score_func(l);
		if(p<score)
			return l;
		p -= score;
	}

	throw logic_error("Spire::get_random_layout");
}

uint64_t Pool::get_best_score() const
{
	lock_guard<mutex> lock(layouts_mutex);
	return score_func(layouts.front());
}

template<typename F>
void Pool::visit_layouts(const F &func) const
{
	lock_guard<mutex> lock(layouts_mutex);
	for(const auto &layout: layouts)
		if(!func(layout))
			return;
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
				{
					unsigned cells = base_layout.data.size();
					base_layout.data = cross_layout.data;
					base_layout.data.resize(cells, '_');
				}
			}
		}
		else
			pools_lock.unlock();

		for(unsigned i=0; i<spire.loops_per_cycle; ++i)
		{
			Layout mutated = base_layout;
			mutated.cycle = cycle;
			if(do_cross)
				mutated.cross_from(cross_layout, random);

			unsigned cells = mutated.data.size();
			unsigned mut_count = 1+random()%cells;
			mut_count = max((mut_count*mut_count)/cells, 1U);
			mutated.mutate(random()%3, mut_count, random);
			if(!mutated.is_valid())
				continue;

			mutated.update_cost();
			if(mutated.cost>spire.budget)
				continue;

			mutated.update(spire.income ? Layout::FULL : Layout::FAST);
			pool.add_layout(mutated);
		}
	}
}
