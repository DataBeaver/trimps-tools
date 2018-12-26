#ifndef SPIRELAYOUT_H_
#define SPIRELAYOUT_H_

#include <cstdint>
#include <vector>
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
	unsigned fire_damage;
	unsigned frost_damage;
	unsigned chill_dur;
	unsigned poison_damage;
	unsigned lightning_damage;
	unsigned shock_dur;
	unsigned damage_multi;
	unsigned special_multi;

	TrapEffects(const TrapUpgrades &);
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
		FULL
	};

	enum MutateMode
	{
		REPLACE_ONLY,
		PERMUTE_ONLY,
		ALL_MUTATIONS
	};

	struct SimResult
	{
		Number max_hp;
		Number damage;
		Number toxicity;
		std::uint16_t runestone_pct;
		std::uint16_t steps_taken;
		int kill_cell;

		SimResult();
	};

	static const char traps[];

private:
	struct Step
	{
		std::uint16_t cell;
		char trap;
		std::uint8_t slow;
		bool shock;
		std::uint8_t kill_pct;
		std::uint8_t toxic_pct;
		std::uint8_t rs_bonus;
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
	std::string data;
	Number damage;
	Number cost;
	Number rs_per_sec;
	unsigned threat;
	unsigned cycle;

public:
	Layout();

	void set_upgrades(const TrapUpgrades &);
	void set_traps(const std::string &, unsigned = 0);
	const TrapUpgrades &get_upgrades() const { return upgrades; }
	const std::string &get_traps() const { return data; }
private:
	void build_steps(std::vector<Step> &) const;
	SimResult simulate(const std::vector<Step> &, Number, std::vector<SimDetail> * = 0) const;
	void build_results(const std::vector<Step> &, unsigned, std::vector<SimResult> &) const;
	template<typename F>
	unsigned integrate_results_for_threat(const std::vector<SimResult> &, unsigned, const F &) const;
public:
	void update(UpdateMode, unsigned = 12);
private:
	void update_damage(const std::vector<Step> &, unsigned);
	void update_damage(const std::vector<Step> &, const std::vector<SimResult> &, unsigned);
	void refine_damage(const std::vector<Step> &, Number, Number, unsigned);
	void update_cost();
	void update_threat(const std::vector<SimResult> &, unsigned);
	void update_runestones(const std::vector<SimResult> &);
public:
	void cross_from(const Layout &, Random &);
	void mutate(MutateMode, unsigned, Random &, unsigned);
	Number get_damage() const { return damage; }
	Number get_cost() const { return cost; }
	Number get_runestones_per_second() const { return rs_per_sec; }
	unsigned get_threat() const { return threat; }
	unsigned get_cycle() const { return cycle; }
	bool is_valid() const;
	void debug(Number) const;
	void build_cell_info(std::vector<CellInfo> &, Number) const;
};

#endif
