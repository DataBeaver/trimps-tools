#include "spiredb.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#include <pqxx/result>
#include <pqxx/transaction>
#include "getopt.h"
#include "http.h"
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

const unsigned SpireDB::current_version = 10;

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

	string layout_fields = "layouts.id, fire, frost, poison, lightning, traps, core_id";
	string core_fields = "cores.id, type, tier";
	string join_core = "LEFT JOIN cores ON cores.id=layouts.core_id";
	string join_core_mods =
		"LEFT JOIN core_mods AS core_fire ON core_fire.core_id=cores.id AND core_fire.mod=0 "
		"LEFT JOIN core_mods AS core_poison ON core_poison.core_id=cores.id AND core_poison.mod=1 "
		"LEFT JOIN core_mods AS core_lightning ON core_lightning.core_id=cores.id AND core_lightning.mod=2 "
		"LEFT JOIN core_mods AS core_strength ON core_strength.core_id=cores.id AND core_strength.mod=3 "
		"LEFT JOIN core_mods AS core_condenser ON core_condenser.core_id=cores.id AND core_condenser.mod=4"
		"LEFT JOIN core_mods AS core_runestone ON core_runestone.core_id=cores.id AND core_condenser.mod=4";
	string filter_base = "floors<=$1 AND fire<=$2 AND frost<=$3 AND poison<=$4 AND lightning<=$5 AND layouts.cost<=$6";
	string filter_core = "cores.type=$7 AND cores.tier<=$8 AND cores.cost<=$9";
	string filter_config = "floors=$1 AND fire=$2 AND frost=$3 AND poison=$4 AND lightning=$5";
	string filter_core_config = "cores.type=$6 AND cores.tier=$7";
	string empty_traps = "LENGTH(REGEXP_REPLACE(traps, '[^_]', '', 'g')) AS empty_count";

	pq_conn = new pqxx::connection(dbopts);
	pq_conn->prepare("select_all_layouts", "SELECT "+layout_fields+" FROM layouts");
	pq_conn->prepare("select_all_cores", "SELECT "+core_fields+" FROM cores");
	pq_conn->prepare("select_old_layouts", "SELECT "+layout_fields+" FROM layouts WHERE version<$1");
	pq_conn->prepare("select_old_cores", "SELECT "+core_fields+" FROM cores WHERE version<$1");

	pq_conn->prepare("find_core", "SELECT cores.id FROM cores "+join_core_mods+" WHERE type=$1 AND tier=$2 "
		"AND coalesce(core_fire.value, 0)=$3 AND coalesce(core_poison.value, 0)=$4 AND coalesce(core_lightning.value, 0)=$5 "
		"AND coalesce(core_strength.value, 0)=$6 AND coalesce(core_condenser.value, 0)=$7 AND coalesce(core_runestone.value, 0)=$8");
	pq_conn->prepare("select_core", "SELECT "+core_fields+" FROM cores WHERE id=$1");
	pq_conn->prepare("select_mods", "SELECT mod, value FROM core_mods WHERE core_id=$1");
	pq_conn->prepare("update_layout", "UPDATE layouts SET damage=$2, threat=$3, rs_per_sec=$4, towers=$5, cost=$6, version=$7 WHERE id=$1");
	pq_conn->prepare("update_core", "UPDATE cores SET cost=$2, version=$3 WHERE id=$1");
	pq_conn->prepare("insert_layout", "INSERT INTO layouts (floors, fire, frost, poison, lightning, traps, core_id, damage, threat, rs_per_sec, towers, cost, submitter, version) "
		"VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14) RETURNING id");
	pq_conn->prepare("insert_core", "INSERT INTO cores (type, tier, cost, version) VALUES ($1, $2, $3, $4) RETURNING id");
	pq_conn->prepare("insert_mod", "INSERT INTO core_mods (core_id, mod, value) VALUES ($1, $2, $3)");

	string select_no_core = "SELECT "+layout_fields+" FROM layouts WHERE "+filter_base+" AND core_id IS NULL";
	string select_with_core = "SELECT "+layout_fields+" FROM layouts "+join_core+" WHERE "+filter_base+" AND "+filter_core;
	string select_any = "SELECT "+layout_fields+" FROM layouts WHERE "+filter_base;
	string best_damage = "ORDER BY damage DESC";
	string best_income = "ORDER BY rs_per_sec DESC";
	string best_towers_damage = "ORDER BY towers DESC, damage DESC";
	string best_towers_income = "ORDER BY towers DESC, rs_per_sec DESC";
	pq_conn->prepare("select_best_damage_no_core", select_no_core+" "+best_damage+" LIMIT 1");
	pq_conn->prepare("select_best_damage_with_core", select_with_core+" "+best_damage+" LIMIT 5");
	pq_conn->prepare("select_best_damage_any", select_any+" "+best_damage+" LIMIT 5");
	pq_conn->prepare("select_best_income_no_core", select_no_core+" "+best_income+" LIMIT 1");
	pq_conn->prepare("select_best_income_with_core", select_with_core+" "+best_income+" LIMIT 5");
	pq_conn->prepare("select_best_income_any", select_any+" "+best_income+" LIMIT 5");
	pq_conn->prepare("select_best_towers_damage_no_core", select_no_core+" "+best_towers_damage+" LIMIT 1");
	pq_conn->prepare("select_best_towers_damage_with_core", select_with_core+" "+best_towers_damage+" LIMIT 5");
	pq_conn->prepare("select_best_towers_damage_any", select_any+" "+best_towers_damage+" LIMIT 5");
	pq_conn->prepare("select_best_towers_income_no_core", select_no_core+" "+best_towers_income+" LIMIT 1");
	pq_conn->prepare("select_best_towers_income_with_core", select_with_core+" "+best_towers_income+" LIMIT 5");
	pq_conn->prepare("select_best_towers_income_any", select_any+" "+best_towers_income+" LIMIT 5");

	pq_conn->prepare("delete_worse_no_core", "DELETE FROM layouts WHERE floors>=$1 AND fire>=$2 AND frost>=$3 AND poison>=$4 AND lightning>=$5 "
		"AND damage<$6 AND rs_per_sec<$7 AND towers<=$8 AND cost>=$9 AND core_id IS NULL");
	pq_conn->prepare("delete_worse_with_core", "DELETE FROM layouts WHERE floors>=$1 AND fire>=$2 AND frost>=$3 AND poison>=$4 AND lightning>=$5 "
		"AND damage<$6 AND rs_per_sec<$7 AND towers<=$8 AND cost>=$9 AND core_id IN (SELECT cores.id FROM cores "+join_core_mods+" WHERE type=$10 AND tier>=$11 "
		"AND coalesce(core_fire.value, 0)>=$12 AND coalesce(core_poison.value, 0)>=$13 AND coalesce(core_lightning.value, 0)>=$14 "
		"AND coalesce(core_strength.value, 0)>=$15 AND coalesce(core_condenser.value, 0)>=$16 AND coalesce(core_runestone.value, 0)>=$17)");

	pq_conn->prepare("select_configs", "SELECT floors, fire, frost, poison, lightning, type, tier FROM layouts "+join_core+" "
		"WHERE floors>=7 AND floors<=20 AND fire>=4 AND frost>=4 AND poison>=3 AND lightning>=1 AND core_id IS NOT NULL "
		"GROUP BY floors, fire, frost, poison, lightning, type, tier");
	pq_conn->prepare("select_incomplete", "SELECT "+layout_fields+", "+empty_traps+" FROM layouts "+join_core+" "
		"WHERE "+filter_config+" AND "+filter_core_config+" ORDER BY empty_count DESC");
	pq_conn->prepare("select_underperforming", "SELECT "+layout_fields+" FROM layouts "+join_core+" "
		"WHERE "+filter_config+" AND "+filter_core_config+" ORDER BY rs_per_sec/layouts.cost ASC");
	pq_conn->prepare("select_missing_floor", "SELECT "+layout_fields+" FROM layouts "+join_core+" "
		"WHERE "+filter_config+" AND "+filter_core_config+" AND floors<20 AND threat/100>LENGTH(traps)/5");
	pq_conn->prepare("select_best_for_config", "SELECT "+layout_fields+" FROM layouts "+join_core+" "
		"WHERE "+filter_config+" AND "+filter_core_config+" ORDER BY rs_per_sec DESC");
	pq_conn->prepare("select_best_towers_for_config", "SELECT "+layout_fields+" FROM layouts "+join_core+" "
		"WHERE "+filter_config+" AND "+filter_core_config+" ORDER BY towers DESC, rs_per_sec DESC");
	pq_conn->prepare("select_random", "SELECT "+layout_fields+" FROM layouts "+join_core+" "
		"WHERE "+filter_config+" AND "+filter_core_config+" ORDER BY RANDOM()");
}

