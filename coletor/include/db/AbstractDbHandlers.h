#ifndef __DB_ABSTRACT_DB_HANDLER_H__
#define __DB_ABSTRACT_DB_HANDLER_H__

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <string>
#include <cstring>
#include <vector>
#include <errno.h>
#include <iostream>
#include "db/DbExceptions.h"
#include "SmartPtr.h"
#include "config.h"

/* TODO: Cleanup the code and remove unused/unnecessary code */

BEGIN_APP_NAMESPACE

namespace Db
{

enum TransactionCharacteristics
{
	TRANS_DEFAULT = 0,
	TRANS_ISOLATION_READ_UNCOMMITED = 1,
	TRANS_ISOLATION_READ_COMMITED = 2,
	TRANS_ISOLATION_REPEATABLE_READ = 4,
	TRANS_ISOLATION_SERIALIZABLE = 8,
	TRANS_READ_WRITE = 16,
	TRANS_READ_ONLY = 32,
	TRANS_DEFERRABLE = 64,
	TRANS_NOT_DEFERRABLE = 128
};

namespace Internal
{

class ValueFetcher
{
public:
	virtual ~ValueFetcher() {}
	virtual bool isNull(size_t col) = 0;
	virtual void fetch(size_t col, int &toVar) = 0;
	virtual void fetch(size_t col, unsigned int &toVar) = 0;
	virtual void fetch(size_t col, long long int &toVar) = 0;
	virtual void fetch(size_t col, unsigned long long int &toVar) = 0;
	virtual void fetch(size_t col, double &toVar) = 0;
	virtual void fetch(size_t col, float &toVar) = 0;
	virtual void fetch(size_t col, char &toVar) = 0;
	virtual void fetch(size_t col, bool &toVar) = 0;
	virtual void fetch(size_t col, std::string &toVar) = 0;
	virtual void fetch(size_t col, std::tm &toVar) = 0;
	void fetch(size_t col, std::pair<std::istream*,std::ostream*> &toVar)
	{
		throw NotImplementedException();
	}
};

class ResultHandler : public ValueFetcher, public SmartObject
{
public:
	virtual ~ResultHandler() {}
	virtual bool next() = 0;
	virtual size_t ncols() = 0;
	virtual size_t nrows() = 0;
	virtual std::string colName(size_t field_number) = 0;
	virtual size_t colNumber(const std::string &field_name) = 0;
	template<typename T>
	void fetchByName(const std::string &colName, T &toVar)
	{
		fetch(colNumber(colName), toVar);
	}
};

class ValueBinder
{
public:
	virtual ~ValueBinder() {}
	virtual void bindNull(size_t col) = 0;
	virtual void bind(size_t col, const int &toVar) = 0;
	virtual void bind(size_t col, const unsigned int &toVar) = 0;
	virtual void bind(size_t col, const long long int &toVar) = 0;
	virtual void bind(size_t col, const unsigned long long int &toVar) = 0;
	virtual void bind(size_t col, const double &toVar) = 0;
	virtual void bind(size_t col, const float &toVar) = 0;
	virtual void bind(size_t col, const char &toVar) = 0;
	virtual void bind(size_t col, const bool &toVar) = 0;
	virtual void bind(size_t col, const std::string &toVar) = 0;
	virtual void bind(size_t col, const std::tm &toVar) = 0;
	void bind(size_t col, std::pair<std::istream*,std::ostream*> &toVar)
	{
		throw NotImplementedException();
	}
};

class CopyOutputStreamHandler : public SmartObject
{
public:
	virtual ~CopyOutputStreamHandler() {}
	virtual bool next() = 0;
	virtual std::string get() const = 0;
	/**
	 * We provide a default implementation toStream and toFile,
	 * but the underline library may provide a more efficient one.
	 */
	virtual void toStream(std::ostream &ostr)
	{
		while (this->next())
		{
			ostr << this->get();
		}
	}
	virtual void toFile(FILE *f)
	{
		while (this->next())
		{
			std::string s = this->get();
			const char *buf = s.c_str();
			if (fwrite(buf, sizeof(char) * s.size(), 1, f) != 1)
			{
				throw Exception(std::string("fwrite failed: ") + std::string(strerror(errno)));
			}
		}
	}
};

class CopyInputStreamHandler : public SmartObject
{
public:
	virtual ~CopyInputStreamHandler() {}
	virtual void put(const std::string &str) = 0;
	virtual void putLine(const std::string &str) = 0;
	virtual void end() = 0;
	virtual void abort() = 0;
	/**
	 * We provide a default implementation fromStream and fromFile,
	 * but the underline library may provide a more efficient one.
	 */
	virtual void fromStream(std::istream &istr)
	{
		std::string tmp;
		while (std::getline(istr, tmp))
		{
			this->putLine(tmp);
		}
	}
	virtual void fromFile(FILE *f)
	{
		char buf[1024];
		while(fread(buf, sizeof(buf), 1, f) == 1)
		{
			this->putLine(buf);
		}
		/* Check if error or EOF */
		if (ferror(f) != 0)
		{
			throw Exception(std::string("fread failed: ") + std::string(strerror(errno)));
		}
	}
};

class StatementHandler : public ValueBinder, public SmartObject
{
public:
	virtual ~StatementHandler() {}
	virtual void execUpdate() = 0;
	virtual ResultHandler *execQuery() = 0;
	virtual unsigned long long int sequenceLastId(const std::string &seqName) = 0;
};

class ConnectionHandler : public SmartObject
{
public:
	virtual ~ConnectionHandler() {}
	virtual StatementHandler *prepare(const std::string &sql) = 0;
	virtual void begin(int characteristics = TRANS_DEFAULT) = 0;
	virtual void commit() = 0;
	virtual void rollback() = 0;
	virtual void releaseAll() = 0;
	virtual ResultHandler *execQuery(const std::string &stmt) = 0;
	virtual void execCommand(const std::string &stmt) = 0;
	virtual CopyOutputStreamHandler *openCopyOutputStream(const std::string &cmd) = 0;
	virtual CopyInputStreamHandler *openCopyInputStream(const std::string &cmd) = 0;
	virtual std::string quoteString(const std::string &original) const = 0;
	virtual std::string quoteIdentifier(const std::string &original) const = 0;
	virtual std::string copyValue(const std::string &original, const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const = 0;
	virtual std::string copyNullValue(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const = 0;
	virtual std::string copyFieldSeparator(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const = 0;
	virtual std::string copyRecordSeparator(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const = 0;
};

class GenIdWrapper : public ValueFetcher
{
protected:
	unsigned long long int value;
public:
	GenIdWrapper(unsigned long long int value)
		: value(value)
	{}
	bool isNull(size_t col)
	{
		return false;
	}
	void fetch(size_t col, int &toVar)
	{
		toVar = value;
	}
	void fetch(size_t col, unsigned int &toVar)
	{
		toVar = value;
	}
	void fetch(size_t col, long long int &toVar)
	{
		toVar = value;
	}
	void fetch(size_t col, unsigned long long int &toVar)
	{
		toVar = value;
	}
	void fetch(size_t col, double &toVar)
	{
		toVar = value;
	}
	void fetch(size_t col, float &toVar)
	{
		toVar = value;
	}
	void fetch(size_t col, char &toVar) {}
	void fetch(size_t col, bool &toVar) {}
	void fetch(size_t col, std::string &toVar) {}
	void fetch(size_t col, std::tm &toVar) {}
};

class OStreamWrapper : public ValueBinder
{
protected:
	std::ostream *out;
public:
	OStreamWrapper(std::ostream *out)
		: out(out)
	{}
	OStreamWrapper(std::ostream &out)
		: out(&out)
	{}

	void bindNull(size_t col)
	{
		if (out)
		{
			(*out) << "(NULL)";
		}
	}
	void bind(size_t col, const int &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const unsigned int &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const long long int &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const unsigned long long int &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const double &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const float &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const char &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const bool &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const std::string &newValue)
	{
		if (out)
		{
			(*out) << newValue;
		}
	}
	void bind(size_t col, const std::tm &newValue)
	{
		if (out)
		{
			(*out) << "{"
				   << newValue.tm_year << "-" << newValue.tm_mon << "-" << newValue.tm_mday << " "
				   << newValue.tm_hour << ":" << newValue.tm_min << ":" << newValue.tm_sec
				   << "}";
		}
	}
};

class AbstractFieldParser : public ValueFetcher
{
protected:
	std::string value;
public:
	AbstractFieldParser() {}
	virtual ~AbstractFieldParser() {}
	void setStringValue(const std::string &value)
	{
		this->value = value;
	}
	const std::string &getStringValue() const
	{
		return this->value;
	}
	bool isNull(size_t /*col*/)
	{
		return false;
	}
};

class ScopedTransaction
{
protected:
	ConnectionHandler &cn;
	bool do_release_all;
	void finish()
	{
		if (do_release_all)
		{
			cn.releaseAll();
		}
	}
public:
	ScopedTransaction(ConnectionHandler &cn, int characteristics = TRANS_DEFAULT, bool release_all = true)
		: cn(cn), do_release_all(release_all)
	{
		this->cn.begin(characteristics);
	}
	ScopedTransaction(ConnectionHandler *cn, int characteristics = TRANS_DEFAULT, bool release_all = true)
		: cn(*cn), do_release_all(release_all)
	{
		this->cn.begin(characteristics);
	}
	virtual ~ScopedTransaction()
	{
		rollback();
	}
	void rollback()
	{
		cn.rollback();
		this->finish();
	}
	void commit()
	{
		cn.commit();
		this->finish();
	}
};

} // namespace Internal

typedef SmartPtr<Internal::ResultHandler> ResultSet;
typedef SmartPtr<Internal::ConnectionHandler> Connection;
typedef SmartPtr<Internal::StatementHandler> PreparedStatement;
typedef SmartPtr<Internal::CopyOutputStreamHandler> CopyOutputStream;
typedef SmartPtr<Internal::CopyInputStreamHandler> CopyInputStream;
typedef Internal::ScopedTransaction ScopedTransaction;

} // namespace Db

END_APP_NAMESPACE

#endif // __DB_ABSTRACT_DB_HANDLER_H__

