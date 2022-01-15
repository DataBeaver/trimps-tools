#include <cctype>
#include "http.h"

using namespace std;

HttpMessage::HttpMessage(unsigned r):
	response(r)
{ }

HttpMessage::HttpMessage(const string &str):
	response(0)
{
	string::size_type start = 0;

	while(start<str.size())
	{
		string::size_type newline = str.find('\n', start);
		if(newline==string::npos)
			break;

		bool has_cr = (str[newline-1]=='\r');
		string line = str.substr(start, newline-start-has_cr);

		if(!response && method.empty())
		{
			vector<string> parts = split(line);
			if(parts.size()!=3)
				throw invalid_argument("HttpMessage::HttpMessage");

			if(!parts[0].compare(0, 5, "HTTP/"))
				response = parse_value<unsigned>(parts[1]);
			else
			{
				method = parts[0];
				path = parts[1];
			}
		}
		else if(line.empty())
		{
			body = str.substr(newline+1);
			break;
		}
		else
		{
			string::size_type colon = str.find(':', start);
			string::size_type value_start = colon+1;
			while(value_start<newline && isspace(str[value_start]))
				++value_start;

			if(value_start>=newline)
				throw invalid_argument("HttpMessage::HttpMessage");

			string name = str.substr(start, colon-start);
			bool upper = true;
			for(auto &c: name)
			{
				if(isalpha(c))
					c = (upper ? toupper(c) : tolower(c));
				upper = (c=='-');
			}
			add_header(name, str.substr(value_start, newline-value_start-has_cr));
		}

		start = newline+1;
	}
}

void HttpMessage::add_header(const string &name, const string &value)
{
	headers.insert(make_pair(name, value));
}

string HttpMessage::str() const
{
	string result;
	if(response)
		result = format("HTTP/1.1 %s\r\n", response);
	else
		result = format("%s %s HTTP/1.1\r\n", method, path);

	for(const auto &h: headers)
		result += format("%s: %s\r\n", h.first, h.second);

	result += "\r\n";
	result += body;

	return result;
}
