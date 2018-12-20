#include <cctype>
#include <iostream>
#include <iomanip>
#include "spirelayout.h"

using namespace std;

TrapUpgrades::TrapUpgrades():
	fire(1),
	frost(1),
	poison(1),
	lightning(1)
{ }

TrapUpgrades::TrapUpgrades(const string &upgrades)
{
	if(upgrades.size()!=4)
		throw invalid_argument("TrapUpgrades::TrapUpgrades");
	for(auto c: upgrades)
		if(!isdigit(c))
			throw invalid_argument("TrapUpgrades::TrapUpgrades");

	fire = upgrades[0]-'0';
	frost = upgrades[1]-'0';
	poison = upgrades[2]-'0';
	lightning = upgrades[3]-'0';
}

string TrapUpgrades::str() const
{
	char buf[4];
	buf[0] = '0'+fire;
	buf[1] = '0'+frost;
	buf[2] = '0'+poison;
	buf[3] = '0'+lightning;
	return string(buf, 4);
}


TrapEffects::TrapEffects(const TrapUpgrades &upgrades):
	fire_damage(50),
	frost_damage(10),
	chill_dur(3),
	poison_damage(5),
	lightning_damage(50),
	shock_dur(1),
	damage_multi(2),
	special_multi(2)
{
	if(upgrades.fire>=2)
		fire_damage *= 10;
	if(upgrades.fire>=3)
		fire_damage *= 5;
	if(upgrades.fire>=4)
		fire_damage *= 2;
	if(upgrades.fire>=5)
		fire_damage *= 2;
	if(upgrades.fire>=6)
		fire_damage *= 10;
	if(upgrades.fire>=7)
		fire_damage *= 10;
	if(upgrades.fire>=8)
		fire_damage *= 100;

	if(upgrades.frost>=2)
	{
		++chill_dur;
		frost_damage *= 5;
	}
	if(upgrades.frost>=3)
		frost_damage *= 10;
	if(upgrades.frost>=4)
		frost_damage *= 5;
	if(upgrades.frost>=5)
		frost_damage *= 2;
	if(upgrades.frost>=6)
	{
		++chill_dur;
		frost_damage *= 5;
	}

	if(upgrades.poison>=2)
		poison_damage *= 2;
	if(upgrades.poison>=4)
		poison_damage *= 2;
	if(upgrades.poison>=5)
		poison_damage *= 2;
	if(upgrades.poison>=6)
		poison_damage *= 2;
	if(upgrades.poison>=7)
		poison_damage *= 2;

	if(upgrades.lightning>=2)
	{
		lightning_damage *= 10;
		++shock_dur;
	}
	if(upgrades.lightning>=3)
	{
		lightning_damage *= 10;
		damage_multi *= 2;
	}
	if(upgrades.lightning>=5)
	{
		lightning_damage *= 10;
		++shock_dur;
	}
	if(upgrades.lightning>=6)
	{
		lightning_damage *= 10;
		damage_multi *= 2;
	}
}


const char Layout::traps[] = "_FZPLSCK";

Layout::Layout():
	damage(0),
	cost(0),
	cycle(0)
{ }

uint64_t Layout::update_damage()
{
	damage = simulate(0);
	if(upgrades.poison>=5)
	{
		uint64_t low = damage;
		uint64_t high = simulate(low);
		for(unsigned i=0; (i<10 && low*101<high*100); ++i)
		{
			uint64_t mid = (low+high*3)/4;
			damage = simulate(mid);
			if(damage>mid)
				low = mid;
			else
			{
				high = mid;
				low = damage;
			}
		}

		damage = low;
	}

	return damage;
}

