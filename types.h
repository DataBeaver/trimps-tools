#ifndef TYPES_H_
#define TYPES_H_

#include <cstdint>
#include <istream>
#include <ostream>
#include <random>

#ifdef WITH_128BIT
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
typedef unsigned __int128 Number;
// std::numeric_limits<unsigned __int128>::max() returns zero in some environments, so use
// -1 cast to Number instead for the maximum representable unsigned 128-bit number.
constexpr Number number_max = Number(-1);
#pragma GCC diagnostic pop
#else
#error "128-bit integers are not available with this compiler"
#endif
#else
typedef std::uint64_t Number;
constexpr Number number_max = std::numeric_limits<Number>::max();
#endif

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

#if defined(WITH_128BIT) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
std::ostream &operator<<(std::ostream &, unsigned __int128);
std::istream &operator>>(std::istream &, unsigned __int128 &);
#pragma GCC diagnostic pop
#endif

template<typename T>
std::ostream &operator<<(std::ostream &, const NumericIO<T> &);

template<typename T>
std::istream &operator>>(std::istream &, NumericIO<T> &);

typedef std::minstd_rand Random;

#endif
