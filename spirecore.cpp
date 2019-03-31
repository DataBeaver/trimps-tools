#include "spirecore.h"
#include "stringutils.h"

using namespace std;

const Core::TierInfo Core::tiers[] =
{
	{ "common", 1, {{ 16, 400, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 16, 160, 0 }, { 0, 0, 0 }, { 16, 400, 0 }} },
	{ "uncommon", 2, {{ 16, 400, 0 }, { 16, 400, 0 }, { 0, 0, 0 }, { 16, 160, 0 }, { 4, 80, 160 }, { 16, 400, 0 }} },
	{ "rare", 3, {{ 16, 400, 0 }, { 16, 400, 0 }, { 16, 160, 0 }, { 16, 160, 0 }, { 4, 80, 160 }, { 16, 400, 0 }} },
	{ "epic", 3, {{ 16, 800, 0 }, { 16, 800, 0 }, { 16, 320, 0 }, { 16, 320, 0 }, { 4, 160, 240 }, { 16, 800, 0 }} },
	{ "legendary", 3, {{ 32, 1600, 0 }, { 32, 1600, 0 }, { 32, 800, 0 }, { 32, 800, 0 }, { 8, 240, 400 }, { 32, 1600, 0 }} },
	{ "magnificent", 4, {{ 48, 3184, 0 }, { 48, 3184, 0 }, { 32, 1600, 0 }, { 48, 1600, 0 }, { 8, 320, 560 }, {48, 3184, 0 }} },
	{ "ethereal", 4, {{ 64, 6400, 0 }, { 64, 6400, 0 }, { 64, 3184, 0 }, { 64, 3184, 0 }, { 8, 480, 800 }, { 64, 6400, 0 }} },
	{ 0, 0, 0, 0, {} }
};

const char *Core::mod_names[] = { "fire", "poison", "lightning", "strength", "condenser", "runestones", 0 };

const unsigned Core::value_scale = 16;

Core::Core():
	tier(-1),
	fire(0),
	poison(0),
	lightning(0),
	strength(0),
	condenser(0),
	runestones(0)
{ }

Core::Core(const string &desc):
	Core()
{
	vector<string> parts = split(desc, '/');
	if(parts.empty())
		throw invalid_argument("Invalid core description");

	if(isdigit(parts[0][0]))
		tier = parse_value<unsigned>(parts[0])-1;
	else
	{
		for(tier=0; tiers[tier].name; ++tier)
			if(parts[0]==tiers[tier].name)
				break;
	}

	if(tier>=7)
		throw invalid_argument("Invalid core tier "+parts[0]);
	if(parts.size()>static_cast<unsigned>(tiers[tier].max_mods)+1)
		throw invalid_argument("Too many mods for tier "+parts[0]);

	for(unsigned i=1; i<parts.size(); ++i)
	{
		const string &part = parts[i];
		string::size_type colon = part.find(':');

		unsigned mod = 0;
		for(; mod_names[mod]; ++mod)
			if(!part.compare(0, colon, mod_names[mod]) || (colon==1 && tolower(part[0])==mod_names[mod][0]))
				break;

		if(mod>=N_MODS)
			throw invalid_argument("Invalid core mod "+part);

		unsigned value = tiers[tier].mods[mod].soft_cap;
		if(colon!=string::npos)
		{
			value = parse_value<float>(part.substr(colon+1))*value_scale;
			unsigned hard_cap = tiers[tier].mods[mod].hard_cap;
			if(hard_cap && value>hard_cap)
				throw invalid_argument("Core mod "+part+" out of range");
		}

		set_mod(mod, value);
	}
}

void Core::set_mod(unsigned mod, uint16_t value)
{
	switch(mod)
	{
	case 0: fire = value; break;
	case 1: poison = value; break;
	case 2: lightning = value; break;
	case 3: strength = value; break;
	case 4: condenser = value; break;
	case 5: runestones = value; break;
	}
}

uint16_t Core::get_mod(unsigned mod) const
{
	switch(mod)
	{
	case 0: return fire;
	case 1: return poison;
	case 2: return lightning;
	case 3: return strength;
	case 4: return condenser;
	default: return 0;
	}
}

string Core::get_type() const
{
	string result;
	result.reserve(4);
	for(unsigned i=0; i<N_MODS; ++i)
		if(get_mod(i))
			result += toupper(mod_names[i][0]);
	return result;
}

string Core::str(bool compact) const
{
	if(tier<0)
		return string();

	string result;

	if(compact)
		result = stringify(tier+1);
	else
		result = tiers[tier].name;

	for(unsigned i=0; i<N_MODS; ++i)
	{
		unsigned value = get_mod(i);
		if(!value)
			continue;

		float fvalue = static_cast<float>(value)/value_scale;
		if(compact)
			result += format("/%c:%d", static_cast<char>(toupper(mod_names[i][0])), fvalue);
		else
			result += format("/%s:%d", mod_names[i], fvalue);
	}

	return result;
}
