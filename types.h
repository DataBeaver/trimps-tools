#ifndef TYPES_H_
#define TYPES_H_

#include <cstdint>
#include <istream>
#include <ostream>
#include <random>

typedef std::uint64_t Number;

template<typename T>
struct NumericIO
{
	T value;

	NumericIO() { }
	NumericIO(T v): value(v) { }

	operator T() const { return value; }
};

typedef NumericIO<Number> NumberIO;
typedef NumericIO<double> DoubleIO;

template<typename T>
std::ostream &operator<<(std::ostream &, const NumericIO<T> &);

template<typename T>
std::istream &operator>>(std::istream &, NumericIO<T> &);

typedef std::minstd_rand Random;

#endif
