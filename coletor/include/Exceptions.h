#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include "config.h"
#include <stdexcept>
#include <ostringstream>

BEGIN_APP_NAMESPACE

class Exception : public std::runtime_error
{
public:
	virtual const char *what() const throw() = 0;
	virtual ~Exception() throw() {}
};

template<typename TChild>
class StreamedException : public Exception
{
protected:
	std::string m_message;
public:
	virtual ~StreamedException() throw() {}
	template<typename TData>
	TChild &operator<<(const TData &data)
	{
		std::ostringstream ss_tmp;
		ss_tmp << data;
		this->m_message.append(ss_tmp.str());
		return *(static_cast<TChild *>(this));
	}
	const char *what() const throw()
	{
		return this->m_message.c_str();
	}
};

class RuntimeException : public StreamedException<RuntimeException> {};

END_APP_NAMESPACE

#define THROW_EXCEPTION(cls, messages...) throw cls() << "[" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "] " << messages;

#endif // __EXCEPTIONS_H__

