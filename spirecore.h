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
		std::uint8_t max_mods;
		ModValues mods[5];
	};

	std::int16_t tier;
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

	void set_mod(unsigned, std::uint16_t);
	std::uint16_t get_mod(unsigned) const;

	std::string get_type() const;

	std::string str(bool = false) const;
};

#endif
