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

#ifdef WITH_128BIT
namespace pqxx {

template<>
struct string_traits<Number>
{
	static constexpr const char *name() { return "Number"; }
	static constexpr bool has_null() noexcept { return false; }
	static bool is_null(Number) { return false; }
	static void from_string(const char *str, Number &n) { n = parse_value<Number>(str); }
	static std::string to_string(Number n) { return stringify(n); }
};

} // namespace pqxx
#endif

int main(int argc, char **argv)
{
	SpireDB spiredb(argc, argv);
	return spiredb.main();
}

const unsigned SpireDB::current_version = 9;

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

	string common_fields = "layouts.id, fire, frost, poison, lightning, traps, core_tier";
	string join_core_mods =
		"LEFT JOIN core_mods AS core_fire ON core_fire.layout_id=layouts.id AND core_fire.mod=0 "
		"LEFT JOIN core_mods AS core_poison ON core_poison.layout_id=layouts.id AND core_poison.mod=1 "
		"LEFT JOIN core_mods AS core_lightning ON core_lightning.layout_id=layouts.id AND core_lightning.mod=2 "
		"LEFT JOIN core_mods AS core_strength ON core_strength.layout_id=layouts.id AND core_strength.mod=3 "
		"LEFT JOIN core_mods AS core_condenser ON core_condenser.layout_id=layouts.id AND core_condenser.mod=4";
	string filter_base = "floors<=$1 AND fire<=$2 AND frost<=$3 AND poison<=$4 AND lightning<=$5 AND cost<=$6";
	string filter_core = "core_type=$7 AND core_tier<=$8 AND core_cost<=$9";

	pq_conn = new pqxx::connection(dbopts);
	pq_conn->prepare("select_all", "SELECT "+common_fields+" FROM layouts");
	pq_conn->prepare("select_old", "SELECT "+common_fields+" FROM layouts WHERE version<$1");
	pq_conn->prepare("select_core_tier", "SELECT core_tier FROM layouts WHERE id=$1");
	pq_conn->prepare("select_mods", "SELECT mod, value FROM core_mods WHERE layout_id=$1");
	pq_conn->prepare("update_values", "UPDATE layouts SET damage=$2, threat=$3, rs_per_sec=$4, cost=$5, core_cost=$6, version=$7 WHERE id=$1");
	pq_conn->prepare("insert_layout", "INSERT INTO layouts (floors, fire, frost, poison, lightning, traps, core_tier, core_type, damage, threat, rs_per_sec, cost, core_cost, submitter, version) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15) RETURNING id");
	pq_conn->prepare("insert_mod", "INSERT INTO core_mods (layout_id, mod, value) VALUES ($1, $2, $3)");
	pq_conn->prepare("select_best_damage_no_core", "SELECT "+common_fields+", damage, cost FROM layouts WHERE "+filter_base+" AND core_type IS NULL ORDER BY damage DESC LIMIT 1");
	pq_conn->prepare("select_best_damage_with_core", "SELECT "+common_fields+" FROM layouts "+join_core_mods+" WHERE "+filter_base+" AND "+filter_core+" ORDER BY damage DESC LIMIT 5");
	pq_conn->prepare("select_best_damage_any", "SELECT "+common_fields+", damage, cost FROM layouts WHERE "+filter_base+" ORDER BY damage DESC LIMIT 5");
	pq_conn->prepare("select_best_income_no_core", "SELECT "+common_fields+", rs_per_sec, cost FROM layouts WHERE "+filter_base+" AND core_type IS NULL ORDER BY rs_per_sec DESC LIMIT 1");
	pq_conn->prepare("select_best_income_with_core", "SELECT "+common_fields+" FROM layouts "+join_core_mods+" WHERE "+filter_base+" AND "+filter_core+" ORDER BY rs_per_sec DESC LIMIT 5");
	pq_conn->prepare("select_best_income_any", "SELECT "+common_fields+", rs_per_sec, cost FROM layouts WHERE "+filter_base+" ORDER BY rs_per_sec DESC LIMIT 5");
	pq_conn->prepare("delete_worse", "DELETE FROM layouts WHERE floors>=$1 AND fire>=$2 AND frost>=$3 AND poison>=$4 AND lightning>=$5 AND damage<$6 AND rs_per_sec<$7 AND cost>=$8 AND core_type IS NULL");
	pq_conn->prepare("delete_worse_with_core", "DELETE FROM layouts WHERE floors>=$1 AND fire>=$2 AND frost>=$3 AND poison>=$4 AND lightning>=$5 AND damage<$6 AND rs_per_sec<$7 AND cost>=$8 AND core_type=$9 AND NOT EXISTS (SELECT 1 FROM core_mods WHERE (mod=0 AND value<$10) OR (mod=1 AND value<$11) OR (mod=2 AND value<$12) OR (mod=3 AND value<$13) OR (mod=4 AND value<$14))");
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
		if (!row[6].is_null())
			layout.set_core(query_core(xact, id));
		layout.update(Layout::FULL);
		Core core = layout.get_core();
		core.update();
		Number core_cost = (core.cost>>64 ? 0 : core.cost);
		xact.exec_prepared("update_values", id, layout.get_damage(), layout.get_threat(), layout.get_runestones_per_second(), layout.get_cost(), core_cost, current_version);
	}
	xact.commit();
}

