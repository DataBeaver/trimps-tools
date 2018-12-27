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
				if(layout.get_traps().empty())
					network.send_message(tag, "notfound");
				else
					network.send_message(tag, format("ok %s %s", layout.get_upgrades().str(), layout.get_traps()));
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
		upgrades.fire = row[0].as<uint16_t>();
		upgrades.frost = row[1].as<uint16_t>();
		upgrades.poison = row[2].as<uint16_t>();
		upgrades.lightning = row[3].as<uint16_t>();
		layout.set_upgrades(upgrades);
		layout.set_traps(row[4].as<string>());
	}

	cout << "query " << up_str << ' ' << floors << ' ' << budget << ' ' << upgrades.str() << ' ' << layout.get_traps() << endl;

	return layout;
}

SpireDB::SubmitResult SpireDB::submit_layout(const string &up_str, const string &data, const string &submitter)
{
	Layout layout;
	layout.set_upgrades(up_str);
	layout.set_traps(data);
	if(!layout.is_valid())
		throw invalid_argument("SpireDB::submit_layout");
	layout.update(Layout::FULL, 40);

	unsigned floors = layout.get_traps().size()/5;
	const TrapUpgrades &upgrades = layout.get_upgrades();
	Number damage = layout.get_damage();
	Number rs_per_sec = layout.get_runestones_per_second();
	Number cost = layout.get_cost();

	cout << "submit " << upgrades.str() << ' ' << layout.get_traps() << ' ' << damage << ' ' << cost << ' ' << submitter << endl;

	bool accepted = false;
	bool duplicate_damage = false;
	bool duplicate_income = false;

	pqxx::work xact(*pq_conn);
	pqxx::result result = xact.exec_prepared("select_best_damage", data.size()/5, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, cost);
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

	result = xact.exec_prepared("select_best_income", data.size()/5, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, cost);
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

		return ACCEPTED;
	}
	else if(duplicate_damage && duplicate_income)
		return DUPLICATE;
	else
		return OBSOLETE;
}