uint64_t Layout::simulate(uint64_t max_hp, bool debug) const
{
	unsigned slots = data.size();

	uint8_t column_flags[5] = { };
	for(unsigned i=0; i<slots; ++i)
		if(data[i]=='L')
			++column_flags[i%5];

	vector<uint16_t> floor_flags(slots/5, 0);
	for(unsigned i=0; i<slots; ++i)
	{
		unsigned j = i/5;
		char t = data[i];
		if(t=='F')
			floor_flags[j] += 1+0x10*column_flags[i%5];
		else if(t=='S')
			floor_flags[j] |= 0x08;
	}

	TrapEffects effects(upgrades);

	if(debug)
		cout << "Enemy HP: " << max_hp << endl;

	unsigned chilled = 0;
	unsigned frozen = 0;
	unsigned shocked = 0;
	unsigned damage_multi = 1;
	unsigned special_multi = 1;
	uint64_t poison = 0;
	uint64_t sim_damage = 0;
	uint64_t last_fire = 0;
	unsigned step = 0;
	for(unsigned i=0; i<slots; )
	{
		char t = data[i];
		bool antifreeze = false;
		if(t=='Z')
		{
			sim_damage += effects.frost_damage*damage_multi;
			chilled = effects.chill_dur*special_multi+1;
			frozen = 0;
			antifreeze = true;
		}
		else if(t=='F')
		{
			unsigned d = effects.fire_damage*damage_multi;
			if(floor_flags[i/5]&0x08)
				d *= 2;
			if(chilled && upgrades.frost>=3)
				d = d*5/4;
			if(upgrades.lightning>=4)
				d = d*(10+column_flags[i%5])/10;
			sim_damage += d;
			last_fire = sim_damage;
		}
		else if(t=='P')
		{
			unsigned p = effects.poison_damage*damage_multi;
			if(upgrades.frost>=4 && i+1<slots && data[i+1]=='Z')
				p *= 4;
			if(upgrades.poison>=3)
			{
				if(i>0 && data[i-1]=='P')
					p *= 3;
				if(i+1<slots && data[i+1]=='P')
					p *= 3;
			}
			if(upgrades.poison>=5 && max_hp && sim_damage*4>=max_hp)
				p *= 5;
			if(upgrades.lightning>=4)
				p = p*(10+column_flags[i%5])/10;
			poison += p;
		}
		else if(t=='L')
		{
			sim_damage += effects.lightning_damage*damage_multi;
			shocked = effects.shock_dur+1;
			damage_multi = effects.damage_multi;
			special_multi = effects.special_multi;
		}
		else if(t=='S')
		{
			uint16_t flags = floor_flags[i/5];
			uint64_t d = effects.fire_damage*(flags&0x07);
			if(upgrades.lightning>=4)
				d += effects.fire_damage*(flags>>4)/10;
			d *= 2*damage_multi;
			if(chilled && upgrades.frost>=3)
				d = d*5/4;
			sim_damage += d;
		}
		else if(t=='K')
		{
			if(chilled)
			{
				chilled = 0;
				frozen = 5*special_multi+1;
			}
			antifreeze = true;
		}
		else if(t=='C')
			poison = poison*(4+special_multi)/4;

		sim_damage += poison;

		if(debug)
		{
			cout << setw(2) << i << ':' << step << ": " << t << ' ' << setw(9) << sim_damage;
			if(max_hp && upgrades.poison>=5)
			{
				if(sim_damage>max_hp)
					cout << "  0%";
				else if(sim_damage)
					cout << ' ' << setw(2) << (max_hp-sim_damage)*100/max_hp << '%';
				else
					cout << " **%";
			}
			if(poison)
				cout << " P" << setw(6) << poison;
			else
				cout << "        ";
			if(frozen)
				cout << " F" << setw(2) << frozen;
			else if(chilled)
				cout << " C" << setw(2) << chilled;
			else
				cout << "    ";
			if(shocked)
				cout << " S" << shocked;
			cout << endl;
		}

		++step;
		if(shocked)
		{
			if(!--shocked)
			{
				damage_multi = 1;
				special_multi = 1;
			}
		}
		if(!antifreeze)
		{
			if(chilled && step<2)
				continue;
			if(frozen && step<3)
				continue;
		}

		if(chilled)
			--chilled;
		if(frozen)
			--frozen;
		step = 0;
		++i;
	}

	if(upgrades.fire>=4)
		sim_damage = max(sim_damage, last_fire*5/4);

	if(debug)
		cout << "Total damage: " << sim_damage << endl;

	return sim_damage;
}

