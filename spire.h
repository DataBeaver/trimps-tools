#ifndef SPIRE_H_
#define SPIRE_H_

#include <atomic>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include "console.h"
#include "network.h"
#include "spirelayout.h"
#include "spirepool.h"
#include "types.h"

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

	struct PrintNum
	{
		Number num;
		bool raw;

		PrintNum(Number n, bool r): num(n), raw(r) { }
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
	unsigned loops_per_second;
	unsigned accuracy;
	bool debug_layout;
	bool numeric_format;
	bool raw_values;
	bool fancy_output;
	bool show_pools;
	Network *network;
	bool live;
	Network::ConnectionTag connection;
	bool intr_flag;

	Number budget;
	bool income;
	bool towers;
	Pool::ScoreFunc *score_func;
	Layout start_layout;
	Layout best_layout;
	std::mutex best_mutex;

	Console console;

	static Spire *instance;

public:
	Spire(int, char **);
private:
	void init_start_layout(const std::string &, const std::string &, unsigned);
	void init_pools(unsigned);
	void init_network();
public:
	~Spire();

	int main();
private:
	bool query_network();
	bool check_results();
	void update_output(bool);
	unsigned get_next_cycle();
	void prune_pools();
	void receive(Network::ConnectionTag, const std::string &);
	void report(const Layout &, const std::string &);
	bool print(const Layout &, unsigned &);
	void print_fancy(const Layout &);
	PrintNum print_num(Number) const;
	static Number damage_score(const Layout &);
	static Number damage_towers_score(const Layout &);
	static Number income_score(const Layout &);
	static Number income_towers_score(const Layout &);
	static unsigned get_towers_multiplier(const Layout &);
	static void sighandler(int);

	friend std::ostream &operator<<(std::ostream &, const PrintNum &);
};

#endif
