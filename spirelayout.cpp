#include "spirelayout.h"
#include <cctype>
#include <iostream>
#include <iomanip>
#include <stdexcept>

using namespace std;

class WeightedAccumulator
{
private:
	Number high;
	Number low;
	Number total_weight;

	static constexpr unsigned bits = sizeof(Number)*8;
	static constexpr unsigned half_bits = bits/2;
	static constexpr Number low_half_mask = (Number(1)<<half_bits)-1;
	static constexpr Number high_bit = Number(1)<<(bits-1);

public:
	WeightedAccumulator();

	void add(Number, Number);
private:
	void adc(Number);
public:
	Number result() const;
};

WeightedAccumulator::WeightedAccumulator():
	high(0),
	low(0),
	total_weight(0)
{ }

void WeightedAccumulator::add(Number n, Number w)
{
	total_weight += w;
	Number nlow = n&low_half_mask;
	Number nhigh = n>>half_bits;
	Number wlow = w&low_half_mask;
	Number whigh = w>>half_bits;
	Number mid = nlow*whigh+nhigh*wlow;
	adc(nlow*wlow);
	adc(mid<<half_bits);
	high += nhigh*whigh+(mid>>half_bits);
}

void WeightedAccumulator::adc(Number n)
{
	Number l = low;
	low += n;
	if(low<l)
		++high;
}

Number WeightedAccumulator::result() const
{
	Number q = 0;
	Number r = high;
	Number bit = high_bit;
	while(bit)
	{
		r <<= 1;
		if(low&bit)
			r |= 1;
		if(r>=total_weight)
		{
			q |= bit;
			r -= total_weight;
		}
		bit >>= 1;
	}

	return q;
}


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


TrapEffects::TrapEffects(const TrapUpgrades &upgrades, const Core &core):
	fire_damage(50),
	frost_damage(10),
	chill_dur(3),
	poison_damage(5),
	lightning_damage(50),
	shock_dur(1),
	shock_damage_pml(2000),
	special_multi(2),
	lightning_column_pml(100),
	strength_pml(1000),
	condenser_pml(250)
{
	unsigned core_scale = 100*Core::value_scale;

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

	fire_damage = (Number(fire_damage)*(core_scale+core.fire)+core_scale/2)/core_scale;

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

	poison_damage = (poison_damage*(core_scale+core.poison)+core_scale/2)/core_scale;

	if(upgrades.lightning>=2)
	{
		lightning_damage *= 10;
		++shock_dur;
	}
	if(upgrades.lightning>=3)
	{
		lightning_damage *= 10;
		shock_damage_pml *= 2;
	}
	if(upgrades.lightning>=5)
	{
		lightning_damage *= 10;
		++shock_dur;
	}
	if(upgrades.lightning>=6)
	{
		lightning_damage *= 10;
		shock_damage_pml *= 2;
	}

	lightning_damage = (lightning_damage*(core_scale+core.lightning)+core_scale/2)/core_scale;
	shock_damage_pml = (shock_damage_pml*(core_scale+core.lightning)+core_scale/2)/core_scale;
	lightning_column_pml = (lightning_column_pml*(core_scale+core.lightning)+core_scale/2)/core_scale;

	strength_pml = (strength_pml*(core_scale+core.strength)+core_scale/2)/core_scale;
	condenser_pml = (condenser_pml*(core_scale+core.condenser)+core_scale/2)/core_scale;
}


CellInfo::CellInfo():
	trap(0),
	steps(0),
	shocked_steps(0),
	damage_taken(0),
	hp_left(0)
{ }


const char Layout::traps[] = "_FZPLSCK";

Layout::Layout():
	damage(0),
	cost(0),
	rs_per_sec(0),
	threat(0),
	cycle(0)
{ }

void Layout::set_upgrades(const TrapUpgrades &u)
{
	upgrades = u;
}

void Layout::set_core(const Core &c)
{
	core = c;
}

