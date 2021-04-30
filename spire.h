#ifndef SPIRE_H_
#define SPIRE_H_

#include <atomic>
#include <list>
#include "mingw.mutex.h"
#include "mingw.thread.h"
#include <vector>
#include "console.h"
#include "spirelayout.h"
#include "spirepool.h"
#include "types.h"

struct ParsedLayoutValues
{
	unsigned floors;
	std::string upgrades;
	std::string core;
	std::string traps;
};

std::ostream& operator<<(std::ostream& os, const ParsedLayoutValues& p);

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
	unsigned core_rate;
	bool heterogeneous;
	unsigned n_workers;
	std::list<Worker *> workers;
	unsigned loops_per_cycle;
	std::atomic<unsigned> cycle;
	unsigned loops_per_second;
	bool debug_layout;
	Layout::UpdateMode update_mode;
	Layout::UpdateMode report_update_mode;
	bool numeric_format;
	bool raw_values;
	bool fancy_output;
	bool show_pools;
	bool live;
	bool intr_flag;

	Number budget;
	Number core_budget;
	Core::MutateMode core_mutate;
	bool no_core_downgrade;
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
	ParsedLayoutValues parse_layout(const std::string &, const std::string &, const std::string &, unsigned);
	void init_start_layout(const ParsedLayoutValues &);
	void init_pools(unsigned);
public:
	~Spire();

	int main();
private:
	bool check_results();
	void submit_best();
	void update_output(bool);
	unsigned get_next_cycle();
	void prune_pools();
	void report(const Layout &, const std::string &);
	bool print(const Layout &, unsigned &);
	void print_fancy(const Layout &);
	PrintNum print_num(Number) const;
	static Number damage_score(const Layout &);
	static Number income_score(const Layout &);
	template<Pool::ScoreFunc *, uint32_t>
	static Number towers_score(const Layout &);
	template<Pool::ScoreFunc *>
	static Pool::ScoreFunc *get_towers_score_func(char, bool);
	static void sighandler(int);

	friend std::ostream &operator<<(std::ostream &, const PrintNum &);
};

#endif
