#ifndef SPIRELAYOUT_H_
#define SPIRELAYOUT_H_

#include <cstdint>
#include <random>
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

struct Layout
{
	typedef std::minstd_rand Random;

	enum UpdateMode
	{
		FAST,
		COMPATIBLE,
		FULL
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

	TrapUpgrades upgrades;
	std::string data;
	Number damage;
	Number cost;
	Number rs_per_sec;
	unsigned threat;
	unsigned cycle;

	static const char traps[];

	Layout();

	void build_steps(std::vector<Step> &) const;
	SimResult simulate(const std::vector<Step> &, Number, bool = false) const;
	void build_results(const std::vector<Step> &, unsigned, std::vector<SimResult> &) const;
	template<typename F>
	unsigned integrate_results_for_threat(const std::vector<SimResult> &, unsigned, const F &) const;
	void update(UpdateMode);
	void update_damage(const std::vector<Step> &);
	void update_damage(const std::vector<Step> &, const std::vector<SimResult> &);
	void refine_damage(const std::vector<Step> &, Number, Number);
	void update_cost();
	void update_threat(const std::vector<SimResult> &);
	void update_runestones(const std::vector<SimResult> &);
	void cross_from(const Layout &, Random &);
	void mutate(unsigned, unsigned, Random &);
	bool is_valid() const;
};

#endif