void Layout::set_traps(const string &t, unsigned floors)
{
	data = t;
	if(!floors)
		floors = (data.size()+4)/5;
	data.resize(floors*5, '_');
}

void Layout::build_steps(vector<Step> &steps) const
{
	unsigned cells = data.size();
	steps.clear();
	steps.reserve(cells*3);

	uint8_t column_flags[5] = { };
	for(unsigned i=0; i<cells; ++i)
		if(data[i]=='L')
			++column_flags[i%5];

	vector<uint16_t> floor_flags(cells/5, 0);
	for(unsigned i=0; i<cells; ++i)
	{
		unsigned j = i/5;
		char t = data[i];
		if(t=='F')
			floor_flags[j] += 1+0x10*column_flags[i%5];
		else if(t=='S')
			floor_flags[j] |= 0x08;
	}

	TrapEffects effects(upgrades, core);

	unsigned chilled = 0;
	unsigned frozen = 0;
	unsigned shocked = 0;
	unsigned damage_pml = 1000;
	unsigned special_multi = 1;
	unsigned repeat = 1;
	for(unsigned i=0; i<cells; )
	{
		char t = data[i];
		Step step;
		step.cell = i;
		step.trap = t;
		step.slow = (frozen ? 2 : chilled ? 1 : 0);
		step.shock = (shocked!=0);

		if(t=='Z')
		{
			step.direct_damage = effects.frost_damage*damage_pml/1000;
			chilled = effects.chill_dur*special_multi+1;
			frozen = 0;
			repeat = 1;
		}
		else if(t=='F')
		{
			step.direct_damage = effects.fire_damage*damage_pml/1000;
			if(floor_flags[i/5]&0x08)
				step.direct_damage = step.direct_damage*(1000+effects.strength_pml)/1000;
			if(chilled && upgrades.frost>=3)
				step.direct_damage = step.direct_damage*5/4;
			if(upgrades.lightning>=4)
				step.direct_damage = step.direct_damage*(1000+effects.lightning_column_pml*column_flags[i%5])/1000;
			if(upgrades.fire>=4)
				step.kill_pml = 200;
		}
		else if(t=='P')
		{
			step.toxicity = effects.poison_damage*damage_pml/1000;
			if(upgrades.frost>=4 && i+1<cells && data[i+1]=='Z')
				step.toxicity *= 4;
			if(upgrades.poison>=3)
			{
				if(i>0 && data[i-1]=='P')
					step.toxicity *= 3;
				if(i+1<cells && data[i+1]=='P')
					step.toxicity *= 3;
			}
			if(upgrades.lightning>=4)
				step.toxicity = step.toxicity*(1000+effects.lightning_column_pml*column_flags[i%5])/1000;
		}
		else if(t=='L')
		{
			step.direct_damage = effects.lightning_damage*damage_pml/1000;
			shocked = effects.shock_dur+1;
			damage_pml = effects.shock_damage_pml;
			special_multi = effects.special_multi;
		}
		else if(t=='S')
		{
			uint16_t flags = floor_flags[i/5];
			step.direct_damage = effects.fire_damage*(flags&0x07);
			if(upgrades.lightning>=4)
				step.direct_damage += effects.fire_damage*effects.lightning_column_pml*(flags>>4)/1000;
			step.direct_damage = step.direct_damage*2*damage_pml/1000;
			if(chilled && upgrades.frost>=3)
				step.direct_damage = step.direct_damage*5/4;
		}
		else if(t=='K')
		{
			if(chilled)
			{
				chilled = 0;
				frozen = 5*special_multi+1;
			}
			repeat = 1;
		}
		else if(t=='C')
			step.toxic_pml = effects.condenser_pml*special_multi;

		if(repeat>1 && upgrades.frost>=5)
			step.rs_bonus = 2;
		steps.push_back(step);

		if(shocked && !--shocked)
		{
			damage_pml = 1000;
			special_multi = 1;
		}

		if(repeat && --repeat)
			continue;

		++i;
		if(chilled)
			--chilled;
		if(frozen)
			--frozen;
		repeat = (frozen ? 3 : chilled ? 2 : 1);
	}
}

