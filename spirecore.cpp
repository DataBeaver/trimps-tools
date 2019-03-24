#include "spirecore.h"
#include "stringutils.h"

using namespace std;

const Core::TierInfo Core::tiers[] =
{
	{ "common", {{ 16, 400, 0 }, { 15, 400, 0 }, { 0, 0, 0 }, { 16, 400, 0 }, { 0, 0, 0 }} },
	{ "uncommon", {{ 16, 400, 0 }, { 16, 400, 0 }, { 0, 0, 0 }, { 16, 400, 0 }, { 4, 80, 160 }} },
	{ "rare", {{ 16, 400, 0 }, { 16, 400, 0 }, { 16, 400, 0 }, { 16, 400, 0 }, { 4, 80, 160 }} },
	{ "epic", {{ 16, 800, 0 }, { 16, 800, 0 }, { 16, 720, 0 }, { 16, 800, 0 }, { 4, 160, 240 }} },
	{ "legendary", {{ 32, 1600, 0 }, { 32, 1600, 0 }, { 32, 1600, 0 }, { 32, 1600, 0 }, { 8, 240, 400 }} },
	{ "magnificent", {{ 48, 3184, 0 }, { 48, 3184, 0 }, { 32, 2400, 0 }, { 48, 3184, 0 }, { 8, 320, 560 }} },
	{ "ethereal", {{ 64, 6400, 0 }, { 64, 6400, 0 }, { 64, 4000, 0 }, { 64, 4000, 0 }, { 8, 480, 800 }} },
	{ 0, {} }
};

const char *Core::mod_names[] = { "fire", "poison", "lightning", "strength", "condenser", 0 };

const unsigned Core::value_scale = 16;

Core::Core():
	tier(-1),
	fire(0),
	poison(0),
	lightning(0),
	strength(0),
	condenser(0)
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

	for(unsigned i=1; i<parts.size(); ++i)
	{
		const string &part = parts[i];
		string::size_type colon = part.find(':');

		unsigned mod = 0;
		for(; mod_names[mod]; ++mod)
			if(!part.compare(0, colon, mod_names[mod]) || (colon==1 && tolower(part[0])==mod_names[mod][0]))
				break;

		if(mod>=5)
			throw invalid_argument("Invalid core mod "+part);

		unsigned value = tiers[tier].mods[mod].soft_cap;
		if(colon!=string::npos)
		{
			value = parse_value<float>(part.substr(colon+1))*value_scale;
			unsigned hard_cap = tiers[tier].mods[mod].hard_cap;
			if(hard_cap && value>hard_cap)
				throw invalid_argument("Core mod "+part+" out of range");
		}

		switch(mod)
		{
		case 0: fire = value; break;
		case 1: poison = value; break;
		case 2: lightning = value; break;
		case 3: strength = value; break;
		case 4: condenser = value; break;
		}
	}
}
