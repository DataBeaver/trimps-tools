#ifndef STRINGUTILS_H_
#define STRINGUTILS_H_

#include <stdexcept>
#include <string>
#include <sstream>

template<typename T>
inline std::string stringify(const T &value)
{
	std::ostringstream ss;
	ss << value;
	return ss.str();
}

template<typename T>
inline T parse_string(const std::string &str)
{
	std::istringstream ss(str);
	T value;
	ss >> value;
	if(ss.fail() || !ss.eof())
		throw std::logic_error("bad conversion");
	return value;
}

inline std::string format(const std::string &fmt)
{
	return fmt;
}

template<typename T, typename... Args>
inline std::string format(const std::string &fmt, const T &arg, Args... args)
{
	std::string::size_type marker = fmt.find('%');
	if(marker==std::string::npos)
		return fmt;
	std::string::size_type marker_end = marker+1;
	while(marker_end<fmt.size() && !isalpha(fmt[marker_end]))
		++marker_end;
	return fmt.substr(0, marker)+stringify(arg)+format(fmt.substr(marker_end+1), args...);
}

#endif
