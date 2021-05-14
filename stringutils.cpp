#include <algorithm>
#include <cctype>
#include "stringutils.h"

using namespace std;

vector<string> split(const string &str, char sep)
{
	vector<string> parts;
	string::size_type start = 0;

	while(start<str.size())
	{
		string::size_type space = str.find(sep, start);
		parts.push_back(str.substr(start, space-start));
		if(space==string::npos)
			break;
		start = space+1;
	}

	return parts;
}

string remove_spaces(const string &input)
{
	string result = input;
	result.erase(remove_if(result.begin(), result.end(), [](unsigned char c){ return isspace(c); }), result.end());
	return result;
}
