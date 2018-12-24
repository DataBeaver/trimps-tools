#ifndef TYPES_H_
#define TYPES_H_

#include <cstdint>
#include <istream>
#include <ostream>
#include <random>

typedef std::uint64_t Number;

struct NumberIO
{
	Number value;

	NumberIO() { }
	NumberIO(Number v): value(v) { }

	operator Number() const { return value; }
};

std::ostream &operator<<(std::ostream &, const NumberIO &);
std::istream &operator>>(std::istream &, NumberIO &);

typedef std::minstd_rand Random;

#endif
