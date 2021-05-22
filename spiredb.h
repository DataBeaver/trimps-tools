#ifndef SPIREDB_H_
#define SPIREDB_H_

#include <deque>
#include <string>
#define PQXX_HIDE_EXP_OPTIONAL
#include <pqxx/connection>
#include <pqxx/transaction>
#include "network.h"
#include "spirecore.h"
#include "spirelayout.h"
#include "types.h"

struct HttpMessage;

class SpireDB
{
private:
	struct LiveQuery
	{
		std::string upgrades;
		unsigned floors;
		Number budget;
		Core core;
		std::string core_type;
		Number core_budget;
	};

	struct RecentQuery: LiveQuery
	{
		bool income;
		bool towers;
		std::string client;
		std::chrono::steady_clock::time_point time;
		unsigned work_given_count;
	};

	enum WorkType
	{
		INCOMPLETE,
		UNDERPERFORMING,
		ADD_FLOOR,
		INCREASE_BUDGET,
		DECREASE_BUDGET
	};

	Network network;
	std::mutex database_mutex;
	pqxx::connection *pq_conn;
	bool force_update;
	std::map<Network::ConnectionTag, LiveQuery> live_queries;
	std::mutex recent_mutex;
	std::deque<RecentQuery> recent_queries;
	Random random;
	std::mutex work_mutex;
	std::string current_work;
	unsigned work_given_count;

	static const unsigned current_version;

public:
	SpireDB(int, char **);
	~SpireDB();

	int main();
private:
	void update_data();
	void serve(Network::ConnectionTag, const std::string &);
	void serve_http(Network::ConnectionTag, const std::string &);
	void serve_http_file(const std::string &, HttpMessage &);
	std::string query(Network::ConnectionTag, const std::vector<std::string> &, const std::string &, bool = false);
	Layout query_layout(pqxx::transaction_base &, unsigned, const TrapUpgrades &, Number, const Core *, Number, bool ,bool);
	Core query_core(pqxx::transaction_base &, unsigned);
	std::string submit(Network::ConnectionTag, const std::vector<std::string> &, const std::string &);
	int check_better_layout(pqxx::transaction_base &, const Layout &, bool, bool);
	void check_live_queries(Network::ConnectionTag, const Layout &);
	static int compare_layouts(const Layout &, const Layout &, bool, bool);
	void select_random_work();
	std::string get_work();
};

#endif
