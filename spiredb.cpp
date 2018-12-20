#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#define PQXX_HIDE_EXP_OPTIONAL
#include <pqxx/connection>
#include <pqxx/transaction>
#include "getopt.h"
#include "network.h"
#include "spirelayout.h"
#include "stringutils.h"

class SpireDB
{
private:
	enum SubmitResult
	{
		ACCEPTED,
		DUPLICATE,
		OBSOLETE
	};

	Network network;
	pqxx::connection *pq_conn;

public:
	SpireDB(int, char **);
	~SpireDB();

	int main();
private:
	void serve(Network::ConnectionTag, const std::string &);
	Layout query_layout(const std::string &, unsigned, std::uint64_t);
	SubmitResult submit_layout(const std::string &, const std::string &, const std::string &);
};

using namespace std;
using namespace std::placeholders;

int main(int argc, char **argv)
{
	SpireDB spiredb(argc, argv);
	return spiredb.main();
}

SpireDB::SpireDB(int argc, char **argv)
{
	string dbopts;

	GetOpt getopt;
	getopt.add_option("dbopts", dbopts, GetOpt::REQUIRED_ARG);
	getopt(argc, argv);

	if(dbopts.size()>=2 && dbopts[0]=='<')
	{
		ifstream in(dbopts.substr(1));
		if(!in)
			throw runtime_error("Could not open database options file");

		char buf[1024];
		in.read(buf, sizeof(buf));
		dbopts.assign(buf, in.gcount());
	}

	pq_conn = new pqxx::connection(dbopts);
	pq_conn->prepare("insert_layout", "INSERT INTO layouts (floors, fire, frost, poison, lightning, traps, damage, cost, submitter) values ($1, $2, $3, $4, $5, $6, $7, $8, $9)");
	pq_conn->prepare("select_best", "SELECT fire, frost, poison, lightning, traps, damage, cost FROM layouts WHERE floors<=$1 AND fire<=$2 AND frost<=$3 AND poison<=$4 AND lightning<=$5 AND cost<=$6 ORDER BY damage DESC LIMIT 1");
	pq_conn->prepare("delete_worse", "DELETE FROM layouts WHERE floors>=$1 AND fire>=$2 AND frost>=$3 AND poison>=$4 AND lightning>=$5 AND damage<$6 AND cost>=$7");

	network.serve(8676, bind(&SpireDB::serve, this, _1, _2));
}

SpireDB::~SpireDB()
{
	delete pq_conn;
}

int SpireDB::main()
{
	while(1)
		this_thread::sleep_for(chrono::milliseconds(100));
}

void SpireDB::serve(Network::ConnectionTag tag, const string &data)
{
	vector<string> parts = split(data);
	if(parts[0]=="submit")
	{
		if(parts.size()==3)
		{
			try
			{
				SubmitResult result = submit_layout(parts[1], parts[2], network.get_remote_host(tag));
				const char *result_str = 0;
				switch(result)
				{
				case ACCEPTED: result_str = "accepted"; break;
				case DUPLICATE: result_str = "duplicate"; break;
				case OBSOLETE: result_str = "obsolete"; break;
				}
				network.send_message(tag, format("ok %s", result_str));
			}
			catch(const exception &e)
			{
				network.send_message(tag, format("error %s", e.what()));
			}
		}
		else
			network.send_message(tag, "error bad args");
	}
	else if(parts[0]=="query")
	{
		if(parts.size()==4)
		{
			Layout layout = query_layout(parts[1], parse_string<unsigned>(parts[2]), parse_string<uint64_t>(parts[3]));
			if(layout.data.empty())
				network.send_message(tag, "notfound");
			else
				network.send_message(tag, format("ok %s %s", layout.upgrades.str(), layout.data));
		}
		else
			network.send_message(tag, "error bad args");
	}
	else
		network.send_message(tag, "error bad command");
}

Layout SpireDB::query_layout(const string &up_str, unsigned floors, uint64_t budget)
{
	TrapUpgrades upgrades(up_str);
	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared("select_best", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget);
	Layout layout;
	if(!result.empty())
	{
		pqxx::row row = result.front();
		layout.upgrades.fire = row[0].as<uint16_t>();
		layout.upgrades.frost = row[1].as<uint16_t>();
		layout.upgrades.poison = row[2].as<uint16_t>();
		layout.upgrades.lightning = row[3].as<uint16_t>();
		layout.data = row[4].as<string>();
	}

	cout << "query " << up_str << ' ' << floors << ' ' << budget << ' ' << layout.upgrades.str() << ' ' << layout.data << endl;

	return layout;
}

SpireDB::SubmitResult SpireDB::submit_layout(const string &up_str, const string &data, const string &submitter)
{
	Layout layout;
	layout.upgrades = up_str;
	layout.data = data;
	if(!layout.is_valid())
		throw invalid_argument("SpireDB::submit_layout");
	layout.update_damage();
	layout.update_cost();
	cout << "submit " << layout.upgrades.str() << ' ' << layout.data << ' ' << layout.damage << ' ' << layout.cost << ' ' << submitter << endl;

	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared("select_best", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.cost);
	if(!result.empty())
	{
		pqxx::row row = result.front();
		uint64_t best_damage = row[5].as<uint64_t>(0);
		uint64_t best_cost = row[6].as<uint64_t>(0);
		if(best_damage>layout.damage || (best_damage==layout.damage && best_cost<layout.cost))
			return OBSOLETE;
		else if(best_damage==layout.damage && best_cost==layout.cost)
			return DUPLICATE;
	}

	xact.exec_prepared("delete_worse", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.damage, layout.cost);
	xact.exec_prepared("insert_layout", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.data, layout.damage, layout.cost, submitter);
	xact.commit();

	return ACCEPTED;
}
