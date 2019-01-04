#ifndef SPIREDB_H_
#define SPIREDB_H_

#include <string>
#define PQXX_HIDE_EXP_OPTIONAL
#include <pqxx/connection>
#include "network.h"
#include "types.h"

class Layout;

class SpireDB
{
private:
	struct LiveQuery
	{
		std::string upgrades;
		unsigned floors;
		Number budget;
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
	std::string submit(Network::ConnectionTag, const std::vector<std::string> &, const std::string &);
	void check_live_queries(Network::ConnectionTag, const Layout &);
};

#endif
