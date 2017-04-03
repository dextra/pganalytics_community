#ifndef __DB_DB_EXCEPTIONS_H__
#define __DB_DB_EXCEPTIONS_H__

#include <exception>
#include <string>
#include "config.h"

BEGIN_APP_NAMESPACE

namespace Db
{

class Exception : public std::exception
{
protected:
	std::string _what;
	std::string _message;
public:
	Exception(const std::string &_what = "Database exception", const std::string &_message = "")
		: _what(_what + ": " + _message), _message(_message)
	{}
	virtual ~Exception() throw() {}
	virtual const char *what() const throw()
	{
		return _what.c_str();
	}
	virtual const std::string &message() const
	{
		return _message;
	}
};

class AbstractField;

class AbstractFieldException
{
protected:
	AbstractField *field;
public:
	//virtual const char *what() const throw() = 0;
	AbstractFieldException(AbstractField *field = 0)
		: field(field)
	{}
	void setField(AbstractField *field)
	{
		this->field = field;
	}
	AbstractField *getField()
	{
		return this->field;
	}
};

class BadValueCastException : public Exception, public AbstractFieldException
{
protected:
	std::string value;
public:
	BadValueCastException(AbstractField *field = 0, const std::string &value = "")
		: Exception("BadValueCastException"), AbstractFieldException(field), value(value)
	{}
	virtual ~BadValueCastException() throw() {}
	void setValue(const std::string &value)
	{
		this->value = value;
	}
	const std::string &getValue() const
	{
		return this->value;
	}
};

class NullValueFetchException : public Exception, public AbstractFieldException
{
public:
	NullValueFetchException()
		: Exception("NullValueFetchException"), AbstractFieldException()
	{}
	virtual ~NullValueFetchException() throw() {}
};

class IncompatibleTypesExecption : public Exception
{
public:
	IncompatibleTypesExecption(const std::string &message = "")
		: Exception("IncompatibleTypesExecption", message)
	{}
	virtual ~IncompatibleTypesExecption() throw() {}
};

class NotImplementedException : public Exception
{
public:
	NotImplementedException(const std::string &message = "")
		: Exception("NotImplementedException", message)
	{}
	virtual ~NotImplementedException() throw() {}
};

class ConnectionException : public Exception
{
public:
	ConnectionException(const std::string &message = "")
		: Exception("ConnectionException", message)
	{}
	virtual ~ConnectionException() throw() {}
};

} // namespace Db

END_APP_NAMESPACE

#endif // __DB_DB_EXCEPTIONS_H__

