#ifndef SPIREDB_H_
#define SPIREDB_H_

#include <string>
#define PQXX_HIDE_EXP_OPTIONAL
#include <pqxx/connection>
#include <pqxx/transaction>
#include "network.h"
#include "types.h"

class Core;
class Layout;
class TrapUpgrades;

class SpireDB
{
private:
	struct LiveQuery
	{
		std::string upgrades;
		unsigned floors;
		Number budget;
		std::string core_type;
	};

	Network network;
	pqxx::connection *pq_conn;
	bool force_update;
	std::map<Network::ConnectionTag, LiveQuery> live_queries;

	static const unsigned current_version;

public:
	SpireDB(int, char **);
	~SpireDB();

	int main();
private:
	void update_layouts();
	void serve(Network::ConnectionTag, const std::string &);
	std::string query(Network::ConnectionTag, const std::vector<std::string> &);
	Layout query_layout(pqxx::transaction_base &, unsigned, const TrapUpgrades &, Number, const Core *, bool);
	Core query_core(pqxx::transaction_base &, unsigned);
	std::string submit(Network::ConnectionTag, const std::vector<std::string> &, const std::string &);
	int check_better_layout(pqxx::transaction_base &, const Layout &, bool);
	void check_live_queries(Network::ConnectionTag, const Layout &);
	static int compare_layouts(const Layout &, const Layout &, bool);
	static void calculate_core_mod_deltas(const Layout &, unsigned, Number &, Number &);
};

#endif
