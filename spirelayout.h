#ifndef SPIRELAYOUT_H_
#define SPIRELAYOUT_H_

#include <cstdint>
#include <vector>
#include "fixedpoint.h"
#include "spirecore.h"
#include "types.h"

struct TrapUpgrades
{
	std::uint16_t fire;
	std::uint16_t frost;
	std::uint16_t poison;
	std::uint16_t lightning;

	TrapUpgrades();
	TrapUpgrades(const std::string &);

	std::string str() const;
};

struct TrapEffects
{
	// Damage values are modified in 1% increments.
	Fixed<100> fire_damage;
	Fixed<100> frost_damage;
	unsigned chill_dur;
	Fixed<100> poison_damage;
	Fixed<100> lightning_damage;
	unsigned shock_dur;
	// Shock damage multiplier starts at 200% and is modified in 1% increments.
	Fixed<100, unsigned> shock_damage_multi;
	unsigned special_multi;
	// Lightning column bonus starts at 10% and is modified in 1% increments.
	Fixed<1000, unsigned> lightning_column_bonus;
	// Strength multiplier starts at 200% and is modified in 1% increments.
	Fixed<100, unsigned> strength_multi;
	// Condenser bonus starts at 25% and is modified in 0.25% increments.
	Fixed<1600, unsigned> condenser_bonus;
	// Runestone bonus occurs in 2% increments.
	Fixed<100, unsigned> slow_rs_bonus;

	TrapEffects(const TrapUpgrades &, const Core &);
};

struct CellInfo
{
	char trap;
	std::uint8_t steps;
	std::uint8_t shocked_steps;
	Number damage_taken;
	Number hp_left;

	CellInfo();
};

class Layout
{
public:
	enum UpdateMode
	{
		COST_ONLY,
		FAST,
		COMPATIBLE,
		EXACT_DAMAGE,
		FULL
	};

	enum MutateMode
	{
		REPLACE_ONLY,
		PERMUTE_ONLY,
		ALL_MUTATIONS
	};

	static const char traps[];

private:
	struct SimResult
	{
		Number sim_hp;
		Number max_hp;
		Number damage;
		Number toxicity;
		Fixed<100, std::uint16_t> runestone_multi;
		std::uint16_t steps_taken;
		int kill_cell;

		SimResult();
	};

	struct Step
	{
		std::uint16_t cell;
		char trap;
		std::uint8_t slow;
		bool shock;
		Fixed<100, std::uint8_t> rs_bonus;
		bool culling_strike;
		Fixed<1600, std::uint16_t> toxic_bonus;
		Number direct_damage;
		Number toxicity;

		Step();
	};

	struct SimDetail
	{
		Number damage_taken;
		Number toxicity;
		Number hp_left;
	};

	TrapUpgrades upgrades;
	Core core;
	std::string data;
	Number damage;
	Number cost;
	Number rs_per_sec;
	Fixed<16, unsigned> threat;
	unsigned cycle;

public:
	Layout();

	void set_upgrades(const TrapUpgrades &);
	void set_core(const Core &);
	void set_traps(const std::string &, unsigned = 0);
	const TrapUpgrades &get_upgrades() const { return upgrades; }
	const std::string &get_traps() const { return data; }
	unsigned get_tower_count() const;
	const Core &get_core() const { return core; }
private:
	void build_steps(std::vector<Step> &) const;
	SimResult simulate(const std::vector<Step> &, Number, bool, std::vector<SimDetail> * = 0) const;
	void build_results(const std::vector<Step> &, std::vector<SimResult> &) const;
	template<typename F>
	Number integrate_results(const std::vector<SimResult> &, Fixed<16, unsigned>, const F &) const;
public:
	void update(UpdateMode);
private:
	void update_damage(const std::vector<Step> &, unsigned);
	void update_damage(const std::vector<SimResult> &);
	void update_cost();
	void update_threat(const std::vector<SimResult> &);
	void update_runestones(const std::vector<SimResult> &);
public:
	void cross_from(const Layout &, Random &);
	void mutate(MutateMode, unsigned, Random &, unsigned);
	Number get_damage() const { return damage; }
	Number get_cost() const { return cost; }
	Number get_runestones_per_second() const { return rs_per_sec; }
	unsigned get_threat() const { return threat.round(); }
	unsigned get_cycle() const { return cycle; }
	bool is_valid() const;
	void debug(Number) const;
	void build_cell_info(std::vector<CellInfo> &, Number) const;
};

#endif
