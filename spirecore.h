#ifndef SPIRECORE_H_
#define SPIRECORE_H_

#include <cstdint>
#include <string>
#include "types.h"

struct Core
{
	enum
	{
		N_MODS = 6
	};

	struct ModValues
	{
		std::uint16_t step;
		std::uint16_t base;
		std::uint16_t soft_cap;
		std::uint16_t hard_cap;
	};

	struct TierInfo
	{
		const char *name;
		unsigned upgrade_cost;
		unsigned cost_increase;
		std::uint8_t max_mods;
		ModValues mods[N_MODS];
	};

	std::int16_t tier;
	std::uint16_t fire;
	std::uint16_t poison;
	std::uint16_t lightning;
	std::uint16_t strength;
	std::uint16_t condenser;
	std::uint16_t runestones;
	Number cost;

	static const TierInfo tiers[];
	static const char *mod_names[];
	static const unsigned value_scale;

	Core();
	Core(const std::string &);

	void set_mod(unsigned, std::uint16_t);
	std::uint16_t get_mod(unsigned) const;
	Number get_mod_cost(unsigned, std::uint16_t);
	void update();

	std::string get_type() const;

	std::string str(bool = false) const;

	void mutate(unsigned, Random &);
};

#endif
