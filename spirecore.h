#ifndef SPIRECORE_H_
#define SPIRECORE_H_

#include <cstdint>
#include <string>

struct Core
{
	struct ModValues
	{
		std::uint16_t step;
		std::uint16_t soft_cap;
		std::uint16_t hard_cap;
	};

	struct TierInfo
	{
		const char *name;
		ModValues mods[5];
	};

	unsigned tier;
	std::uint16_t fire;
	std::uint16_t poison;
	std::uint16_t lightning;
	std::uint16_t strength;
	std::uint16_t condenser;

	static const TierInfo tiers[];
	static const char *mod_names[];
	static const unsigned value_scale;

	Core();
	Core(const std::string &);
};

#endif
