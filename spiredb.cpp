#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#define PQXX_HIDE_EXP_OPTIONAL
#include <pqxx/connection>
#include <pqxx/result>
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

	static const unsigned current_version;

public:
	SpireDB(int, char **);
	~SpireDB();

	int main();
private:
	void update_layouts();
	void serve(Network::ConnectionTag, const std::string &);
	Layout query_layout(const std::string &, unsigned, Number, bool);
	SubmitResult submit_layout(const std::string &, const std::string &, const std::string &);
};

using namespace std;
using namespace std::placeholders;

int main(int argc, char **argv)
{
	SpireDB spiredb(argc, argv);
	return spiredb.main();
}

const unsigned SpireDB::current_version = 3;

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
	pq_conn->prepare("select_old", "SELECT id, fire, frost, poison, lightning, traps FROM layouts WHERE version<$1");
	pq_conn->prepare("update_values", "UPDATE layouts SET damage=$2, threat=$3, rs_per_sec=$4, cost=$5, version=$6 WHERE id=$1");
	pq_conn->prepare("insert_layout", "INSERT INTO layouts (floors, fire, frost, poison, lightning, traps, damage, threat, rs_per_sec, cost, submitter, version) values ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)");
	pq_conn->prepare("select_best_damage", "SELECT fire, frost, poison, lightning, traps, damage, cost FROM layouts WHERE floors<=$1 AND fire<=$2 AND frost<=$3 AND poison<=$4 AND lightning<=$5 AND cost<=$6 ORDER BY damage DESC LIMIT 1");
	pq_conn->prepare("select_best_income", "SELECT fire, frost, poison, lightning, traps, rs_per_sec, cost FROM layouts WHERE floors<=$1 AND fire<=$2 AND frost<=$3 AND poison<=$4 AND lightning<=$5 AND cost<=$6 ORDER BY rs_per_sec DESC LIMIT 1");
	pq_conn->prepare("delete_worse", "DELETE FROM layouts WHERE floors>=$1 AND fire>=$2 AND frost>=$3 AND poison>=$4 AND lightning>=$5 AND damage<$6 AND rs_per_sec<$7 AND cost>=$8");
}

SpireDB::~SpireDB()
{
	delete pq_conn;
}

int SpireDB::main()
{
	update_layouts();

	bool waiting = false;
	while(1)
	{
		try
		{
			network.serve(8676, bind(&SpireDB::serve, this, _1, _2));
			break;
		}
		catch(const exception &)
		{
			if(!waiting)
				cout << "Waiting for port to become available" << endl;
			waiting = true;
			this_thread::sleep_for(chrono::seconds(1));
		}
	}

	cout << "Begin serving requests" << endl;
	while(1)
		this_thread::sleep_for(chrono::milliseconds(100));
}

void SpireDB::update_layouts()
{
	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared("select_old", current_version);
	if(!result.empty())
		cout << "Updating " << result.size() << " layouts" << endl;
	for(const auto &row: result)
	{
		unsigned id = row[0].as<unsigned>();
		Layout layout;
		layout.upgrades.fire = row[1].as<uint16_t>();
		layout.upgrades.frost = row[2].as<uint16_t>();
		layout.upgrades.poison = row[3].as<uint16_t>();
		layout.upgrades.lightning = row[4].as<uint16_t>();
		layout.data = row[5].as<string>();
		layout.update(Layout::FULL);
		xact.exec_prepared("update_values", id, layout.damage, layout.threat, layout.rs_per_sec, layout.cost, current_version);
	}
	xact.commit();
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
				cout << e.what() << endl;
				string message = e.what();
				for(char &c: message)
					if(c=='\n')
						c = ' ';
				network.send_message(tag, format("error %s", message));
			}
		}
		else
			network.send_message(tag, "error bad args");
	}
	else if(parts[0]=="query")
	{
		if(parts.size()==4 || parts.size()==5)
		{
			if(parts.size()>=5 && parts[4]!="damage" && parts[4]!="income")
				network.send_message(tag, "error bad args");
			else
			{
				bool income = (parts.size()>=5 && parts[4]=="income");
				Layout layout = query_layout(parts[1], parse_value<unsigned>(parts[2]), parse_value<Number>(parts[3]), income);
				if(layout.data.empty())
					network.send_message(tag, "notfound");
				else
					network.send_message(tag, format("ok %s %s", layout.upgrades.str(), layout.data));
			}
		}
		else
			network.send_message(tag, "error bad args");
	}
	else
		network.send_message(tag, "error bad command");
}

Layout SpireDB::query_layout(const string &up_str, unsigned floors, Number budget, bool income)
{
	TrapUpgrades upgrades(up_str);
	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared((income ? "select_best_income" : "select_best_damage"), floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget);
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
	layout.update(Layout::FULL);
	cout << "submit " << layout.upgrades.str() << ' ' << layout.data << ' ' << layout.damage << ' ' << layout.cost << ' ' << submitter << endl;

	bool accepted = false;
	bool duplicate_damage = false;
	bool duplicate_income = false;

	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared("select_best_damage", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.cost);
	if(!result.empty())
	{
		pqxx::row row = result.front();
		Number best_damage = row[5].as<Number>(0);
		Number best_cost = row[6].as<Number>(0);
		if(best_damage<layout.damage || (best_damage==layout.damage && best_cost>layout.cost))
			accepted = true;
		else if(best_damage==layout.damage && best_cost==layout.cost)
			duplicate_damage = true;
	}
	else
		accepted = true;

	result = xact.exec_prepared("select_best_income", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.cost);
	if(!result.empty())
	{
		pqxx::row row = result.front();
		Number best_income = row[5].as<Number>(0);
		Number best_cost = row[6].as<Number>(0);
		if(best_income<layout.rs_per_sec || (best_income==layout.rs_per_sec && best_cost>layout.cost))
			accepted = true;
		else if(best_income==layout.rs_per_sec && best_cost==layout.cost)
			duplicate_income = true;
	}
	else
		accepted = true;

	if(accepted)
	{
		xact.exec_prepared("delete_worse", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.damage, layout.rs_per_sec, layout.cost);
		xact.exec_prepared("insert_layout", layout.data.size()/5, layout.upgrades.fire, layout.upgrades.frost, layout.upgrades.poison, layout.upgrades.lightning, layout.data, layout.damage, layout.threat, layout.rs_per_sec, layout.cost, submitter, current_version);
		xact.commit();

		return ACCEPTED;
	}
	else if(duplicate_damage && duplicate_income)
		return DUPLICATE;
	else
		return OBSOLETE;
}
