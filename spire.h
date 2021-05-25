#ifndef SPIRE_H_
#define SPIRE_H_

#include <atomic>
#include <condition_variable>
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
		enum State
		{
			WORKING,
			PAUSE_PENDING,
			PAUSED,
			INTERRUPT
		};

		Spire &spire;
		Random random;
		volatile State state;
		std::mutex state_mutex;
		std::condition_variable state_cond;
		std::thread thread;

	public:
		Worker(Spire &, unsigned, bool);

		void interrupt();
		void set_paused(bool);
		void wait_paused();
		void join();

	private:
		void main();
	};

	struct ParsedLayout
	{
		unsigned floors;
		std::string upgrades;
		std::string core;
		std::string traps;
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
	unsigned extinction_interval;
	unsigned next_extinction;
	unsigned isolation_period;
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
	bool numeric_format;
	bool raw_values;
	bool fancy_output;
	bool show_pools;
	Network *network;
	bool live;
	bool athome;
	Network::ConnectionTag connection;
	std::chrono::steady_clock::time_point reconnect_timeout;
	std::chrono::steady_clock::time_point next_work_timeout;
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
	static void parse_numeric_layout(const std::string &, ParsedLayout &);
	static void parse_alpha_layout(const std::string &, ParsedLayout &);
	static ParsedLayout parse_layout(const std::string &, const std::string &, const std::string &, unsigned);
	void init_start_layout(const ParsedLayout &);
	void init_pools(unsigned);
	void init_network(bool);
public:
	~Spire();

	int main();
private:
	bool query_network();
	void process_network_reply(const std::vector<std::string> &, Layout &);
	bool check_better_core(const Layout &, const Core &);
	bool validate_core(const Core &);
	void check_reconnect(const std::chrono::steady_clock::time_point &);
	void check_athome_work(const std::chrono::steady_clock::time_point &);
	bool check_results();
	void submit_best();
	void update_output(bool);
	unsigned get_next_cycle();
	void prune_pools();
	void extinct_pools(Number);
	void receive(Network::ConnectionTag, const std::string &);
	void pause_workers();
	void resume_workers();
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
