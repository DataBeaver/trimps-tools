#include "types.h"
#include "stringutils.h"

using namespace std;

namespace {

const char suffixes[][3] = { "", "k", "M", "B", "T", "Qa", "Qi", "Sx", "Sp", "Oc", "No" };
const char suffixes2[][4] = { "", "Dc", "V", "Tg", "Qaa", "Qia" };
const char suffixes3[][3] = { "", "U", "D", "T", "Qa", "Qi", "Sx", "Sp", "O", "N" };
const char suffixes4[][3] = { "", "d", "v", "tg", "qa", "qi" };

}

#if defined(WITH_128BIT) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
ostream &operator<<(ostream &os, unsigned __int128 value)
{
	char buf[40];
	unsigned i = 39;
	buf[i] = 0;
	while(value || i==39)
	{
		buf[--i] = '0'+value%10;
		value /= 10;
	}

	os << (buf+i);

	return os;
}

istream &operator>>(istream &is, unsigned __int128 &value)
{
	string word;
	is >> word;
	unsigned __int128 v = 0;
	for(auto c: word)
	{
		if(c<'0' || c>'9')
		{
			is.setstate(ios_base::failbit);
			return is;
		}
		v = v*10+(c-'0');
	}

	value = v;

	return is;
}
#pragma GCC diagnostic pop
#endif

template<typename T>
ostream &operator<<(ostream &os, const NumericIO<T> &num)
{
	double value = num.value;
	if(value<10000)
		return (os << value);

	unsigned i = 0;
	for(; (i<59 && value>=1000); ++i)
		value /= 1000;

	ios::fmtflags flags = os.flags();
	streamsize precision = os.precision();
	os.unsetf(ios_base::floatfield);
	os.precision(3);
	os << value;
	if(i<=10)
		os << suffixes[i];
	else
	{
		--i;
		if(i%10==0)
			os << suffixes2[i/10];
		else
			os << suffixes3[i%10] << suffixes4[i/10];
	}
	os.flags(flags);
	os.precision(precision);

	return os;
}

template ostream &operator<<(ostream &, const NumericIO<Number> &);
template ostream &operator<<(ostream &, const NumericIO<double> &);

template<typename T>
istream &operator>>(istream &is, NumericIO<T> &num)
{
	string word;
	is >> word;

	string::size_type letter = 0;
	for(; (letter<word.size() && !isalpha(word[letter])); ++letter) ;

	if(letter>=word.size())
		num.value = parse_value<T>(word);
	else if(word[letter]=='e')
		num.value = parse_value<double>(word);
	else
	{
		double value = parse_value<double>(word.substr(0, letter));
		bool found = false;
		for(unsigned i=1; (!found && i<11); ++i)
		{
			value *= 1000;
			found = !word.compare(letter, string::npos, suffixes[i]);
		}

		if(found)
			num.value = value;
		else
			is.setstate(ios_base::failbit);
	}

	return is;
}

template istream &operator>>(istream &, NumericIO<Number> &);
template istream &operator>>(istream &, NumericIO<double> &);
