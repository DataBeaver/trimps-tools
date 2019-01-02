#include "spiredb.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#include <pqxx/result>
#include <pqxx/transaction>
#include "getopt.h"
#include "spirelayout.h"
#include "stringutils.h"

using namespace std;
using namespace std::placeholders;

int main(int argc, char **argv)
{
	SpireDB spiredb(argc, argv);
	return spiredb.main();
}

const unsigned SpireDB::current_version = 5;

SpireDB::SpireDB(int argc, char **argv):
	pq_conn(0),
	force_update(false)
{
	string dbopts;

	GetOpt getopt;
	getopt.add_option("dbopts", dbopts, GetOpt::REQUIRED_ARG);
	getopt.add_option("force-update", force_update, GetOpt::NO_ARG);
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
	pq_conn->prepare("select_all", "SELECT id, fire, frost, poison, lightning, traps FROM layouts");
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
	pqxx::result result;
	if(force_update)
		result = xact.exec_prepared("select_all");
	else
		result = xact.exec_prepared("select_old", current_version);
	if(!result.empty())
		cout << "Updating " << result.size() << " layouts" << endl;
	for(const auto &row: result)
	{
		unsigned id = row[0].as<unsigned>();
		TrapUpgrades upgrades;
		upgrades.fire = row[1].as<uint16_t>();
		upgrades.frost = row[2].as<uint16_t>();
		upgrades.poison = row[3].as<uint16_t>();
		upgrades.lightning = row[4].as<uint16_t>();
		Layout layout;
		layout.set_upgrades(upgrades);
		layout.set_traps(row[5].as<string>());
		layout.update(Layout::FULL, 40);
		xact.exec_prepared("update_values", id, layout.get_damage(), layout.get_threat(), layout.get_runestones_per_second(), layout.get_cost(), current_version);
	}
	xact.commit();
}

void SpireDB::serve(Network::ConnectionTag tag, const string &data)
{
	if(data.empty())
		return;

	vector<string> parts = split(data);
	string cmd;
	if(!parts.empty())
	{
		cmd = parts[0];
		parts.erase(parts.begin());
	}

	const string &remote = network.get_remote_host(tag);
	cout << '[' << remote << "] -> " << cmd;
	for(const auto &p: parts)
		cout << ' ' << p;
	cout << endl;

	string result;
	try
	{
		if(cmd=="submit")
			result = submit(parts, remote);
		else if(cmd=="query")
			result = query(parts);
		else
			throw logic_error("bad command");
	}
	catch(const exception &e)
	{
		string type = typeid(e).name();
		string message = e.what();
		for(char &c: message)
			if(c=='\n')
				c = ' ';
		result = format("error %s %s", type, message);
	}

	cout << '[' << remote << "] <- " << result << endl;
	network.send_message(tag, result);
}

string SpireDB::query(const vector<string> &args)
{
	if(args.size()<3 || args.size()>4)
		throw invalid_argument("SpireDB::query");

	TrapUpgrades upgrades(args[0]);
	unsigned floors = parse_value<unsigned>(args[1]);
	Number budget = parse_value<Number>(args[2]);
	bool income = false;
	if(args.size()>=4)
	{
		if(args[3]=="income")
			income = true;
		else if(args[3]!="damage")
			throw invalid_argument("SpireDB::query");
	}

	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared((income ? "select_best_income" : "select_best_damage"), floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget);
	if(!result.empty())
	{
		pqxx::row row = result.front();
		upgrades.fire = row[0].as<uint16_t>();
		upgrades.frost = row[1].as<uint16_t>();
		upgrades.poison = row[2].as<uint16_t>();
		upgrades.lightning = row[3].as<uint16_t>();
		string traps = row[4].c_str();
		return format("ok %s %s", upgrades.str(), traps);
	}
	else
		return "notfound";
}

string SpireDB::submit(const vector<string> &args, const string &submitter)
{
	if(args.size()!=2)
		throw invalid_argument("SpireDB::submit");

	Layout layout;
	layout.set_upgrades(args[0]);
	layout.set_traps(args[1]);
	if(!layout.is_valid())
		throw invalid_argument("SpireDB::submit");
	layout.update(Layout::FULL, 40);

	unsigned floors = layout.get_traps().size()/5;
	const TrapUpgrades &upgrades = layout.get_upgrades();
	Number damage = layout.get_damage();
	Number rs_per_sec = layout.get_runestones_per_second();
	Number cost = layout.get_cost();

	bool accepted = false;
	bool duplicate_damage = false;
	bool duplicate_income = false;

	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared("select_best_damage", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, cost);
	if(!result.empty())
	{
		pqxx::row row = result.front();
		Number best_damage = row[5].as<Number>(0);
		Number best_cost = row[6].as<Number>(0);
		if(best_damage<damage || (best_damage==damage && best_cost>cost))
			accepted = true;
		else if(best_damage==damage && best_cost==cost)
			duplicate_damage = true;
	}
	else
		accepted = true;

	result = xact.exec_prepared("select_best_income", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, cost);
	if(!result.empty())
	{
		pqxx::row row = result.front();
		Number best_income = row[5].as<Number>(0);
		Number best_cost = row[6].as<Number>(0);
		if(best_income<rs_per_sec || (best_income==rs_per_sec && best_cost>cost))
			accepted = true;
		else if(best_income==rs_per_sec && best_cost==cost)
			duplicate_income = true;
	}
	else
		accepted = true;

	if(accepted)
	{
		unsigned threat = layout.get_threat();
		xact.exec_prepared("delete_worse", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, damage, rs_per_sec, cost);
		xact.exec_prepared("insert_layout", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, layout.get_traps(), damage, threat, rs_per_sec, cost, submitter, current_version);
		xact.commit();

		return "ok accepted";
	}
	else if(duplicate_damage && duplicate_income)
		return "ok duplicate";
	else
		return "ok obsolete";
}
