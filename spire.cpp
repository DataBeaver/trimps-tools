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

typedef std::minstd_rand Random;

struct Layout
{
	struct
	{
		uint16_t fire;
		uint16_t frost;
		uint16_t poison;
		uint16_t lightning;
	} upgrades;
	std::string data;
	std::uint64_t damage;
	std::uint64_t cost;
	unsigned cycle;

	Layout();
};

class Pool 
{
private:
	unsigned max_size;
	std::list<Layout> layouts;
	mutable std::mutex mutex;

public:
	Pool(unsigned);

	void add_layout(const Layout &);
	Layout get_best_layout() const;
	Layout get_random_layout(Random &) const;
	std::uint64_t get_highest_damage() const;
	std::uint64_t get_lowest_damage() const;

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
		Random random;
		bool intr_flag;
		std::thread thread;

	public:
		Worker(Spire &, unsigned);

		void interrupt();
		void join();

	private:
		void main();
	};

	struct TrapEffects
	{
		unsigned fire_damage;
		unsigned frost_damage;
		unsigned chill_dur;
		unsigned poison_damage;
		unsigned lightning_damage;
		unsigned shock_dur;
		unsigned damage_multi;
		unsigned special_multi;
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
	bool intr_flag;

	std::uint64_t budget;
	Layout start_layout;

	static Spire *instance;
	static const char traps[];

public:
	Spire(int, char **);
	~Spire();

	int main();
private:
	unsigned get_next_cycle();
	void prune_pools();
	void calculate_effects(const Layout &, TrapEffects &) const;
	std::uint64_t simulate(const Layout &, bool = false) const;
	std::uint64_t simulate_with_hp(const Layout &, uint64_t, bool = false) const;
	std::uint64_t calculate_cost(const std::string &) const;
	void cross(std::string &, const std::string &, Random &) const;
	void mutate(Layout &, unsigned, unsigned, Random &) const;
	bool is_valid(const std::string &) const;
	void report(const Layout &, const std::string &);
	bool print(const Layout &, unsigned &);
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
const char Spire::traps[] = "_FZPLSCK";

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
	intr_flag(false),
	budget(1000000)
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

	GetOpt getopt;
	getopt.add_option('b', "budget", budget, GetOpt::REQUIRED_ARG).set_help("Maximum amount of runestones to spend", "NUM").bind_seen_count(budget_seen);
	getopt.add_option('f', "floors", floors, GetOpt::REQUIRED_ARG).set_help("Number of floors in the spire", "NUM").bind_seen_count(floors_seen);
	getopt.add_option("fire", start_layout.upgrades.fire, GetOpt::REQUIRED_ARG).set_help("Set fire trap upgrade level", "LEVEL");
	getopt.add_option("frost", start_layout.upgrades.frost, GetOpt::REQUIRED_ARG).set_help("Set frost trap upgrade level", "LEVEL");
	getopt.add_option("poison", start_layout.upgrades.poison, GetOpt::REQUIRED_ARG).set_help("Set poison trap upgrade level", "LEVEL");
	getopt.add_option("lightning", start_layout.upgrades.lightning, GetOpt::REQUIRED_ARG).set_help("Set lightning trap upgrade level", "LEVEL");
	getopt.add_option('u', "upgrades", upgrades, GetOpt::REQUIRED_ARG).set_help("Set all trap upgrade levels", "NNNN");
	getopt.add_option('n', "numeric-format", numeric_format, GetOpt::NO_ARG).set_help("Output layouts in numeric format");
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
	pools.reserve(n_pools);
	for(unsigned i=0; i<n_pools; ++i)
		pools.push_back(new Pool(pool_size));
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
		bool valid = (upgrades.size()==4);
		for(auto i=upgrades.begin(); (valid && i!=upgrades.end()); ++i)
			valid = isdigit(*i);
		if(!valid)
			throw usage_error("Upgrades string must consist of four numbers");

		start_layout.upgrades.fire = upgrades[0]-'0';
		start_layout.upgrades.frost = upgrades[1]-'0';
		start_layout.upgrades.poison = upgrades[2]-'0';
		start_layout.upgrades.lightning = upgrades[3]-'0';
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
				clean_data += traps[c-'0'];
			else if(c!=' ')
				clean_data += c;
		}
		start_layout.data = clean_data;

		if(!floors_seen)
			floors = max<unsigned>((start_layout.data.size()+4)/5, 1U);

		start_layout.data.resize(floors*5, '_');
		start_layout.damage = simulate(start_layout);
		start_layout.cost = calculate_cost(start_layout.data);
		pools.front()->add_layout(start_layout);

		if(!budget_seen)
			budget = start_layout.cost;
	}

	unsigned downgrade = 0;
	for(unsigned i=0; i<pools.size(); ++i)
	{
		Layout empty;
		empty.upgrades = start_layout.upgrades;
		unsigned reduce = 0;
		if(downgrade>0)
		{
			for(unsigned dg=downgrade; dg; dg/=4)
			{
				unsigned t = --dg%4;
				if(t==0 && empty.upgrades.fire>1)
					--empty.upgrades.fire;
				if(t==1 && empty.upgrades.frost>1)
					--empty.upgrades.frost;
				if(t==2 && empty.upgrades.poison>1)
					--empty.upgrades.poison;
				if(t==3 && empty.upgrades.lightning>1)
					--empty.upgrades.lightning;
				++reduce;
			}
		}
		empty.data = string((floors-reduce)*5, '_');
		pools[i]->add_layout(empty);
		if(heterogeneous)
			++downgrade;
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
		uint64_t damage = simulate(start_layout, true);
		cout << "Total damage: " << damage << endl;
		return 0;
	}

	signal(SIGINT, sighandler);

	Layout best_layout = pools.front()->get_best_layout();

	Random random;
	for(unsigned i=0; i<n_workers; ++i)
		workers.push_back(new Worker(*this, random()));

	if(show_pools)
		cout << "\033[1;1H\033[2J";

	if(best_layout.damage && !show_pools)
		report(best_layout, "Initial layout");

	unsigned n_print = 100/n_pools-1;
	while(!intr_flag)
	{
		std::this_thread::sleep_for(chrono::milliseconds(500));
		if(show_pools)
		{
			cout << "\033[1;1H";
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
		}
		else
		{
			unsigned best_damage = best_layout.damage;
			for(auto *p: pools)
			{
				if(p->get_highest_damage()>best_layout.damage)
					best_layout = p->get_best_layout();
				if(heterogeneous)
					break;
			}

			if(best_layout.damage>best_damage)
				report(best_layout, "New best layout found");
		}

		if(next_prune && cycle>=next_prune)
		{
			prune_pools();
			n_print = 100/n_pools-1;
		}
	}

	for(auto w: workers)
		w->interrupt();
	for(auto w: workers)
	{
		w->join();
		delete w;
	}

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

	lock_guard<std::mutex> lock(pools_mutex);
	unsigned lowest = 0;
	if(heterogeneous)
		++lowest;
	uint64_t damage = pools[lowest]->get_highest_damage();
	for(unsigned i=0; i<n_pools; ++i)
	{
		uint64_t d = pools[i]->get_highest_damage();
		if(d<damage)
		{
			lowest = i;
			damage = d;
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

void Spire::calculate_effects(const Layout &layout, TrapEffects &effects) const
{
	effects.fire_damage = 50;
	effects.frost_damage = 10;
	effects.chill_dur = 3;
	effects.poison_damage = 5;
	effects.lightning_damage = 50;
	effects.shock_dur = 1;
	effects.damage_multi = 2;
	effects.special_multi = 2;

	if(layout.upgrades.fire>=2)
		effects.fire_damage *= 10;
	if(layout.upgrades.fire>=3)
		effects.fire_damage *= 5;
	if(layout.upgrades.fire>=4)
		effects.fire_damage *= 2;
	if(layout.upgrades.fire>=5)
		effects.fire_damage *= 2;
	if(layout.upgrades.fire>=6)
		effects.fire_damage *= 10;
	if(layout.upgrades.fire>=7)
		effects.fire_damage *= 10;
	if(layout.upgrades.fire>=8)
		effects.fire_damage *= 100;

	if(layout.upgrades.frost>=2)
	{
		++effects.chill_dur;
		effects.frost_damage *= 5;
	}
	if(layout.upgrades.frost>=3)
		effects.frost_damage *= 10;
	if(layout.upgrades.frost>=4)
		effects.frost_damage *= 5;
	if(layout.upgrades.frost>=5)
		effects.frost_damage *= 2;
	if(layout.upgrades.frost>=6)
	{
		++effects.chill_dur;
		effects.frost_damage *= 5;
	}

	if(layout.upgrades.poison>=2)
		effects.poison_damage *= 2;
	if(layout.upgrades.poison>=4)
		effects.poison_damage *= 2;
	if(layout.upgrades.poison>=5)
		effects.poison_damage *= 2;
	if(layout.upgrades.poison>=6)
		effects.poison_damage *= 2;
	if(layout.upgrades.poison>=7)
		effects.poison_damage *= 2;

	if(layout.upgrades.lightning>=2)
	{
		effects.lightning_damage *= 10;
		++effects.shock_dur;
	}
	if(layout.upgrades.lightning>=3)
	{
		effects.lightning_damage *= 10;
		effects.damage_multi *= 2;
	}
	if(layout.upgrades.lightning>=5)
	{
		effects.lightning_damage *= 10;
		++effects.shock_dur;
	}
	if(layout.upgrades.lightning>=6)
	{
		effects.lightning_damage *= 10;
		effects.damage_multi *= 2;
	}
}

uint64_t Spire::simulate(const Layout &layout, bool debug) const
{
	uint64_t damage = simulate_with_hp(layout, 0, (debug && layout.upgrades.poison<5));
	if(layout.upgrades.poison>=5)
	{
		uint64_t low = damage;
		uint64_t high = simulate_with_hp(layout, low, false);
		if(debug)
		{
			cout << "Initial damage: " << low << endl;
			cout << "Maximum damage: " << high << endl;
		}
		for(unsigned i=0; (i<10 && low*101<high*100); ++i)
		{
			uint64_t mid = (low+high*3)/4;
			damage = simulate_with_hp(layout, mid, false);
			if(damage>mid)
				low = mid;
			else
			{
				high = mid;
				low = damage;
			}
		}
		if(debug)
			simulate_with_hp(layout, low, true);
		return low;
	}
	else
		return damage;
}

uint64_t Spire::simulate_with_hp(const Layout &layout, uint64_t max_hp, bool debug) const
{
	unsigned slots = layout.data.size();

	uint8_t column_flags[5] = { };
	for(unsigned i=0; i<slots; ++i)
		if(layout.data[i]=='L')
			++column_flags[i%5];

	std::vector<uint16_t> floor_flags(slots/5, 0);
	for(unsigned i=0; i<slots; ++i)
	{
		unsigned j = i/5;
		char t = layout.data[i];
		if(t=='F')
			floor_flags[j] += 1+0x10*column_flags[i%5];
		else if(t=='S')
			floor_flags[j] |= 0x08;
	}

	TrapEffects effects;
	calculate_effects(layout, effects);

	unsigned chilled = 0;
	unsigned frozen = 0;
	unsigned shocked = 0;
	unsigned damage_multi = 1;
	unsigned special_multi = 1;
	uint64_t poison = 0;
	uint64_t damage = 0;
	uint64_t last_fire = 0;
	unsigned step = 0;
	for(unsigned i=0; i<slots; )
	{
		char t = layout.data[i];
		bool antifreeze = false;
		if(t=='Z')
		{
			damage += effects.frost_damage*damage_multi;
			chilled = effects.chill_dur*special_multi+1;
			frozen = 0;
			antifreeze = true;
		}
		else if(t=='F')
		{
			unsigned d = effects.fire_damage*damage_multi;
			if(floor_flags[i/5]&0x08)
				d *= 2;
			if(chilled && layout.upgrades.frost>=3)
				d = d*5/4;
			if(layout.upgrades.lightning>=4)
				d = d*(10+column_flags[i%5])/10;
			damage += d;
			last_fire = damage;
		}
		else if(t=='P')
		{
			unsigned p = effects.poison_damage*damage_multi;
			if(layout.upgrades.frost>=4 && i+1<slots && layout.data[i+1]=='Z')
				p *= 4;
			if(layout.upgrades.poison>=3)
			{
				if(i>0 && layout.data[i-1]=='P')
					p *= 3;
				if(i+1<slots && layout.data[i+1]=='P')
					p *= 3;
			}
			if(layout.upgrades.poison>=5 && max_hp && damage*4>=max_hp)
				p *= 5;
			if(layout.upgrades.lightning>=4)
				p = p*(10+column_flags[i%5])/10;
			poison += p;
		}
		else if(t=='L')
		{
			damage += effects.lightning_damage*damage_multi;
			shocked = effects.shock_dur+1;
			damage_multi = effects.damage_multi;
			special_multi = effects.special_multi;
		}
		else if(t=='S')
		{
			uint16_t flags = floor_flags[i/5];
			uint64_t d = effects.fire_damage*(flags&0x07);
			if(layout.upgrades.lightning>=4)
				d += effects.fire_damage*(flags>>4)/10;
			d *= 2*damage_multi;
			if(chilled && layout.upgrades.frost>=3)
				d = d*5/4;
			damage += d;
		}
		else if(t=='K')
		{
			if(chilled)
			{
				chilled = 0;
				frozen = 5*special_multi+1;
			}
			antifreeze = true;
		}
		else if(t=='C')
			poison = poison*(4+special_multi)/4;

		damage += poison;

		if(debug)
		{
			cout << setw(2) << i << ':' << step << ": " << t << ' ' << setw(9) << damage;
			if(max_hp)
			{
				if(damage>max_hp)
					cout << "  0%";
				else if(damage)
					cout << ' ' << setw(2) << (max_hp-damage)*100/max_hp << '%';
				else
					cout << " **%";
			}
			else
				cout << "    ";
			if(poison)
				cout << " P" << setw(6) << poison;
			else
				cout << "        ";
			if(frozen)
				cout << " F" << setw(2) << frozen;
			else if(chilled)
				cout << " C" << setw(2) << chilled;
			else
				cout << "    ";
			if(shocked)
				cout << " S" << shocked;
			cout << endl;
		}

		++step;
		if(shocked)
		{
			if(!--shocked)
			{
				damage_multi = 1;
				special_multi = 1;
			}
		}
		if(!antifreeze)
		{
			if(chilled && step<2)
				continue;
			if(frozen && step<3)
				continue;
		}

		if(chilled)
			--chilled;
		if(frozen)
			--frozen;
		step = 0;
		++i;
	}

	if(layout.upgrades.fire>=4)
		return max(damage, last_fire*5/4);
	else
		return damage;
}

uint64_t Spire::calculate_cost(const std::string &data) const
{
	uint64_t fire_cost = 100;
	uint64_t frost_cost = 100;
	uint64_t poison_cost = 500;
	uint64_t lightning_cost = 1000;
	uint64_t strength_cost = 3000;
	uint64_t condenser_cost = 6000;
	uint64_t knowledge_cost = 9000;
	uint64_t cost = 0;
	for(char t: data)
	{
		uint64_t prev_cost = cost;
		if(t=='F')
		{
			cost += fire_cost;
			fire_cost = fire_cost*3/2;
		}
		else if(t=='Z')
		{
			cost += frost_cost;
			frost_cost *= 5;
		}
		else if(t=='P')
		{
			cost += poison_cost;
			poison_cost = poison_cost*7/4;
		}
		else if(t=='L')
		{
			cost += lightning_cost;
			lightning_cost *= 3;
		}
		else if(t=='S')
		{
			cost += strength_cost;
			strength_cost *= 100;
		}
		else if(t=='C')
		{
			cost += condenser_cost;
			condenser_cost *= 100;
		}
		else if(t=='K')
		{
			cost += knowledge_cost;
			knowledge_cost *= 100;
		}

		if(cost<prev_cost)
			return numeric_limits<uint64_t>::max();
	}

	return cost;
}

void Spire::cross(std::string &data1, const std::string &data2, Random &random) const
{
	unsigned slots = min(data1.size(), data2.size());
	for(unsigned i=0; i<slots; ++i)
		if(random()&1)
			data1[i] = data2[i];
}

void Spire::mutate(Layout &layout, unsigned mode, unsigned count, Random &random) const
{
	unsigned slots = layout.data.size();
	unsigned locality = (slots>=10 ? random()%(slots*2/15) : 0);
	unsigned base = 0;
	if(locality)
	{
		slots -= locality*5;
		base = (random()%locality)*5;
	}

	unsigned traps_count = (layout.upgrades.lightning>0 ? 7 : 5);
	for(unsigned i=0; i<count; ++i)
	{
		unsigned op = 0;  // replace only
		if(mode==1)       // permute only
			op = 1+random()%4;
		else if(mode==2)  // any operation
			op = random()%8;

		unsigned t = 1+random()%traps_count;
		if(!layout.upgrades.lightning && t>=4)
			++t;
		char trap = traps[t];

		if(op==0)  // replace
			layout.data[base+random()%slots] = trap;
		else if(op==1 || op==2 || op==5)  // swap, rotate, insert
		{
			unsigned pos = base+random()%slots;
			unsigned end = base+random()%(slots-1);
			while(end==pos)
				++end;

			if(op==1)
				swap(layout.data[pos], layout.data[end]);
			else
			{
				if(op==2)
					trap = layout.data[end];

				for(unsigned j=end; j>pos; --j)
					layout.data[j] = layout.data[j-1];
				for(unsigned j=end; j<pos; ++j)
					layout.data[j] = layout.data[j+1];
				layout.data[pos] = trap;
			}
		}
		else  // floor operations
		{
			unsigned floors = slots/5;
			unsigned pos = random()%floors;
			unsigned end = random()%(floors-1);
			while(end==pos)
				++end;

			pos = base+pos*5;
			end = base+end*5;

			if(op==3 || op==6)  // rotate, duplicate
			{
				char floor[5];
				for(unsigned j=0; j<5; ++j)
					floor[j] = layout.data[end+j];

				for(unsigned j=end; j>pos; --j)
					layout.data[j+4] = layout.data[j-1];
				for(unsigned j=end; j<pos; ++j)
					layout.data[j] = layout.data[j+5];

				if(op==3)
				{
					for(unsigned j=0; j<5; ++j)
						layout.data[pos+j] = floor[j];
				}
			}
			else if(op==7)  // copy
			{
				for(unsigned j=0; j<5; ++j)
					layout.data[end+j] = layout.data[pos+j];
			}
			else if(op==4)  // swap
			{
				for(unsigned j=0; j<5; ++j)
					swap(layout.data[pos+j], layout.data[end+j]);
			}
		}
	}
}

bool Spire::is_valid(const std::string &data) const
{
	unsigned slots = data.size();
	bool have_strength = false;
	for(unsigned i=0; i<slots; ++i)
	{
		if(i%5==0)
			have_strength = false;
		if(data[i]=='S')
		{
			if(have_strength)
				return false;
			have_strength = true;
		}
	}

	return true;
}

void Spire::report(const Layout &layout, const string &message)
{
	cout << message << " (" << layout.damage << " damage, " << layout.cost << " Rs, cycle " << layout.cycle << "):" << endl;
	unsigned count = 1;
	print(layout, count);
}

bool Spire::print(const Layout &layout, unsigned &count)
{
	unsigned slots = layout.data.size();
	string descr;
	unsigned upgrades_pos = 0;
	if(numeric_format)
	{
		descr.reserve(slots+8);
		for(unsigned i=0; i<slots; ++i)
			for(unsigned j=0; j<sizeof(traps); ++j)
				if(layout.data[i]==traps[j])
					descr += '0'+j;
		descr += "+0000+";
		descr += stringify(slots/5);
		upgrades_pos = slots+1;
	}
	else
	{
		descr.reserve(5+slots+slots/5-1);
		descr += "0000";
		for(unsigned i=0; i<slots; i+=5)
		{
			descr += ' ';
			descr.append(layout.data.substr(i, 5));
		}
	}

	descr[upgrades_pos] = '0'+layout.upgrades.fire;
	descr[upgrades_pos+1] = '0'+layout.upgrades.frost;
	descr[upgrades_pos+2] = '0'+layout.upgrades.poison;
	descr[upgrades_pos+3] = '0'+layout.upgrades.lightning;

	if(show_pools)
		cout << "\033[K" << descr << ' ' << layout.damage << ' ' << layout.cost << ' ' << layout.cycle << endl;
	else
		cout << descr << endl;

	return (count && --count);
}

void Spire::sighandler(int)
{
	instance->intr_flag = true;
}


Layout::Layout():
	upgrades({ 1, 1, 1, 1 }),
	damage(0),
	cost(0),
	cycle(0)
{ }


Pool::Pool(unsigned s):
	max_size(s)
{ }

void Pool::add_layout(const Layout &layout)
{
	lock_guard<std::mutex> lock(mutex);

	if(layouts.size()>=max_size && layout.damage<layouts.back().damage)
		return;

	auto i = layouts.begin();
	for(; (i!=layouts.end() && i->damage>layout.damage); ++i)
		if(i->cost<=layout.cost)
			return;
	if(i!=layouts.end() && i->damage==layout.damage)
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
	lock_guard<std::mutex> lock(mutex);
	return layouts.front();
}

Layout Pool::get_random_layout(Random &random) const
{
	lock_guard<std::mutex> lock(mutex);

	unsigned total = 0;
	for(const auto &l: layouts)
		total += l.damage;

	if(!total)
	{
		auto i = layouts.begin();
		advance(i, random()%layouts.size());
		return *i;
	}

	unsigned p = random()%total;
	for(const auto &l: layouts)
	{
		if(p<l.damage)
			return l;
		p -= l.damage;
	}

	throw logic_error("Spire::get_random_layout");
}

uint64_t Pool::get_highest_damage() const
{
	lock_guard<std::mutex> lock(mutex);
	return layouts.front().damage;
}

uint64_t Pool::get_lowest_damage() const
{
	lock_guard<std::mutex> lock(mutex);
	return layouts.back().damage;
}

template<typename F>
void Pool::visit_layouts(const F &func) const
{
	lock_guard<std::mutex> lock(mutex);
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
	unique_lock<std::mutex> pools_lock(spire.pools_mutex, defer_lock);
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
					unsigned slots = base_layout.data.size();
					base_layout.data = cross_layout.data;
					base_layout.data.resize(slots, '_');
				}
			}
		}
		else
			pools_lock.unlock();

		uint64_t lowest_damage = pool.get_lowest_damage();
		for(unsigned i=0; i<spire.loops_per_cycle; ++i)
		{
			Layout mutated = base_layout;
			mutated.cycle = cycle;
			if(do_cross)
				spire.cross(mutated.data, cross_layout.data, random);

			unsigned slots = mutated.data.size();
			unsigned mut_count = 1+random()%slots;
			mut_count = max((mut_count*mut_count)/slots, 1U);
			spire.mutate(mutated, random()%3, mut_count, random);
			if(!spire.is_valid(mutated.data))
				continue;
			
			mutated.cost = spire.calculate_cost(mutated.data);
			if(mutated.cost>spire.budget)
				continue;

			mutated.damage = spire.simulate(mutated);
			if(mutated.damage>=lowest_damage)
				pool.add_layout(mutated);
		}
	}
}