void SpireDB::serve(Network::ConnectionTag tag, const string &data)
{
	if(data.empty())
	{
		live_queries.erase(tag);
		return;
	}

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
			result = submit(tag, parts, remote);
		else if(cmd=="query")
			result = query(tag, parts);
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

string SpireDB::query(Network::ConnectionTag tag, const vector<string> &args)
{
	if(args.size()<3)
		throw invalid_argument("SpireDB::query");

	TrapUpgrades upgrades("8896");
	unsigned floors = 20;
	Number budget = static_cast<Number>(-1);
	Core core;
	Number core_budget = 0;
	bool income = false;
	bool live = false;
	for(unsigned i=0; i<args.size(); ++i)
	{
		if(!args[i].compare(0, 2, "f="))
			floors = parse_value<unsigned>(args[i].substr(2));
		else if(!args[i].compare(0, 4, "upg="))
			upgrades = args[i].substr(4);
		else if(!args[i].compare(0, 3, "rs="))
			budget = parse_value<Number>(args[i].substr(3));
		else if(args[i]=="income")
			income = true;
		else if(args[i]=="damage")
			income = false;
		else if(args[i]=="live")
			live = true;
		else if(!args[i].compare(0, 5, "core="))
			core = args[i].substr(5);
		else if(!args[i].compare(0, 3, "ss="))
			core_budget = parse_value<Number>(args[i].substr(3));
		else
			throw invalid_argument("SpireDB::query");
	}

	if(core.tier>=0)
	{
		core.update();
		core_budget = max(core_budget, core.cost);
	}
	else
		core_budget = 0;

	if(live)
	{
		LiveQuery lq;
		lq.upgrades = upgrades.str();
		lq.floors = floors;
		lq.budget = budget;
		lq.core_type = core.get_type();
		lq.core_tier = core.tier;
		lq.core_budget = core_budget;
		live_queries[tag] = lq;
	}

	pqxx::work xact(*pq_conn);
	Layout best = query_layout(xact, floors, upgrades, budget, (core.tier>=0 ? &core : 0), core_budget, income);

	if(!best.get_traps().empty())
	{
		string result;
		result = format("ok upg=%s t=%s", best.get_upgrades().str(), best.get_traps());
		if(best.get_core().tier>=0)
			result += format(" core=%s", best.get_core().str(true));
		return result;
	}
	else
		return "notfound";
}

Layout SpireDB::query_layout(pqxx::transaction_base &xact, unsigned floors, const TrapUpgrades &upgrades, Number budget, const Core *core, Number core_budget, bool income)
{
	Layout best;
	unsigned best_id = 0;
	auto process = [&best, &best_id, core, income](const pqxx::result &result){
		for(const auto &row: result)
		{
			unsigned id = row[0].as<unsigned>();

			TrapUpgrades res_upg;
			res_upg.fire = row[1].as<uint16_t>();
			res_upg.frost = row[2].as<uint16_t>();
			res_upg.poison = row[3].as<uint16_t>();
			res_upg.lightning = row[4].as<uint16_t>();

			Layout layout;
			layout.set_upgrades(res_upg);
			layout.set_traps(row[5].c_str());

			if(core)
				layout.set_core(*core);

			layout.update(income ? Layout::FULL : Layout::EXACT_DAMAGE);
			if(compare_layouts(layout, best, income)>0)
			{
				best = layout;
				best_id = id;
			}
		}
	};

	const char *query = (income ? "select_best_income_no_core" : "select_best_damage_no_core");
	process(xact.exec_prepared(query, floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget));

	query = (income ? "select_best_income_any" : "select_best_damage_any");
	process(xact.exec_prepared(query, floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget));

	if(core)
	{
		query = (income ? "select_best_income_with_core" : "select_best_damage_with_core");
		process(xact.exec_prepared(query, floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget,
			core->get_type(), core->tier, core_budget));
	}

	best.set_core(query_core(xact, best_id));

	return best;
}

Core SpireDB::query_core(pqxx::transaction_base &xact, unsigned layout_id)
{
	Core core;

	pqxx::result result = xact.exec_prepared("select_core_tier", layout_id);
	core.tier = result.front()[0].as<int16_t>();

	result = xact.exec_prepared("select_mods", layout_id);
	for(const auto &row: result)
		core.set_mod(row[0].as<unsigned>(), row[1].as<uint16_t>());

	return core;
}

string SpireDB::submit(Network::ConnectionTag tag, const vector<string> &args, const string &submitter)
{
	if(args.size()<2)
		throw invalid_argument("SpireDB::submit");

	unsigned kw_start = 0;

	Layout layout;
	if(args[0].find('=')==string::npos)
	{
		layout.set_upgrades(args[0]);
		layout.set_traps(args[1]);
		kw_start = 2;
	}

	for(unsigned i=kw_start; i<args.size(); ++i)
	{
		if(!args[i].compare(0, 4, "upg="))
			layout.set_upgrades(args[i]);
		else if(!args[i].compare(0, 2, "t="))
			layout.set_traps(args[i]);
		else if(!args[i].compare(0, 5, "core="))
		{
			Core core = args[i].substr(5);
			core.update();
			layout.set_core(core);
		}
		else
			throw invalid_argument("SpireDB::submit");
	}
	if(!layout.is_valid())
		throw invalid_argument("SpireDB::submit");
	layout.update(Layout::FULL);

	pqxx::work xact(*pq_conn);
	int verdict = check_better_layout(xact, layout, false);
	verdict = max(verdict, check_better_layout(xact, layout, true));

	if(verdict>0)
	{
		unsigned floors = layout.get_traps().size()/5;
		const TrapUpgrades &upgrades = layout.get_upgrades();
		Number damage = layout.get_damage();
		Number rs_per_sec = layout.get_runestones_per_second();
		Number cost = layout.get_cost();
		unsigned threat = layout.get_threat();

		const Core &core = layout.get_core();
		if(core.tier>=0)
			xact.exec_prepared("delete_worse_with_core", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, damage, rs_per_sec, cost, core.get_type(), core.fire, core.poison, core.lightning, core.strength, core.condenser);
		else
			xact.exec_prepared("delete_worse", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, damage, rs_per_sec, cost);

		if(core.tier>=0)
		{
			pqxx::row row = xact.exec_prepared1("insert_layout", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, layout.get_traps(), core.tier, core.get_type(), damage, threat, rs_per_sec, cost, core.cost, submitter, current_version);
			unsigned layout_id = row[0].as<unsigned>();

			for(unsigned i=0; i<Core::N_MODS; ++i)
			{
				unsigned value = core.get_mod(i);
				if(!value)
					continue;

				xact.exec_prepared("insert_mod", layout_id, i, value);
			}
		}
		else
			xact.exec_prepared("insert_layout", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, layout.get_traps(), -1, nullptr, damage, threat, rs_per_sec, cost, 0, submitter, current_version);
		xact.commit();

		check_live_queries(tag, layout);

		return "ok accepted";
	}
	else if(!verdict)
		return "ok duplicate";
	else
		return "ok obsolete";
}

int SpireDB::check_better_layout(pqxx::transaction_base &xact, const Layout &layout, bool income)
{
	unsigned floors = layout.get_traps().size()/5;
	const TrapUpgrades &upgrades = layout.get_upgrades();
	const Core &core = layout.get_core();
	Number cost = layout.get_cost();

	Layout best = query_layout(xact, floors, upgrades, cost, (core.tier>=0 ? &core : 0), core.cost, income);
	best.update(income ? Layout::FULL : Layout::EXACT_DAMAGE);
	int result = compare_layouts(layout, best, income);
	if(result)
		return result;
	else if(layout.get_cost()>best.get_cost())
		return 1;
	else if(layout.get_cost()<best.get_cost())
		return -1;
	else
		return 0;
}

void SpireDB::check_live_queries(Network::ConnectionTag tag, const Layout &layout)
{
	string up_str = layout.get_upgrades().str();
	unsigned floors = layout.get_traps().size()/5;
	Number cost = layout.get_cost();
	string core_type = layout.get_core().get_type();
	unsigned core_tier = layout.get_core().tier;
	Number core_cost = layout.get_core().cost;
	for(const auto &lq: live_queries)
	{
		if(tag!=lq.first && up_str==lq.second.upgrades && floors==lq.second.floors && cost<=lq.second.budget
			&& core_type==lq.second.core_type && core_tier<=lq.second.core_tier && core_cost<=lq.second.core_budget)
		{
			string push = format("push upg=%s t=%s", up_str, layout.get_traps());
			if(layout.get_core().tier>=0)
				push += format(" core=%s", layout.get_core().str(true));
			cout << '[' << network.get_remote_host(lq.first) << "] <- " << push << endl;
			network.send_message(lq.first, push);
		}
	}
}

int SpireDB::compare_layouts(const Layout &layout1, const Layout &layout2, bool income)
{
	Number score1 = (income ? layout1.get_runestones_per_second() : layout1.get_damage());
	Number score2 = (income ? layout2.get_runestones_per_second() : layout2.get_damage());
	if(score1<score2)
		return -1;
	else if(score1>score2)
		return 1;
	else
		return 0;
}
