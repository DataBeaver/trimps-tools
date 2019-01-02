#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include "getopt.h"
#include "types.h"

struct PerkInfo
{
	enum CostMode
	{
		MULTIPLICATIVE,
		ADDITIVE
	};

	const char *name;
	double base_cost;
	CostMode cost_mode;
	double cost_growth;
	unsigned max_level;
};

struct EvalStats
{
	double population;
	double production;
	double loot;
	double army;
	unsigned prestige_level;
	unsigned equipment_level;
	unsigned geneticists;
	double health;
	double attack;
};

class Perks
{
private:
	typedef std::map<std::string, unsigned> LevelMap;

	double base_pop;
	unsigned target_zone;
	double target_breed_time;
	double equip_time;
	double helium_budget;
	LevelMap perk_levels;

	static const PerkInfo perk_info[];

public:
	Perks(int, char **);

	int main();
private:
	void optimize();
	void print_perks() const;
	unsigned get_perk(const std::string &) const;
	double get_perk_cost(const PerkInfo &, unsigned, unsigned = 1) const;
	double evaluate(EvalStats &) const;
	double evaluate() const;
};

using namespace std;

int main(int argc, char **argv)
{
	try
	{
		Perks perks(argc, argv);
		return perks.main();
	}
	catch(const usage_error &e)
	{
		cout << e.what() << endl;
		cout << e.help() << endl;
	}
}

const PerkInfo Perks::perk_info[] =
{
	{ "looting", 1, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "toughness", 1, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "power", 1, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "motivation", 2, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "pheromones", 3, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "agility", 4, PerkInfo::MULTIPLICATIVE, 1.3, 20 },
	{ "range", 1, PerkInfo::MULTIPLICATIVE, 1.3, 10 },
	{ "artisanistry", 15, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "carpentry", 25, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "relentlessness", 75, PerkInfo::MULTIPLICATIVE, 1.3, 20 },
	{ "meditation", 75, PerkInfo::MULTIPLICATIVE, 1.3, 7 },
	{ "resilience", 100, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "siphonology", 100e3, PerkInfo::MULTIPLICATIVE, 1.3, 3 },
	{ "coordinated", 150e3, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "resourceful", 50e3, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "overkill", 1e6, PerkInfo::MULTIPLICATIVE, 1.3, 30 },
	{ "curious", 100e12, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "cunning", 100e9, PerkInfo::MULTIPLICATIVE, 1.3, 0 },
	{ "capable", 100e6, PerkInfo::MULTIPLICATIVE, 10, 10 },
	{ "toughness2", 20e3, PerkInfo::ADDITIVE, 500, 0 },
	{ "power2", 20e3, PerkInfo::ADDITIVE, 500, 0 },
	{ "motivation2", 50e3, PerkInfo::ADDITIVE, 1000, 0 },
	{ "carpentry2", 100e3, PerkInfo::ADDITIVE, 10e3, 0 },
	{ "looting2", 100e3, PerkInfo::ADDITIVE, 10e3, 0 },
	{ 0, 0, PerkInfo::ADDITIVE, 0, 0 }
};

Perks::Perks(int argc, char **argv):
	base_pop(1),
	target_zone(10),
	target_breed_time(45),
	equip_time(120),
	helium_budget(0)
{
	DoubleIO base_pop_io;
	DoubleIO helium_io;

	GetOpt getopt;
	for(unsigned i=0; perk_info[i].name; ++i)
		getopt.add_option(perk_info[i].name, perk_levels[perk_info[i].name], GetOpt::REQUIRED_ARG);
	getopt.add_argument("base_pop", base_pop_io, GetOpt::REQUIRED_ARG);
	getopt.add_argument("target_zone", target_zone, GetOpt::REQUIRED_ARG);
	getopt.add_argument("helium_budget", helium_io, GetOpt::REQUIRED_ARG);
	getopt(argc, argv);

	base_pop = base_pop_io;
	helium_budget = helium_io;
}

int Perks::main()
{
	optimize();
	print_perks();

	return 0;
}

void Perks::optimize()
{
	double helium_spent = 0;
	for(unsigned i=0; perk_info[i].name; ++i)
		helium_spent += get_perk_cost(perk_info[i], 0, get_perk(perk_info[i].name));

	double score = evaluate();
	while(1)
	{
		double free_helium = helium_budget-helium_spent;
		const PerkInfo *best_perk = 0;
		unsigned best_inc = 1;
		double best_eff = 0;
		for(unsigned i=0; perk_info[i].name; ++i)
		{
			const PerkInfo &perk = perk_info[i];
			unsigned &level = perk_levels[perk.name];
			if(perk.max_level && level>=perk.max_level)
				continue;

			unsigned inc = 1;
			if(perk.cost_mode==PerkInfo::ADDITIVE)
			{
				double a = perk.cost_growth/2;
				double b = perk.base_cost+perk.cost_growth*level;
				double c = -free_helium/1000;
				inc = ceil((sqrt(b*b-4*a*c)-b)/(2*a));
			}

			double cost = get_perk_cost(perk, level, inc);
			if(cost>free_helium)
				continue;

			level += inc;
			double new_score = evaluate();
			level -= inc;

			double eff = (new_score-score)/cost;
			if(eff>best_eff)
			{
				best_perk = &perk;
				best_inc = inc;
				best_eff = eff;
			}
		}

		if(!best_perk)
			break;

		unsigned &level = perk_levels[best_perk->name];
		double cost = get_perk_cost(*best_perk, level, best_inc);
		helium_spent += cost;
		level += best_inc;

		score = evaluate();
	}
}

