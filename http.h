#ifndef HTTP_H_
#define HTTP_H_

#include <map>
#include <string>
#include "stringutils.h"

struct HttpMessage
{
	unsigned response;
	std::string method;
	std::string path;
	std::multimap<std::string, std::string> headers;
	std::string body;

	HttpMessage(unsigned);
	HttpMessage(const std::string &);

	void add_header(const std::string &, const std::string &);
	template<typename T>
	void add_header(const std::string &n, const T &v)
	{ add_header(n, stringify(v)); }

	std::string str() const;
};

#endif