Layout::SimResult Layout::simulate(const vector<Step> &steps, Number hp, vector<SimDetail> *detail) const
{
	SimResult result;
	result.sim_hp = hp;
	result.max_hp = numeric_limits<Number>::max();

	if(detail)
	{
		detail->clear();
		detail->reserve(steps.size());
	}

	Number kill_damage = 0;
	Number toxicity = 0;
	unsigned rs_pct = 100;
	for(const auto &s: steps)
	{
		result.damage += s.direct_damage;
		kill_damage = max(kill_damage, result.damage*1000/(1000-s.kill_pml));
		if(upgrades.poison>=5 && hp && result.damage*4>=hp)
		{
			toxicity += s.toxicity*5;
			if(s.toxicity)
				result.max_hp = min(result.max_hp, result.damage*4);
		}
		else
			toxicity += s.toxicity;
		toxicity = toxicity*(1000+s.toxic_pml)/1000;
		result.damage += toxicity;
		rs_pct += s.rs_bonus;
		kill_damage = max(kill_damage, result.damage);

		if(detail)
		{
			SimDetail sd;
			sd.damage_taken = s.direct_damage+toxicity;
			sd.toxicity = toxicity;
			if(kill_damage>=hp)
				sd.hp_left = 0;
			else
				sd.hp_left = hp-result.damage;
			detail->push_back(sd);
		}

		if(result.kill_cell<0)
		{
			++result.steps_taken;
			if(kill_damage>=hp)
			{
				result.max_hp = min(result.max_hp, kill_damage);
				result.kill_cell = s.cell;
				result.runestone_pct = rs_pct;
				if(upgrades.fire>=7 && s.trap=='F')
					result.runestone_pct = result.runestone_pct*6/5;
			}
		}
	}

	result.toxicity = toxicity;
	if(kill_damage>=hp)
		result.damage = kill_damage;

	return result;
}

void Layout::build_results(const vector<Step> &steps, vector<SimResult> &results) const
{
	results.clear();
	Number hp = 1;
	for(unsigned i=0; i<10000; ++i)
	{
		SimResult res = simulate(steps, hp);
		results.push_back(res);
		hp = res.max_hp+1;
		if(hp<res.max_hp)
			return;
	}
}

template<typename F>
Number Layout::integrate_results(const vector<SimResult> &results, unsigned thrt, const F &func) const
{
	if(results.empty())
		return 0;

	unsigned range = min(max<int>(0.53*thrt/16, 150), 850);
	Number max_hp = 10+0.25*thrt+pow(1.012, thrt/16.0);
	Number min_hp = max_hp*(1000-range)/1000;

	for(auto i=results.begin(); (i!=results.end() && i->sim_hp<max_hp); ++i)
	{
		if(i->max_hp<min_hp)
			continue;

		Number low = max(min_hp, i->sim_hp);
		Number high = min(max_hp, i->max_hp);
		func(*i, low, high);
	}

	return max_hp+1-min_hp;
}

void Layout::update(UpdateMode mode)
{
	update_cost();
	if(mode==COST_ONLY)
		return;

	vector<Step> steps;
	build_steps(steps);
	vector<SimResult> results;
	if(mode>FAST)
		build_results(steps, results);

	if(mode==FAST || mode==COMPATIBLE)
		update_damage(steps, 10);
	if(mode==EXACT_DAMAGE || mode==FULL)
		update_damage(results);
	if(mode==COMPATIBLE || mode==FULL)
	{
		update_threat(results);
		update_runestones(results);
	}
}

