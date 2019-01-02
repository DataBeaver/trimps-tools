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
	Network network;
	pqxx::connection *pq_conn;
	bool force_update;

	static const unsigned current_version;

public:
	SpireDB(int, char **);
	~SpireDB();

	int main();
private:
	void update_layouts();
	void serve(Network::ConnectionTag, const std::string &);
	std::string query(const std::vector<std::string> &);
	std::string submit(const std::vector<std::string> &, const std::string &);
};

#endif