uint64_t Layout::update_cost()
{
	uint64_t fire_cost = 100;
	uint64_t frost_cost = 100;
	uint64_t poison_cost = 500;
	uint64_t lightning_cost = 1000;
	uint64_t strength_cost = 3000;
	uint64_t condenser_cost = 6000;
	uint64_t knowledge_cost = 9000;
	cost = 0;
	for(char t: data)
	{
		uint64_t prev_cost = cost;
		if(t=='F')
		{
			cost += fire_cost;
			fire_cost = fire_cost*3/2;
		}
		else if(t=='Z')
		{
			cost += frost_cost;
			frost_cost *= 5;
		}
		else if(t=='P')
		{
			cost += poison_cost;
			poison_cost = poison_cost*7/4;
		}
		else if(t=='L')
		{
			cost += lightning_cost;
			lightning_cost *= 3;
		}
		else if(t=='S')
		{
			cost += strength_cost;
			strength_cost *= 100;
		}
		else if(t=='C')
		{
			cost += condenser_cost;
			condenser_cost *= 100;
		}
		else if(t=='K')
		{
			cost += knowledge_cost;
			knowledge_cost *= 100;
		}

		if(cost<prev_cost)
		{
			cost = numeric_limits<uint64_t>::max();
			break;
		}
	}

	return cost;
}

void Layout::cross_from(const Layout &other, Random &random)
{
	unsigned slots = min(data.size(), other.data.size());
	for(unsigned i=0; i<slots; ++i)
		if(random()&1)
			data[i] = other.data[i];
}

void Layout::mutate(unsigned mode, unsigned count, Random &random)
{
	unsigned slots = data.size();
	unsigned locality = (slots>=10 ? random()%(slots*2/15) : 0);
	unsigned base = 0;
	if(locality)
	{
		slots -= locality*5;
		base = (random()%locality)*5;
	}

	unsigned traps_count = (upgrades.lightning>0 ? 7 : 5);
	for(unsigned i=0; i<count; ++i)
	{
		unsigned op = 0;  // replace only
		if(mode==1)       // permute only
			op = 1+random()%4;
		else if(mode==2)  // any operation
			op = random()%8;

		unsigned t = 1+random()%traps_count;
		if(!upgrades.lightning && t>=4)
			++t;
		char trap = traps[t];

		if(op==0)  // replace
			data[base+random()%slots] = trap;
		else if(op==1 || op==2 || op==5)  // swap, rotate, insert
		{
			unsigned pos = base+random()%slots;
			unsigned end = base+random()%(slots-1);
			while(end==pos)
				++end;

			if(op==1)
				swap(data[pos], data[end]);
			else
			{
				if(op==2)
					trap = data[end];

				for(unsigned j=end; j>pos; --j)
					data[j] = data[j-1];
				for(unsigned j=end; j<pos; ++j)
					data[j] = data[j+1];
				data[pos] = trap;
			}
		}
		else if(slots>=10)  // floor operations
		{
			unsigned floors = slots/5;
			unsigned pos = random()%floors;
			unsigned end = random()%(floors-1);
			while(end==pos)
				++end;

			pos = base+pos*5;
			end = base+end*5;

			if(op==3 || op==6)  // rotate, duplicate
			{
				char floor[5];
				for(unsigned j=0; j<5; ++j)
					floor[j] = data[end+j];

				for(unsigned j=end; j>pos; --j)
					data[j+4] = data[j-1];
				for(unsigned j=end; j<pos; ++j)
					data[j] = data[j+5];

				if(op==3)
				{
					for(unsigned j=0; j<5; ++j)
						data[pos+j] = floor[j];
				}
			}
			else if(op==7)  // copy
			{
				for(unsigned j=0; j<5; ++j)
					data[end+j] = data[pos+j];
			}
			else if(op==4)  // swap
			{
				for(unsigned j=0; j<5; ++j)
					swap(data[pos+j], data[end+j]);
			}
		}
	}
}

bool Layout::is_valid() const
{
	unsigned slots = data.size();
	bool have_strength = false;
	for(unsigned i=0; i<slots; ++i)
	{
		if(i%5==0)
			have_strength = false;
		if(data[i]=='S')
		{
			if(have_strength)
				return false;
			have_strength = true;
		}
	}

	return true;
}
