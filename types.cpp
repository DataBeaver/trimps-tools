#include "types.h"
#include "stringutils.h"

using namespace std;

namespace {

const char suffixes[][3] = { "", "k", "M", "B", "T", "Qa", "Qi" };
constexpr unsigned n_suffixes = sizeof(suffixes)/sizeof(suffixes[0]);

}

ostream &operator<<(ostream &os, const NumberIO &num)
{
	double value = num.value;
	if(value<10000)
		return (os << value);

	unsigned i = 0;
	for(; (i+1<n_suffixes && value>=1000); ++i)
		value /= 1000;

	ios::fmtflags flags = os.flags();
	streamsize precision = os.precision();
	os.unsetf(ios_base::floatfield);
	os.precision(3);
	os << value << suffixes[i];
	os.flags(flags);
	os.precision(precision);

	return os;
}

istream &operator>>(istream &is, NumberIO &num)
{
	string word;
	is >> word;

	string::size_type letter = 0;
	for(; (letter<word.size() && !isalpha(word[letter])); ++letter) ;

	if(letter>=word.size())
		num.value = parse_value<Number>(word);
	else if(word[letter]=='e')
		num.value = parse_value<double>(word);
	else
	{
		double value = parse_value<double>(word.substr(0, letter));
		bool found = false;
		for(unsigned i=1; (!found && i<7); ++i)
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
