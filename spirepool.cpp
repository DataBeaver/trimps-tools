#include "spirepool.h"
#include "spirelayout.h"

using namespace std;

Pool::Pool(unsigned s, ScoreFunc *f):
	max_size(s),
	score_func(f)
{ }

void Pool::clear()
{
	lock_guard<mutex> lock(layouts_mutex);
	layouts.clear();
}

void Pool::add_layout(const Layout &layout)
{
	lock_guard<mutex> lock(layouts_mutex);

	if(layouts.size()>=max_size && score_func(layout)<score_func(layouts.back()))
		return;

	Number score = score_func(layout);
	auto i = layouts.begin();
	for(; (i!=layouts.end() && score_func(*i)>score); ++i)
		if(i->get_cost()<=layout.get_cost())
			return;
	if(i!=layouts.end() && score_func(*i)==score)
	{
		*i = layout;
		++i;
	}
	else
		layouts.insert(i, layout);

	while(i!=layouts.end())
	{
		if(i->get_cost()>=layout.get_cost())
			i = layouts.erase(i);
		else
			++i;
	}

	if(layouts.size()>max_size)
		layouts.pop_back();
}

Layout Pool::get_best_layout() const
{
	lock_guard<mutex> lock(layouts_mutex);
	return layouts.front();
}

bool Pool::get_best_layout(Layout &layout) const
{
	lock_guard<mutex> lock(layouts_mutex);
	if(score_func(layout)>=score_func(layouts.front()))
		return false;
	layout = layouts.front();
	return true;
}

Layout Pool::get_random_layout(Random &random) const
{
	lock_guard<mutex> lock(layouts_mutex);

	Number total = 0;
	for(const auto &l: layouts)
		total += score_func(l);

	if(!total)
	{
		auto i = layouts.begin();
		advance(i, random()%layouts.size());
		return *i;
	}

	Number p = ((static_cast<Number>(random())<<32)+random())%total;
	for(const auto &l: layouts)
	{
		Number score = score_func(l);
		if(p<score)
			return l;
		p -= score;
	}

	throw logic_error("Spire::get_random_layout");
}

Number Pool::get_best_score() const
{
	lock_guard<mutex> lock(layouts_mutex);
	return score_func(layouts.front());
}