SpireDB::~SpireDB()
{
	delete pq_conn;
}

int SpireDB::main()
{
	update_data();

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

	select_random_work();

	cout << "Begin serving requests" << endl;
	unsigned work_age = 0;
	while(1)
	{
		this_thread::sleep_for(chrono::seconds(5));

		chrono::steady_clock::time_point now = chrono::steady_clock::now();
		{
			lock_guard<mutex> lock(recent_mutex);
			for(auto i=recent_queries.begin(); i!=recent_queries.end(); )
			{
				if(i->time+chrono::hours(1)<now || i->work_given_count>=5)
					i = recent_queries.erase(i);
				else
					++i;
			}
		}

		if(work_given_count>=5 || (work_given_count>0 && work_age>=12))
		{
			select_random_work();
			work_age = 0;
		}
		else
			++work_age;
	}
}

void SpireDB::update_data()
{
	lock_guard<mutex> lock_db(database_mutex);
	pqxx::work xact(*pq_conn);

	pqxx::result result;
	if(force_update)
		result = xact.exec_prepared("select_all_layouts");
	else
		result = xact.exec_prepared("select_old_layouts", current_version);
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
			layout.set_core(query_core(xact, row[6].as<unsigned>()));
		layout.update(Layout::FULL);
		xact.exec_prepared("update_layout", id, layout.get_damage(), layout.get_threat(), layout.get_runestones_per_second(),
			layout.get_tower_count(), layout.get_cost(), current_version);
	}

	if(force_update)
		result = xact.exec_prepared("select_all_cores");
	else
		result = xact.exec_prepared("select_old_cores", current_version);
	if(!result.empty())
		cout << "Updating " << result.size() << " cores" << endl;
	for(const auto &row: result)
	{
		unsigned id = row[0].as<unsigned>();
		Core core;
		core.tier = row[2].as<int16_t>();

		pqxx::result mods_result = xact.exec_prepared("select_mods", id);
		for(const auto &mod: mods_result)
			core.set_mod(mod[0].as<unsigned>(), mod[1].as<uint16_t>());

		core.update();
		Number cost = min<Number>(core.cost, numeric_limits<int64_t>::max());
		xact.exec_prepared("update_core", id, cost, current_version);
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

	if(!data.compare(0, 4, "GET ") || !data.compare(0, 5, "POST "))
		return serve_http(tag, data);

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
			result = query(tag, parts, remote);
		else if(cmd=="getwork")
			result = get_work();
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

void SpireDB::serve_http(Network::ConnectionTag tag, const string &data)
{
	const string &remote = network.get_remote_host(tag);
	HttpMessage request(data);
	cout << '[' << remote << "] -> " << request.method << ' ' << request.path << endl;

	HttpMessage response(404);
	try
	{
		if(request.method=="GET" && (request.path=="/index.html" || request.path=="/"))
			serve_http_file("spiredb.html", response);
		else if(request.method=="GET" && request.path=="/spiredb.css")
			serve_http_file("spiredb.css", response);
		else if(request.method=="GET" && request.path=="/spiredb.js")
			serve_http_file("spiredb.js", response);
		else if(request.method=="GET" && request.path=="/lz-string.js")
			serve_http_file("lz-string.min.js", response);
		else if(request.method=="POST" && request.path=="/query")
		{
			response.response = 200;
			vector<string> parts = split(request.body);
			response.body = query(tag, parts, remote, true);
		}
	}
	catch(const exception &e)
	{
		response.response = 500;
		response.body = format("error %s %s", typeid(e).name(), e.what());
	}

	response.add_header("Content-Length", response.body.size());

	cout << '[' << remote << "] <- HTTP " << response.response << endl;
	network.send_message(tag, response.str());
}

void SpireDB::serve_http_file(const string &filename, HttpMessage &response)
{
	ifstream in_file(filename);
	char buffer[16384];
	while(in_file)
	{
		in_file.read(buffer, sizeof(buffer));
		response.body.append(buffer, in_file.gcount());
	}

	response.response = 200;

	string::size_type fn_dot = filename.rfind('.');
	string content_type;
	if(!filename.compare(fn_dot, string::npos, ".html"))
		content_type = "text/html";
	else if(!filename.compare(fn_dot, string::npos, ".js"))
		content_type = "text/javascript";
	else if(!filename.compare(fn_dot, string::npos, ".css"))
		content_type = "text/css";
	else
		content_type = "text/plain";
	response.add_header("Content-Type", content_type+";charset=utf-8");
}

string SpireDB::query(Network::ConnectionTag tag, const vector<string> &args, const string &client, bool report_stats)
{
	TrapUpgrades upgrades("8896");
	unsigned floors = 20;
	Number budget = static_cast<Number>(-1);
	Core core;
	Number core_budget = 0;
	bool income = false;
	bool towers = false;
	bool live = false;
	for(unsigned i=0; i<args.size(); ++i)
	{
		if(!args[i].compare(0, 2, "f="))
			floors = parse_value<unsigned>(args[i].substr(2));
		else if(!args[i].compare(0, 4, "upg="))
			upgrades = args[i].substr(4);
		else if(!args[i].compare(0, 3, "rs="))
			budget = parse_value<NumberIO>(args[i].substr(3));
		else if(args[i]=="income")
			income = true;
		else if(args[i]=="damage")
			income = false;
		else if(args[i]=="towers")
			towers = true;
		else if(args[i]=="live")
			live = true;
		else if(!args[i].compare(0, 5, "core="))
			core = args[i].substr(5);
		else if(!args[i].compare(0, 3, "ss="))
			core_budget = parse_value<NumberIO>(args[i].substr(3));
		else
			throw invalid_argument("SpireDB::query");
	}

	if(!core.get_n_mods())
	{
		core = Core();
		core_budget = 0;
	}

	RecentQuery rq;
	rq.upgrades = upgrades.str();
	rq.floors = floors;
	rq.budget = budget;
	rq.core = core;
	rq.core_type = core.get_type();
	rq.core_budget = core_budget;
	rq.income = income;
	rq.client = client;
	rq.time = chrono::steady_clock::now();
	rq.work_given_count = 0;
	{
		lock_guard<mutex> lock(recent_mutex);
		auto i = find_if(recent_queries.begin(), recent_queries.end(), [client](const RecentQuery &q){ return q.client==client; });
		if(i!=recent_queries.end())
			recent_queries.erase(i);
		recent_queries.push_back(rq);
	}

	if(live)
		live_queries[tag] = rq;

	if(core.tier>=0)
	{
		core.update();
		core_budget = max(core_budget, core.cost);
	}
	else
		core_budget = 0;

	lock_guard<mutex> lock_db(database_mutex);
	pqxx::work xact(*pq_conn);
	Layout best = query_layout(xact, floors, upgrades, budget, (core.tier>=0 ? &core : 0), core_budget, income, towers);

	if(!best.get_traps().empty())
	{
		string result;
		result = format("ok upg=%s t=%s", best.get_upgrades().str(), best.get_traps());
		if(best.get_core().tier>=0)
			result += format(" core=%s", best.get_core().str(true));
		if(report_stats)
		{
			best.set_upgrades(upgrades);
			best.set_core(core);
			best.update(Layout::FULL);
			result += format(" rs=%s damage=%s income=%s enemy_worth=%s threat=%s",
				best.get_cost(), best.get_damage(), best.get_runestones_per_second(), best.get_runestones_per_enemy(), best.get_threat());
		}
		return result;
	}
	else
		return "notfound";
}

Layout SpireDB::query_layout(pqxx::transaction_base &xact, unsigned floors, const TrapUpgrades &upgrades, Number budget, const Core *core, Number core_budget, bool income, bool towers)
{
	Layout best;
	unsigned best_core_id = 0;
	auto process = [&best, &best_core_id, core, income, towers](const pqxx::result &result){
		for(const auto &row: result)
		{
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
			if(compare_layouts(layout, best, income, towers)>0)
			{
				best = layout;
				best_core_id = (row[6].is_null() ? 0 : row[6].as<unsigned>());
			}
		}
	};

	string query_type = (income ? (towers ? "towers_income" : "income") : (towers ? "towers_damage" : "damage"));

	process(xact.exec_prepared("select_best_"+query_type+"_no_core", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget));

	process(xact.exec_prepared("select_best_"+query_type+"_any", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget));

	if(core)
		process(xact.exec_prepared("select_best_"+query_type+"_with_core", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, budget,
			core->get_type(), core->tier, core_budget));

	if(best_core_id)
		best.set_core(query_core(xact, best_core_id));

	return best;
}

Core SpireDB::query_core(pqxx::transaction_base &xact, unsigned core_id)
{
	Core core;

	pqxx::row row = xact.exec_prepared1("select_core", core_id);

	core.tier = row[2].as<int16_t>();

	pqxx::result result = xact.exec_prepared("select_mods", core_id);
	for(const auto &mod: result)
		core.set_mod(mod[0].as<unsigned>(), mod[1].as<uint16_t>());

	return core;
}

string SpireDB::submit(Network::ConnectionTag tag, const vector<string> &args, const string &submitter)
{
	Layout layout;
	for(unsigned i=0; i<args.size(); ++i)
	{
		if(!args[i].compare(0, 4, "upg="))
			layout.set_upgrades(args[i].substr(4));
		else if(!args[i].compare(0, 2, "t="))
			layout.set_traps(args[i].substr(2));
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

	lock_guard<mutex> lock_db(database_mutex);
	pqxx::work xact(*pq_conn);
	int verdict = check_better_layout(xact, layout, false, false);
	verdict = max(verdict, check_better_layout(xact, layout, true, false));
	verdict = max(verdict, check_better_layout(xact, layout, false, true));
	verdict = max(verdict, check_better_layout(xact, layout, true, true));

	if(verdict>0)
	{
		unsigned floors = layout.get_traps().size()/5;
		const TrapUpgrades &upgrades = layout.get_upgrades();
		Number damage = layout.get_damage();
		Number rs_per_sec = layout.get_runestones_per_second();
		Number cost = layout.get_cost();
		unsigned threat = layout.get_threat();
		unsigned tower_count = layout.get_tower_count();

		const Core &core = layout.get_core();
		if(core.tier>=0)
			xact.exec_prepared("delete_worse_with_core", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, damage, rs_per_sec, tower_count, cost,
				core.get_type(), core.tier, core.fire, core.poison, core.lightning, core.strength, core.condenser, core.runestones);
		else
			xact.exec_prepared("delete_worse_no_core", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, damage, rs_per_sec, tower_count, cost);

		optional<unsigned> core_id;
		if(core.tier>=0)
		{
			pqxx::result result = xact.exec_prepared("find_core", core.get_type(), core.tier, core.fire, core.poison, core.lightning, core.strength, core.condenser, core.runestones);
			if(!result.empty())
				core_id = result.front()[0].as<unsigned>();
			else
			{
				pqxx::row row = xact.exec_prepared1("insert_core", core.get_type(), core.tier, core.cost, current_version);
				core_id = row[0].as<unsigned>();

				for(unsigned i=0; i<Core::N_MODS; ++i)
				{
					unsigned value = core.get_mod(i);
					if(!value)
						continue;

					xact.exec_prepared("insert_mod", core_id, i, value);
				}
			}
		}

		xact.exec_prepared("insert_layout", floors, upgrades.fire, upgrades.frost, upgrades.poison, upgrades.lightning, layout.get_traps(), core_id,
			damage, threat, rs_per_sec, tower_count, cost, submitter, current_version);
		xact.commit();

		check_live_queries(tag, layout);

		return "ok accepted";
	}
	else if(!verdict)
		return "ok duplicate";
	else
		return "ok obsolete";
}

int SpireDB::check_better_layout(pqxx::transaction_base &xact, const Layout &layout, bool income, bool towers)
{
	unsigned floors = layout.get_traps().size()/5;
	const TrapUpgrades &upgrades = layout.get_upgrades();
	const Core &core = layout.get_core();
	Number cost = layout.get_cost();

	Layout best = query_layout(xact, floors, upgrades, cost, (core.tier>=0 ? &core : 0), core.cost, income, towers);
	best.update(income ? Layout::FULL : Layout::EXACT_DAMAGE);
	int result = compare_layouts(layout, best, income, towers);
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
	int16_t core_tier = layout.get_core().tier;
	Number core_cost = layout.get_core().cost;
	for(const auto &lq: live_queries)
	{
		if(tag!=lq.first && up_str==lq.second.upgrades && floors==lq.second.floors && cost<=lq.second.budget
			&& core_type==lq.second.core_type && core_tier<=lq.second.core.tier && core_cost<=lq.second.core_budget)
		{
			string push = format("push upg=%s t=%s", up_str, layout.get_traps());
			if(layout.get_core().tier>=0)
				push += format(" core=%s", layout.get_core().str(true));
			cout << '[' << network.get_remote_host(lq.first) << "] <- " << push << endl;
			network.send_message(lq.first, push);
		}
	}
}

int SpireDB::compare_layouts(const Layout &layout1, const Layout &layout2, bool income, bool towers)
{
	if(towers)
	{
		unsigned towers1 = layout1.get_tower_count();
		unsigned towers2 = layout2.get_tower_count();
		if(towers1<towers2)
			return -1;
		else if(towers1>towers2)
			return 1;
	}

	Number score1 = (income ? layout1.get_runestones_per_second() : layout1.get_damage());
	Number score2 = (income ? layout2.get_runestones_per_second() : layout2.get_damage());
	if(score1<score2)
		return -1;
	else if(score1>score2)
		return 1;
	else
		return 0;
}

void SpireDB::select_random_work()
{
	lock_guard<mutex> lock_db(database_mutex);
	pqxx::work xact(*pq_conn);

	pqxx::result result = xact.exec_prepared("select_configs");
	if(result.empty())
		return;

	unsigned i = random()%result.size();

	const pqxx::row &config_row = result[i];
	unsigned floors = config_row[0].as<unsigned>();
	uint16_t fire = config_row[1].as<uint16_t>();
	uint16_t frost = config_row[2].as<uint16_t>();
	uint16_t poison = config_row[3].as<uint16_t>();
	uint16_t lightning = config_row[4].as<uint16_t>();

	string query_name;
	WorkType work_type = static_cast<WorkType>(random()%6);
	switch(work_type)
	{
	case INCOMPLETE: query_name = "select_incomplete"; break;
	case UNDERPERFORMING: query_name = "select_underperforming"; break;
	case ADD_FLOOR: query_name = "select_missing_floor"; break;
	case INCREASE_BUDGET: query_name = "select_best_for_config"; break;
	case INCREASE_BUDGET_TOWERS: query_name = "select_best_towers_for_config"; break;
	case DECREASE_BUDGET: query_name = "select_random"; break;
	}

	string core_type = config_row[5].c_str();
	int16_t core_tier = config_row[6].as<int16_t>();
	result = xact.exec_prepared(query_name, floors, fire, frost, poison, lightning, core_type, core_tier);
	if(result.empty())
	{
		cout << "Couldn't find any work for " << floors << " floors " << fire << frost << poison << lightning
			<< " with core " << core_tier << '/' << core_type << " using query " << query_name << endl;
		return;
	}

	const pqxx::row &row = result.front();

	TrapUpgrades upg;
	upg.fire = row[1].as<uint16_t>();
	upg.frost = row[2].as<uint16_t>();
	upg.poison = row[3].as<uint16_t>();
	upg.lightning = row[4].as<uint16_t>();

	string traps = row[5].c_str();
	if(work_type==ADD_FLOOR)
		traps += "_____";

	Core core;
	if(!row[6].is_null())
		core = query_core(xact, row[6].as<unsigned>());

	Number budget = 0;
	if(work_type==INCREASE_BUDGET || work_type==DECREASE_BUDGET)
	{
		Layout layout;
		layout.set_upgrades(upg);
		layout.set_traps(traps);
		layout.update(Layout::COST_ONLY);

		uniform_real_distribution<double> distrib;
		if(work_type==INCREASE_BUDGET)
			distrib = uniform_real_distribution<double>(0.0, 2.0);
		else
			distrib = uniform_real_distribution<double>(-0.1, 0.0);
		budget = layout.get_cost()*exp(distrib(random));

		if(work_type==DECREASE_BUDGET)
		{
			for(unsigned j=traps.size()-1; (j<traps.size() && layout.get_cost()>budget); --j)
			{
				traps[j] = '_';
				layout.set_traps(traps);
				layout.update(Layout::COST_ONLY);
			}
		}
	}

	lock_guard<mutex> lock(work_mutex);
	current_work = format("work upg=%s t=%s %s", upg.str(), traps, (work_type==INCOMPLETE ? "damage" : "income"));
	if(work_type==INCREASE_BUDGET_TOWERS)
		current_work += " towers";
	if(budget>0)
		current_work += format(" rs=%s", budget);
	if(core.tier>=0 && core.get_n_mods())
		current_work += format(" core=%s", core.str(true));

	work_given_count = 0;

	cout << "Selected random work: " << upg.str() << ' ' << traps << ' ' << core.str() << endl;
}

string SpireDB::get_work()
{
	{
		lock_guard<mutex> lock(recent_mutex);
		if(!recent_queries.empty())
		{
			auto last_query_age = chrono::steady_clock::now()-recent_queries.back().time;
			if(last_query_age<chrono::minutes(2) || random()%2)
			{
				unsigned i = random()%recent_queries.size();
				RecentQuery &rq = recent_queries[i];
				lock_guard<mutex> lock_db(database_mutex);
				pqxx::work xact(*pq_conn);
				Layout layout = query_layout(xact, rq.floors, rq.upgrades, rq.budget, (rq.core.tier>=0 ? &rq.core : 0), rq.core_budget, rq.income, rq.towers);

				string traps = layout.get_traps();
				traps.resize(rq.floors*5, '_');
				string work = format("work upg=%s t=%s rs=%s %s", rq.upgrades, traps, rq.budget, (rq.income ? "income" : "damage"));
				if(rq.towers)
					work += " towers";
				if(rq.core.tier>=0)
					work += format(" core=%s", rq.core.str(true));
				if(rq.core_budget>0)
					work += format(" ss=%s", rq.core_budget);

				++rq.work_given_count;

				return work;
			}
		}
	}

	lock_guard<mutex> lock(work_mutex);
	++work_given_count;
	return (current_work.empty() ? "nowork" : current_work);
}