void Layout::update_damage(const vector<Step> &steps, unsigned accuracy)
{
	damage = simulate(steps, 0).damage;
	if(upgrades.poison>=5)
	{
		Number high = simulate(steps, damage).damage;
		for(unsigned i=0; (i<accuracy && damage+1<high); ++i)
		{
			Number mid = (damage+high)/2;
			SimResult res = simulate(steps, mid);
			if(res.kill_cell>=0)
				damage = res.max_hp;
			else
				high = mid;
		}
	}
}

void Layout::update_damage(const vector<SimResult> &results)
{
	if(results.empty() || results.front().kill_cell<0)
	{
		damage = 0;
		return;
	}

	unsigned low = 0;
	unsigned high = results.size();
	while(low+1<high)
	{
		unsigned mid = (low+high)/2;
		if(results[mid].kill_cell>=0)
			low = mid;
		else
			high = mid;
	}

	damage = results[low].max_hp;
}

void Layout::update_cost()
{
	Number fire_cost = 100;
	Number frost_cost = 100;
	Number poison_cost = 500;
	Number lightning_cost = 1000;
	Number strength_cost = 3000;
	Number condenser_cost = 6000;
	Number knowledge_cost = 9000;
	cost = 0;
	for(char t: data)
	{
		Number prev_cost = cost;
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
			cost = numeric_limits<Number>::max();
			break;
		}
	}
}

void Layout::update_threat(const vector<SimResult> &results)
{
	if(!damage)
	{
		threat = 16;
		return;
	}

	unsigned cells = data.size();
	unsigned floors = cells/5;

	static double log_base = log(1.012);
	unsigned low = log(damage-4*log(damage)/log_base)/log_base*16;
	unsigned high = low+1024;

	while(low+1<high)
	{
		threat = (low+high+1)/2;

		static Number bias = numeric_limits<Number>::max()/2;
		Number change = bias;
		integrate_results(results, threat, [&change, cells, floors](const SimResult &r, Number low_hp, Number high_hp)
		{
			if(r.kill_cell>=0)
				change += (cells-r.kill_cell+4)/5*(high_hp+1-low_hp);
			else
			{
				Number n = (low_hp*115-r.damage*100-1)/(low_hp*15);
				while(low_hp<high_hp)
				{
					Number step_hp = (r.damage*100-1)/(100-n*15);
					change -= floors*n*(min(step_hp, high_hp)+1-low_hp);
					++n;
					low_hp = step_hp+1;
				}
			}
		});

		if(change>bias)
			low = threat;
		else
			high = threat;
	}
}

void Layout::update_runestones(const vector<SimResult> &results)
{
	unsigned capacity = (1+(data.size()+1)/2)*48;
	WeightedAccumulator runestones;
	Number steps_taken = 0;
	double threat_multi = pow(1.00116, threat/16.0);

	Number hp_range = integrate_results(results, threat, [this, &runestones, &steps_taken, threat_multi](const SimResult &r, Number low_hp, Number high_hp)
	{
		if(r.damage>=r.sim_hp)
		{
			double multi = threat_multi*r.runestone_pct/100;
			Number low_step = (low_hp+599)/600;
			Number high_step = (high_hp+599)/600;
			unsigned threat_term = threat/320;
			runestones.add((low_step+threat_term)*multi, low_step*600-low_hp);
			runestones.add((high_step+threat_term)*multi, high_hp+1-(high_step-1)*600);
			if(high_step>low_step)
				runestones.add(((low_step+high_step-1)/2+threat_term)*multi, (high_step-1-low_step)*600);
		}
		else if(upgrades.poison>=6)
			runestones.add(r.toxicity/10, high_hp+1-low_hp);
		else
			runestones.add(0, high_hp+1-low_hp);
		steps_taken += r.steps_taken*16*(high_hp+1-low_hp);
	});

	if(hp_range)
	{
		steps_taken = max<Number>(steps_taken/hp_range, capacity);
		rs_per_sec = runestones.result()*capacity/steps_taken/3;
	}
	else
		rs_per_sec = 0;
}

