#ifndef __LINE_HANDLER_H__
#define __LINE_HANDLER_H__

#include <string>
#include <ostream>

class LineHandler
{
public:
	virtual ~LineHandler() {}
	virtual void handleLine(const std::string &line) = 0;
};

class StreamLineHandler : public LineHandler
{
protected:
	std::ostream &ostr;
public:
	StreamLineHandler(std::ostream &ostr)
		: ostr(ostr)
	{}
	virtual ~StreamLineHandler() {}
	inline void handleLine(const std::string &line)
	{
		ostr << line << "\n";
	}
};

#endif /* #ifndef __LINE_HANDLER_H__ */

