#ifndef FIXEDPOINT_H_
#define FIXEDPOINT_H_

#include <iostream>
#include "types.h"

template<unsigned SCALE, typename T = Number>
struct Fixed
{
	T value;

	Fixed(T n): value(n*SCALE) { }
	Fixed(int n): value(n*SCALE) { }
	Fixed(double n): value(n*SCALE+0.5) { }

	template<typename U>
	explicit Fixed(Fixed<SCALE, U> f): value(f.value) { }

	static Fixed from_raw(T n) { Fixed f(T(0)); f.value = n; return f; }

	Fixed &operator+=(Fixed f) { value += f.value; return *this; }
	Fixed &operator+=(T n) { value += n*SCALE; return *this; }
	Fixed &operator-=(Fixed f) { value -= f.value; return *this; }
	Fixed &operator-=(T n) { value -= n*SCALE; return *this; }
	Fixed &operator*=(T n) { value *= n; return *this; }
	Fixed &operator/=(T n) { value /= n; return *this; }

	T floor() const { return value/SCALE; }
	T round() const { return (value+SCALE/2)/SCALE; }
	double to_real() const { return static_cast<double>(value)/SCALE; }

	template<unsigned TO_SCALE>
	Fixed<TO_SCALE, T> rescale() const { return Fixed<TO_SCALE, T>::from_raw((value*TO_SCALE+SCALE/2)/SCALE); }
};

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator+(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return Fixed<SCALE, T>::from_raw(f1.value+f2.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator-(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return Fixed<SCALE, T>::from_raw(f1.value-f2.value); }

template<unsigned SCALE1, unsigned SCALE2, typename T>
inline Fixed<SCALE1*SCALE2, T> operator*(Fixed<SCALE1, T> f1, Fixed<SCALE2, T> f2)
{ return Fixed<SCALE1*SCALE2, T>::from_raw(f1.value*f2.value); }

template<unsigned SCALE1, unsigned SCALE2, typename T>
inline Fixed<SCALE1/SCALE2, T> operator/(Fixed<SCALE1, T> f1, Fixed<SCALE2, T> f2)
{
	static_assert(SCALE1%SCALE2==0, "Division of incompatible fixed-point numbers");
	return Fixed<SCALE1/SCALE2, T>::from_raw(f1.value/f2.value);
}

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator+(Fixed<SCALE, T> f, T n)
{ return Fixed<SCALE, T>::from_raw(f.value+n*SCALE); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator+(T n, Fixed<SCALE, T> f)
{ return Fixed<SCALE, T>::from_raw(n*SCALE+f.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator-(Fixed<SCALE, T> f, T n)
{ return Fixed<SCALE, T>::from_raw(f.value-n*SCALE); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator-(T n, Fixed<SCALE, T> f)
{ return Fixed<SCALE, T>::from_raw(n*SCALE-f.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator*(Fixed<SCALE, T> f, T n)
{ return Fixed<SCALE, T>::from_raw(f.value*n); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator*(T n, Fixed<SCALE, T> f)
{ return Fixed<SCALE, T>::from_raw(n*f.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator/(Fixed<SCALE, T> f, T n)
{ return Fixed<SCALE, T>::from_raw(f.value/n); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator+(Fixed<SCALE, T> f, int n)
{ return Fixed<SCALE, T>::from_raw(f.value+n*SCALE); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator+(int n, Fixed<SCALE, T> f)
{ return Fixed<SCALE, T>::from_raw(n*SCALE+f.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator-(Fixed<SCALE, T> f, int n)
{ return Fixed<SCALE, T>::from_raw(f.value-n*SCALE); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator-(int n, Fixed<SCALE, T> f)
{ return Fixed<SCALE, T>::from_raw(n*SCALE-f.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator*(Fixed<SCALE, T> f, int n)
{ return Fixed<SCALE, T>::from_raw(f.value*n); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator*(int n, Fixed<SCALE, T> f)
{ return Fixed<SCALE, T>::from_raw(n*f.value); }

template<unsigned SCALE, typename T>
inline Fixed<SCALE, T> operator/(Fixed<SCALE, T> f, int n)
{ return Fixed<SCALE, T>::from_raw(f.value/n); }

template<unsigned SCALE, typename T>
inline bool operator==(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return f1.value==f2.value; }

template<unsigned SCALE, typename T>
inline bool operator!=(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return f1.value!=f2.value; }

template<unsigned SCALE, typename T>
inline bool operator<(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return f1.value<f2.value; }

template<unsigned SCALE, typename T>
inline bool operator<=(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return f1.value<=f2.value; }

template<unsigned SCALE, typename T>
inline bool operator>(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return f1.value>f2.value; }

template<unsigned SCALE, typename T>
inline bool operator>=(Fixed<SCALE, T> f1, Fixed<SCALE, T> f2)
{ return f1.value>=f2.value; }

template<unsigned SCALE, typename T>
inline std::ostream &operator<<(std::ostream &os, Fixed<SCALE, T> f)
{ os << f.round(); return os; }

#endif