void Perks::print_perks() const
{
	EvalStats stats;
	evaluate(stats);

	double helium_spent = 0;
	for(unsigned i=0; perk_info[i].name; ++i)
	{
		const PerkInfo &perk = perk_info[i];
		unsigned level = get_perk(perk.name);
		double cost = get_perk_cost(perk, 0, level);
		helium_spent += cost;
		cout << left << setw(14) << perk.name << "  " << setw(8) << right << level << "  "
			<< setw(6) << stringify(DoubleIO(cost)) << " He  " << setw(5) << setprecision(2) << fixed << cost*100/helium_budget << "%" << endl;
	}

	cout << "Helium spent: " << DoubleIO(helium_spent) << endl;
	cout << "Population: " << DoubleIO(stats.population) << endl;
	cout << "Army size: " << DoubleIO(stats.army) << endl;
	cout << "Geneticists: " << stats.geneticists << endl;
}

unsigned Perks::get_perk(const string &name) const
{
	auto i = perk_levels.find(name);
	return (i!=perk_levels.end() ? i->second : 0);
}

double Perks::get_perk_cost(const PerkInfo &perk, unsigned level, unsigned count) const
{
	if(perk.cost_mode==PerkInfo::ADDITIVE)
		return (perk.base_cost+perk.cost_growth*level)*count + perk.cost_growth*count*(count-1)/2;
	else
	{
		double cost = 0;
		for(unsigned i=0; i<count; ++i)
			cost += ceil((level+i)/2.0 + perk.base_cost*pow(perk.cost_growth, level+i));
		return cost;
	}
}

double Perks::evaluate(EvalStats &stats) const
{
	stats.population = base_pop;
	stats.population *= pow(1.1, get_perk("carpentry"));
	stats.population *= 1+0.0025*get_perk("carpentry2");

	unsigned max_coords = target_zone-1;
	if(target_zone>=230)
		max_coords += 100;

	double coord_factor = 1+0.25*pow(0.98, get_perk("coordinated"));
	unsigned coords = 0;
	stats.army = 1;
	while(coords<max_coords && stats.army*3<stats.population)
	{
		++coords;
		stats.army = ceil(stats.army*coord_factor);
	}

	stats.production = stats.population/4;
	stats.production *= 1+0.05*get_perk("motivation");
	stats.production *= 1+0.01*get_perk("motivation2");
	stats.production *= 1+0.07*get_perk("meditation");
	// Speed books
	stats.production *= pow(1.25, min(target_zone, 59U));
	// Mega books
	if(target_zone>=60)
		stats.production *= pow(1.6, target_zone-59);
	// Bounty
	if(target_zone>=15)
		stats.production *= 2;
	// Whipimp
	stats.production *= pow(1.003, target_zone*3);

	double speed = 1/pow(0.95, get_perk("agility"));

	// Assume loot mostly comes from caches on perfect maps
	stats.loot = stats.production*20/25*speed;
	stats.loot *= 1+0.05*get_perk("looting");
	stats.loot *= 1+0.01*get_perk("looting2");
	// Magnimp
	stats.loot *= pow(1.003, target_zone*3);

	double income = stats.production+stats.loot;

	double equip_tier_cost = 40+40+55+80+100+140+160+230+275+375+415+450+500;
	equip_tier_cost *= pow(0.95, get_perk("artisanistry"));
	static constexpr double prestige_cost_multi = pow(1.069, 45.05);
	static constexpr double prestige_cost_offset = pow(1.069, 28.15);
	unsigned affordable_prestige = log(income*equip_time/equip_tier_cost*prestige_cost_offset)/log(prestige_cost_multi);
	unsigned max_prestige = (target_zone+5)/10*2;
	stats.prestige_level = min(affordable_prestige, max_prestige);

	equip_tier_cost *= pow(prestige_cost_multi, stats.prestige_level)/prestige_cost_offset;
	double affordable_level = log(income*equip_time/equip_tier_cost)/log(1.2);
	stats.equipment_level = max<unsigned>(affordable_level, 1U);

	double coord_stats = pow(1.25, coords);

	double breed_rate = 0.0085;
	breed_rate *= 1+0.1*get_perk("pheromones");
	if(target_zone>=60)
		breed_rate /= 10;
	// Potency upgrades
	breed_rate *= pow(1.1, target_zone/5);
	// Venimp
	breed_rate *= pow(1.003, target_zone*3);

	double breed_time = -log(1-stats.army*2/stats.population)/breed_rate;
	stats.geneticists = 0;
	if(breed_time<target_breed_time)
		stats.geneticists = ceil(log(target_breed_time/breed_time)/log(1/0.98));

	double overheat = 1;
	if(target_zone>=230)
		overheat = pow(0.8, target_zone-229);

	stats.health = 50;
	stats.health += (4+6+10+14+23+35+60)*pow(1.19, 14*stats.prestige_level)*affordable_level;
	stats.health *= coord_stats;
	stats.health *= pow(1.01, stats.geneticists);
	stats.health *= 1+0.05*get_perk("toughness");
	stats.health *= 1+0.01*get_perk("toughness2");
	stats.health *= pow(1.1, get_perk("resilience"));
	stats.health *= overheat;

	unsigned crit = get_perk("relentlessness");

	stats.attack = 6*coord_stats;
	stats.attack += (2+3+4+7+9+15)*pow(1.19, 13*stats.prestige_level)*affordable_level;
	stats.attack *= 1+0.05*get_perk("power");
	stats.attack *= 1+0.01*get_perk("power2");
	stats.attack *= 1+0.01*get_perk("range");
	stats.attack *= 1+0.05*crit*(1+0.3*crit);
	stats.attack *= overheat;

	double score = 0;
	score += log(speed);
	score += log(stats.health);
	score += log(stats.attack);

	return score;
}

double Perks::evaluate() const
{
	EvalStats stats;
	return evaluate(stats);
}
