#ifndef SPIREPOOL_H_
#define SPIREPOOL_H_

#include <list>
#include <mutex>
#include "types.h"

class Layout;

class Pool 
{
public:
	typedef Number ScoreFunc(const Layout &);

private:
	unsigned max_size;
	ScoreFunc *score_func;
	std::list<Layout> layouts;
	mutable std::mutex layouts_mutex;

public:
	Pool(unsigned, ScoreFunc *);

	void add_layout(const Layout &);
	Layout get_best_layout() const;
	bool get_best_layout(Layout &) const;
	Layout get_random_layout(Random &) const;
	Number get_best_score() const;

	template<typename F>
	void visit_layouts(const F &) const;
};

template<typename F>
void Pool::visit_layouts(const F &func) const
{
	std::lock_guard<std::mutex> lock(layouts_mutex);
	for(const auto &layout: layouts)
		if(!func(layout))
			return;
}

#endif