void Layout::cross_from(const Layout &other, Random &random)
{
	unsigned cells = min(data.size(), other.data.size());
	for(unsigned i=0; i<cells; ++i)
		if(random()&1)
			data[i] = other.data[i];
}

void Layout::mutate(MutateMode mode, unsigned count, Random &random, unsigned cyc)
{
	unsigned cells = data.size();
	unsigned locality = (cells>=10 ? random()%(cells*2/15) : 0);
	unsigned base = 0;
	if(locality)
	{
		cells -= locality*5;
		base = (random()%locality)*5;
	}

	unsigned traps_count = (upgrades.lightning>0 ? 7 : 5);
	for(unsigned i=0; i<count; ++i)
	{
		unsigned op = 0;  // REPLACE_ONLY
		if(mode==PERMUTE_ONLY)
			op = 1+random()%4;
		else if(mode==ALL_MUTATIONS)
			op = random()%8;

		unsigned t = 1+random()%traps_count;
		if(!upgrades.lightning && t>=4)
			++t;
		char trap = traps[t];

		if(op==0)  // replace
			data[base+random()%cells] = trap;
		else if(op==1 || op==2 || op==5)  // swap, rotate, insert
		{
			unsigned pos = base+random()%cells;
			unsigned end = base+random()%(cells-1);
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
		else if(cells>=10)  // floor operations
		{
			unsigned floors = cells/5;
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

	cycle = cyc;
}

bool Layout::is_valid() const
{
	unsigned cells = data.size();
	bool have_strength = false;
	for(unsigned i=0; i<cells; ++i)
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

void Layout::debug(Number hp) const
{
	vector<Step> steps;
	build_steps(steps);
	vector<SimDetail> detail;
	SimResult result = simulate(steps, hp, &detail);

	cout << "Enemy HP: " << hp << endl;

	unsigned last_cell = 0;
	unsigned repeat = 0;
	Number total_damage = 0;
	for(unsigned i=0; i<steps.size(); ++i)
	{
		const Step &s = steps[i];
		const SimDetail &d = detail[i];

		total_damage += d.damage_taken;
		repeat = (s.cell==last_cell ? repeat+1 : 0);

		cout << setw(2) << s.cell << ':' << repeat << ": " << s.trap << ' ' << setw(9) << total_damage;
		if(upgrades.poison>=5)
		{
			if(d.hp_left<hp)
				cout << ' ' << setw(2) << d.hp_left*100/hp << '%';
			else
				cout << " **%";
		}
		if(d.toxicity)
			cout << " P" << setw(6) << d.toxicity;
		else
			cout << "        ";
		cout << ' ' << (s.slow==1 ? 'C' : ' ') << (s.slow==2 ? 'F' : ' ') << (s.shock ? 'S' : ' ') << endl;

		last_cell = s.cell;
	}

	cout << "Total damage: " << result.damage << endl;
}

void Layout::build_cell_info(vector<CellInfo> &info, Number hp) const
{
	vector<Step> steps;
	build_steps(steps);
	vector<SimDetail> detail;
	simulate(steps, hp, &detail);

	info.clear();
	info.resize(data.size());
	for(unsigned i=0; i<steps.size(); ++i)
	{
		const Step &s = steps[i];
		const SimDetail &d = detail[i];

		info[s.cell].trap = s.trap;
		++info[s.cell].steps;
		if(s.shock)
			++info[s.cell].shocked_steps;
		info[s.cell].damage_taken += d.damage_taken;
		info[s.cell].hp_left = d.hp_left;
	}
}


Layout::Step::Step():
	trap(0),
	slow(0),
	shock(false),
	rs_bonus(0),
	kill_pml(0),
	toxic_pml(0),
	direct_damage(0),
	toxicity(0)
{ }


Layout::SimResult::SimResult():
	max_hp(0),
	damage(0),
	runestone_pct(100),
	steps_taken(0),
	kill_cell(-1)
{ }
