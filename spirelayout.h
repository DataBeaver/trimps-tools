#ifndef SPIRELAYOUT_H_
#define SPIRELAYOUT_H_

#include <cstdint>
#include <random>
#include <vector>

struct TrapUpgrades
{
	uint16_t fire;
	uint16_t frost;
	uint16_t poison;
	uint16_t lightning;

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
	std::uint64_t direct_damage;
	std::uint64_t toxicity;

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
		uint64_t max_hp;
		uint64_t damage;
		uint64_t toxicity;
		unsigned runestone_pct;
		unsigned steps_taken;
		int kill_cell;

		SimResult();
	};

	TrapUpgrades upgrades;
	std::string data;
	std::uint64_t damage;
	std::uint64_t cost;
	std::uint64_t rs_per_sec;
	unsigned threat;
	unsigned cycle;

	static const char traps[];

	Layout();

	void build_steps(std::vector<Step> &) const;
	SimResult simulate(const std::vector<Step> &, std::uint64_t, bool = false) const;
	void build_results(const std::vector<Step> &, unsigned, std::vector<SimResult> &) const;
	SimResult interpolate_result(const std::vector<SimResult> &, std::uint64_t) const;
	template<typename F>
	unsigned integrate_results_for_threat(const std::vector<SimResult> &, unsigned, const F &) const;
	void update(UpdateMode);
	void update_damage(const std::vector<Step> &);
	void update_damage(const std::vector<Step> &, const std::vector<SimResult> &);
	void refine_damage(const std::vector<Step> &, uint64_t, uint64_t);
	void update_cost();
	void update_threat(const std::vector<SimResult> &);
	void update_runestones(const std::vector<SimResult> &);
	void cross_from(const Layout &, Random &);
	void mutate(unsigned, unsigned, Random &);
	bool is_valid() const;
};

#endif
