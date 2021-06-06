#include "spirecore.h"
#include "stringutils.h"

using namespace std;

const Core::TierInfo Core::tiers[N_TIERS] =
{
	{ "common", 20, 150, 1, {{ 16, 160, 400, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 16, 16, 160, 0 }, { 0, 0, 0, 0 }, { 16, 160, 400, 0 }} },
	{ "uncommon", 200, 150, 2, {{ 16, 160, 400, 0 }, { 16, 160, 400, 0 }, { 0, 0, 0, 0 }, { 16, 16, 160, 0 }, { 4, 16, 80, 160 }, { 16, 160, 400, 0 }} },
	{ "rare", 2000, 125, 3, {{ 16, 160, 400, 0 }, { 16, 160, 400, 0 }, { 16, 16, 160, 0 }, { 16, 16, 160, 0 }, { 4, 16, 80, 160 }, { 16, 160, 400, 0 }} },
	{ "epic", 20000, 119, 3, {{ 16, 400, 800, 0 }, { 16, 400, 800, 0 }, { 16, 160, 320, 0 }, { 16, 160, 320, 0 }, { 4, 80, 160, 240 }, { 16, 400, 800, 0 }} },
	{ "legendary", 200000, 115, 3, {{ 32, 800, 1600, 0 }, { 32, 800, 1600, 0 }, { 32, 320, 800, 0 }, { 32, 320, 800, 0 }, { 8, 80, 240, 400 }, { 32, 800, 1600, 0 }} },
	{ "magnificent", 2000000, 112, 4, {{ 48, 1600, 3184, 0 }, { 48, 1600, 3184, 0 }, { 32, 800, 1600, 0 }, { 48, 800, 1600, 0 }, { 8, 160, 320, 560 }, {48, 1600, 3184, 0 }} },
	{ "ethereal", 20000000, 110, 4, {{ 64, 3200, 6400, 0 }, { 64, 3200, 6400, 0 }, { 64, 1600, 3184, 0 }, { 64, 1600, 3184, 0 }, { 8, 320, 480, 800 }, { 64, 3200, 6400, 0 }} }
};

Number Core::upgrade_cost_lookup[N_TIERS*100] = { };

const char *Core::mod_names[N_MODS] = { "fire", "poison", "lightning", "strength", "condenser", "runestones" };

const unsigned Core::value_scale = 16;

Core::Core():
	tier(-1),
	fire(0),
	poison(0),
	lightning(0),
	strength(0),
	condenser(0),
	runestones(0),
	cost(0)
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
		for(tier=0; tier<N_TIERS; ++tier)
			if(parts[0]==tiers[tier].name)
				break;
	}

	if(tier>=N_TIERS)
		throw invalid_argument("Invalid core tier "+parts[0]);
	if(parts.size()>static_cast<unsigned>(tiers[tier].max_mods)+1)
		throw invalid_argument("Too many mods for tier "+parts[0]);
	if(parts.size()<2 || parts.size()<static_cast<unsigned>(tiers[tier].max_mods))
		throw invalid_argument("Too few mods for tier "+parts[0]);

	for(unsigned i=1; i<parts.size(); ++i)
	{
		const string &part = parts[i];
		string::size_type colon = part.find(':');

		unsigned mod = 0;
		for(; mod<N_MODS; ++mod)
			if(!part.compare(0, colon, mod_names[mod]) || (colon==1 && tolower(part[0])==mod_names[mod][0]))
				break;

		if(mod>=N_MODS)
			throw invalid_argument("Invalid core mod "+part);

		unsigned value = tiers[tier].mods[mod].soft_cap;
		if(colon!=string::npos)
		{
			value = parse_value<float>(part.substr(colon+1))*value_scale;
			unsigned hard_cap = tiers[tier].mods[mod].hard_cap;
			if(value<tiers[tier].mods[mod].base)
				throw invalid_argument("Core mod "+part+" is below base value");
			if(hard_cap && value>hard_cap)
				throw invalid_argument("Core mod "+part+" exceeds hard cap");
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
	case 5: return runestones;
	default: return 0;
	}
}

unsigned Core::get_n_mods() const
{
	unsigned count = 0;
	for(unsigned i=0; i<N_MODS; ++i)
		count += (get_mod(i)!=0);
	return count;
}

Number Core::get_mod_cost(unsigned mod, uint16_t value)
{
	const ModValues &mod_vals = tiers[tier].mods[mod];
	if(value<=mod_vals.base)
		return 0;

	Number result = 0;
	unsigned steps = (min<unsigned>(value, mod_vals.soft_cap)-mod_vals.base)/mod_vals.step;
	Number step_cost = tiers[tier].upgrade_cost;
	result += step_cost*steps;

	if(value>mod_vals.soft_cap)
	{
		steps = (value-mod_vals.soft_cap)/mod_vals.step;
		if(steps>=100)
		{
			double increase = tiers[tier].cost_increase/100.0;
			result += step_cost*static_cast<Number>((pow(increase, steps)-1)/(increase-1));
		}
		else if(steps>0)
		{
			if(!upgrade_cost_lookup[tier*100])
			{
				unsigned increase = tiers[tier].cost_increase;
				Number lookup_cost = 0;
				for(unsigned i=0; i<100; ++i)
				{
					lookup_cost += step_cost;
					upgrade_cost_lookup[tier*100+i] = lookup_cost;
					step_cost = step_cost*increase/100;
				}
			}

			result += upgrade_cost_lookup[tier*100+steps-1];
		}
	}

	return result;
}

void Core::update()
{
	cost = 0;
	for(unsigned i=0; i<N_MODS; ++i)
		cost += get_mod_cost(i, get_mod(i));
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

void Core::mutate(MutateMode mode, unsigned count, Random &random)
{
	const TierInfo &tier_info = tiers[tier];

	unsigned n_mods = 0;
	unsigned n_swap = 0;
	for(unsigned i=0; i<N_MODS; ++i)
	{
		if(get_mod(i))
			++n_mods;
		else if(tier_info.mods[i].base)
			++n_swap;
	}

	if(!n_mods)
		return;

	unsigned op_range = (mode==VALUES_ONLY ? 2 : 3);
	for(unsigned i=0; i<count; ++i)
	{
		unsigned op = random()%op_range;
		unsigned mod = random()%n_mods;
		for(unsigned j=0; j<=mod; ++j)
			if(!get_mod(j))
				++mod;
		const ModValues &mod_vals = tier_info.mods[mod];

		if(op==0)
		{
			uint16_t value = get_mod(mod)+mod_vals.step;
			if(mod_vals.hard_cap)
				value = min(value, mod_vals.hard_cap);
			set_mod(mod, value);
		}
		else if(op==1)
		{
			uint16_t value = get_mod(mod)-mod_vals.step;
			value = max(value, mod_vals.base);
			set_mod(mod, value);
		}
		else if(op==2)
		{
			unsigned other = random()%n_swap;
			for(unsigned j=0; j<=other; ++j)
				if(get_mod(j) || !tier_info.mods[j].base)
					++other;

			uint16_t value = get_mod(mod);
			Number mod_cost = get_mod_cost(mod, value);
			const ModValues &other_vals = tier_info.mods[other];
			value = other_vals.soft_cap+(value-mod_vals.base)*other_vals.step/mod_vals.step;
			if(other_vals.hard_cap)
				value = min(value, other_vals.hard_cap);
			while(get_mod_cost(other, value)>mod_cost)
				value -= other_vals.step;

			set_mod(mod, 0);
			set_mod(other, value);
		}
	}
}
