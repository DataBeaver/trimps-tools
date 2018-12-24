#include "stringutils.h"

using namespace std;

vector<string> split(const string &str)
{
	vector<string> parts;
	string::size_type start = 0;

	while(start<str.size())
	{
		string::size_type space = str.find(' ', start+1);
		parts.push_back(str.substr(start, space-start));
		if(space==string::npos)
			break;
		start = space+1;
	}

	return parts;
}
