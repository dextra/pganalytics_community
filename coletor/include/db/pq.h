#ifndef __DB_PQ_H__
#define __DB_PQ_H__
#include "debug.h"

extern "C" {
#include <libpq-fe.h>
}

/* for ntohl/htonl */
//#include <netinet/in.h>
//#include <arpa/inet.h>
#include <limits>
#include <cstdlib>
#include <sstream>
#include <cstring>

BEGIN_APP_NAMESPACE

namespace Db
{
namespace Internal
{

namespace PQPrivate
{
static void check_res_for_exception(PGresult *res, ExecStatusType expected, bool clear_on_error = true)
{
	if (!res)
	{
		throw Exception("Null PGresult");
	}
	if (PQresultStatus(res) != expected)
	{
		std::string msg = PQresultErrorMessage(res);
		if (clear_on_error)
		{
			PQclear(res);
		}
		throw Exception(msg);
	}
}
}

namespace PQFormatter
{
static std::string quoteString(const std::string &original)
{
	/* XXX: Assumes standard_conforming_strings = on */
	std::string tmp = "'";
	for (size_t i = 0; i < original.size(); i++)
	{
		if (original[i] == '\'')
		{
			tmp += '\'';
		}
		tmp += original[i];
	}
	tmp += "'";
	return tmp;
}
static std::string quoteIdentifier(const std::string &original)
{
	std::string tmp;
	bool needs_quote = false;
	for (size_t i = 0; i < original.size(); i++)
	{
		if (!(
					(original[i] >= 'a' && original[i] <= 'z')
					|| (original[i] == '_')
					|| (i > 0 && original[i] >= '0' && original[i] <= '9')
				)
		   )
		{
			needs_quote = true;
		}
		if (original[i] == '"')
		{
			needs_quote = true;
			tmp += '"';
		}
		tmp += original[i];
	}
	if (needs_quote)
	{
		tmp = "\"" + tmp + "\"";
	}
	return tmp;
}
static std::string copyValue(const std::string &original, const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0)
{
	std::string tmp;
	for (size_t i = 0; i < original.size(); i++)
	{
		switch(original[i])
		{
		case '\\':
			tmp += "\\\\";
			break;
		case '\b':
			tmp += "\\b";
			break;
		case '\f':
			tmp += "\\f";
			break;
		case '\n':
			tmp += "\\n";
			break;
		case '\r':
			tmp += "\\r";
			break;
		case '\t':
			tmp += "\\t";
			break;
		case '\v':
			tmp += "\\v";
			break;
		default:
			tmp += original[i];
		}
	}
	return tmp;
}
static std::string copyNullValue(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0)
{
	return "\\N";
}
static std::string copyFieldSeparator(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0)
{
	return "\t";
}
static std::string copyRecordSeparator(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0)
{
	return "\n";
}
static std::string escapeConnectionParameter(const std::string &value)
{
	std::string tmp;
	tmp = '\'';
	for (size_t i = 0; i < value.size(); i++)
	{
		if (value[i] == '\\' || value[i] == '\'')
		{
			tmp += '\\';
		}
		tmp += value[i];
	}
	tmp += '\'';
	return tmp;
}
} // namespace PQFormatter


class PQResult : public ResultHandler
{
protected:
	PGresult *res;
	int nfields;
	int ntuples;
	int row;
	std::stringstream ss;
private:
	void validate_col(size_t col)
	{
		if ((int)col > nfields)
		{
			std::stringstream ss;
			ss << "Column " << col << " is out of bound";
			throw Exception(ss.str());
		}
	}
	template<typename T>
	T parse_number(std::string const &s)
	{
		ss.clear();
		ss.str(s);
		if(s.find_first_of(".eEdD")!=std::string::npos)
		{
			long double v;
			ss >> v;
			if(ss.fail() || !std::ws(ss).eof())
				throw BadValueCastException();
			if(std::numeric_limits<T>::is_integer)
			{
				if(v > std::numeric_limits<T>::max() || v < std::numeric_limits<T>::min())
					throw BadValueCastException();
			}
			return (T)(v);
			//return static_cast<T>(v);
		}
		T v;
		ss >> v;
		if(ss.fail() || !std::ws(ss).eof())
			throw BadValueCastException();
		if(std::numeric_limits<T>::is_integer
				&& !std::numeric_limits<T>::is_signed
				&& s.find('-') != std::string::npos
				&& v!=0)
		{
			throw BadValueCastException();
		}
		return v;
	}
	template<typename T>
	void do_fetch_number(size_t col, T &value)
	{
		validate_col(col);
		if (!isNull(col))
		{
			std::string tmp(PQgetvalue(res, row, col), PQgetlength(res, row, col));
			value = parse_number<T>(tmp);
		}
	}
public:
	PQResult(PGresult *res)
		: res(res)
	{
		PQPrivate::check_res_for_exception(res, PGRES_TUPLES_OK, false);
		nfields = PQnfields(res);
		ntuples = PQntuples(res);
		row = -1;
		ss.imbue(std::locale("C"));
	}
	virtual ~PQResult()
	{
		PQclear(res);
	}
	bool next()
	{
		row++;
		if (row+1 > ntuples)
		{
			return false;
		}
		return true;
	}
	size_t ncols()
	{
		return PQnfields(res);
	}
	size_t nrows()
	{
		return PQntuples(res);
	}
	std::string colName(size_t col)
	{
		std::string tmp;
		validate_col(col);
		tmp = PQfname(res, (int)col);
		return tmp;
	}
	size_t colNumber(const std::string &field_name)
	{
		int ret = PQfnumber(res, field_name.c_str());
		if (ret < 0)
		{
			std::stringstream ss;
			ss << "Column \"" << field_name << "\" not found";
			throw Exception(ss.str());
		}
		return (size_t)ret;
	}
	bool isNull(size_t col)
	{
		validate_col(col);
		return PQgetisnull(res, row, col);
	}
	void fetch(size_t col, int &toVar)
	{
		do_fetch_number(col, toVar);
	}
	void fetch(size_t col, unsigned int &toVar)
	{
		do_fetch_number(col, toVar);
	}
	void fetch(size_t col, long long int &toVar)
	{
		do_fetch_number(col, toVar);
	}
	void fetch(size_t col, unsigned long long int &toVar)
	{
		do_fetch_number(col, toVar);
	}
	void fetch(size_t col, double &toVar)
	{
		do_fetch_number(col, toVar);
	}
	void fetch(size_t col, float &toVar)
	{
		do_fetch_number(col, toVar);
	}
	void fetch(size_t col, char &toVar)
	{
		std::string tmp;
		fetch(col, tmp);
		if (tmp.size() > 1)
		{
			throw BadValueCastException();
		}
		toVar = tmp[0];
	}
	void fetch(size_t col, bool &toVar)
	{
		int tmp;
		this->fetch(col, tmp);
		toVar = tmp;
	}
	void fetch(size_t col, std::string &toVar)
	{
		if (!isNull(col))
		{
			toVar = PQgetvalue(res, row, col);
		}
	}

protected:
	/* Functions to validate date/time */
	inline bool isleapyear(unsigned short year)
	{
		return ((!(year%4) && (year%100)) || !(year%400));
	}
	inline bool validate(const std::tm &t)
	{
		unsigned short monthlen[]= {31,28,31,30,31,30,31,31,30,31,30,31};
		if ((t.tm_year < 0) || (t.tm_mon < 0) || (t.tm_mday <= 0) || (t.tm_mon >= 12))
		{
			//DMSG("Invalid range: " << t.tm_year << "-" << t.tm_mon << "-" << t.tm_mday << "\n");
			return false;
		}
		if ((t.tm_mon == 1) && isleapyear(t.tm_year + 1900))
		{
			monthlen[1]++;
		}
		if (t.tm_mday > monthlen[t.tm_mon])
		{
			//DMSG("Month " << t.tm_mon << " can't have " << t.tm_mday << " days!\n");
			return false;
		}
		return true;
	}

public:
	void fetch(size_t col, std::tm &toVar)
	{
		if (isNull(col))
		{
			return;
		}
		std::string aux;
		this->fetch(col, aux);
		int n;
		double sec = 0;
		n = sscanf(aux.c_str(), "%d-%d-%d %d:%d:%lf",
				   &toVar.tm_year,&toVar.tm_mon,&toVar.tm_mday, &toVar.tm_hour,&toVar.tm_min,&sec);
		if (n!=3 && n!=6)
		{
			//DMSG("Invalid format for " << aux);
			throw BadValueCastException();
		}
		toVar.tm_year -= 1900;
		toVar.tm_mon -= 1;
		toVar.tm_isdst = -1;
		toVar.tm_sec = (int)sec;
		if (!validate(toVar))
		{
			//DMSG("Invalid date " << aux);
			throw BadValueCastException();
		}
	}
	void fetch(size_t col, std::pair<std::istream*,std::ostream*> &toVar)
	{
		throw NotImplementedException();
		//result.fetch(col, *(toVar.second));
	}
};

class PQStatement : public StatementHandler
{
protected:
	PGconn *conn;
	size_t nargs;
	char **paramValues;
	int *paramLengths;
	std::string stmt_name;
private:
	template<typename T>
	void do_bind(size_t col, const T &value)
	{
		std::ostringstream ss;
		ss << value;
		std::string tmp = ss.str();
		bind(col, tmp);
	}
public:
	PQStatement(PGconn *conn, const std::string &sql)
		: conn(conn)
	{
		std::ostringstream name;
		name << "s" << ((void*)this);
		stmt_name = name.str();
		std::string tmp;
		tmp.reserve(sql.length() + 100);
		nargs = 0;
		for (size_t i = 0; i < sql.length(); i++)
		{
			if (sql[i] == '?')
			{
				nargs++;
				std::ostringstream ss;
				ss << "$" << nargs;
				tmp.append(ss.str());
			}
			else
			{
				tmp += sql[i];
			}
		}
		PGresult *res = PQprepare(conn, stmt_name.c_str(), tmp.c_str(), nargs, NULL);
		PQPrivate::check_res_for_exception(res, PGRES_COMMAND_OK, true);
		PQclear(res);
		paramValues = (char**)calloc(sizeof(char*), nargs);
		paramLengths = (int*)calloc(sizeof(int), nargs);
	}
	virtual ~PQStatement()
	{
		if (!stmt_name.empty())
		{
			PGresult *res = PQexec(conn, std::string("DEALLOCATE " + stmt_name + ";").c_str());
			PQPrivate::check_res_for_exception(res, PGRES_COMMAND_OK, true);
			PQclear(res);
		}
		for (size_t i = 0; i < nargs; i++)
		{
			if (paramValues[i] != 0)
			{
				::free(paramValues[i]);
			}
		}
		::free(paramValues);
		::free(paramLengths);
	}
	void execUpdate()
	{
		PGresult *res = PQexecPrepared(conn, stmt_name.c_str(), nargs, paramValues, paramLengths, NULL, 0);
		PQPrivate::check_res_for_exception(res, PGRES_COMMAND_OK, true);
		PQclear(res);
	}
	ResultHandler *execQuery()
	{
		PGresult *res = PQexecPrepared(conn, stmt_name.c_str(), nargs, paramValues, paramLengths, NULL, 0);
		return new PQResult(res);
	}
	unsigned long long int sequenceLastId(const std::string &seqName)
	{
		unsigned long long int ret;
		const char *params[] = {seqName.c_str()};
		int lengths[] = {(int)seqName.size()};
		PGresult *res = PQexecParams(conn, "SELECT currval($1)", 1, NULL, params, lengths, NULL, 0);
		PQPrivate::check_res_for_exception(res, PGRES_TUPLES_OK, true);
		std::stringstream ss(PQgetvalue(res, 0, 0));
		ss >> ret;
		PQclear(res);
		return ret;
	}
	void bindNull(size_t col)
	{
		paramValues[col] = NULL;
	}
	void bind(size_t col, const int &newValue)
	{
		do_bind(col, newValue);
	}
	void bind(size_t col, const unsigned int &newValue)
	{
		do_bind(col, newValue);
	}
	void bind(size_t col, const long long int &newValue)
	{
		do_bind(col, newValue);
	}
	void bind(size_t col, const unsigned long long int &newValue)
	{
		do_bind(col, newValue);
	}
	void bind(size_t col, const double &newValue)
	{
		do_bind(col, newValue);
	}
	void bind(size_t col, const float &newValue)
	{
		do_bind(col, newValue);
	}
	void bind(size_t col, const char &newValue)
	{
		std::string tmp = "?";
		tmp[0] = newValue;
		this->bind(col+1, tmp);
	}
	void bind(size_t col, const bool &newValue)
	{
		int tmp = (newValue ? 1 : 0);
		this->bind(col+1, tmp);
	}
	void bind(size_t col, const std::string &newValue)
	{
		if (col+1 > nargs)
		{
			std::ostringstream ss;
			ss << "Column " << col << " is out of bound";
			throw Exception(ss.str());
		}
		/* strdup may not be available on all platforms */
		//char *v = strdup(newValue.c_str());
		char *v = (char *)malloc(newValue.size()+1); /* it will be freed on the destructor */
		memcpy(v, newValue.c_str(), newValue.size()+1);
		paramValues[col] = v;
		paramLengths[col] = newValue.length();
	}
	void bind(size_t col, const std::tm &newValue)
	{
		std::ostringstream ss;
		ss
				<< (newValue.tm_year+1900) << "-" << (newValue.tm_mon+1) << "-" << (newValue.tm_mday)
				<< " "
				<< (newValue.tm_hour) << ":" << (newValue.tm_min) << ":" << (newValue.tm_sec);
		std::string tmp = ss.str();
		bind(col, tmp);
	}
	void bind(size_t col, std::pair<std::istream*,std::ostream*> &newValue)
	{
		throw NotImplementedException();
	}
};

class PQCopyOutputStream : public CopyOutputStreamHandler
{
protected:
	PGconn *conn;
	PGresult *res;
	char *buffer;
	void freeBuffer()
	{
		if (buffer != NULL)
		{
			PQfreemem(buffer);
			buffer = NULL;
		}
	}
public:
	PQCopyOutputStream(PGconn *conn, PGresult *res)
		: conn(conn), res(res), buffer(NULL)
	{}
	virtual ~PQCopyOutputStream()
	{
		this->freeBuffer();
		PQclear(res);
	}
	bool next()
	{
		int ret;
		this->freeBuffer();
		ret = PQgetCopyData(conn, &buffer, 0);
		return (ret >= 0);
	}
	std::string get() const
	{
		std::string tmp;
		if (buffer == NULL)
		{
			throw Exception("No data available");
		}
		tmp = buffer;
		return tmp;
	}
};

class PQCopyInputStream : public CopyInputStreamHandler
{
protected:
	PGconn *conn;
	PGresult *res;
	char *buffer;
	int line;
	void do_end(bool end_ok)
	{
		int ret = PQputCopyEnd(conn, (end_ok || (PQprotocolVersion(conn) < 3)) ? NULL : "abort");
		if (ret < 0)
		{
			throw Exception(std::string("Error at COPY ... FROM end: ") + PQerrorMessage(conn));
		}
		/* Logic stolen from src/bin/psql/copy.c file of PostgreSQL 9.4 source */
		PGresult *res2;
		while (res2 = PQgetResult(conn), PQresultStatus(res2) == PGRES_COPY_IN)
		{
			PQclear(res2);
			/* We can't send an error message if we're using protocol version 2 */
			PQputCopyEnd(conn, (PQprotocolVersion(conn) < 3) ? NULL : "trying to exit copy mode");
		}
		if (PQresultStatus(res2) != PGRES_COMMAND_OK)
		{
			throw Exception(std::string("Error at COPY ... FROM end: ") + PQerrorMessage(conn));
		}
	}
public:
	PQCopyInputStream(PGconn *conn, PGresult *res)
		: conn(conn), res(res), line(0)
	{}
	virtual ~PQCopyInputStream()
	{
		PQclear(res);
	}
	void put(const std::string &str)
	{
		int ret = PQputCopyData(conn, str.c_str(), str.size());
		line++;
		if (ret < 0)
		{
			std::stringstream ss;
			ss << "Error while sending COPY ... FROM data at line " << line << ": " << PQerrorMessage(conn);
			throw Exception(ss.str());
		}
	}
	void putLine(const std::string &str)
	{
		this->put(str + "\n");
	}
	void end()
	{
		this->do_end(true);
	}
	void abort()
	{
		this->do_end(false);
	}
};

class PQConnection : public ConnectionHandler
{
protected:
	PGconn *conn;
private:
	bool inside_transaction;
public:
	void execCommand(const std::string &sql)
	{
		PGresult *res = PQexec(conn, sql.c_str());
		PQPrivate::check_res_for_exception(res, PGRES_COMMAND_OK, true);
		PQclear(res);
	}
	PQConnection(const std::string &conn_info)
		: inside_transaction(false)
	{
		conn = PQconnectdb(conn_info.c_str());
		if (PQstatus(conn) != CONNECTION_OK)
		{
			std::string msg = PQerrorMessage(conn);
			throw ConnectionException(msg);
		}
	}
	virtual ~PQConnection()
	{
		PQfinish(conn);
	}
	virtual StatementHandler *prepare(const std::string &sql)
	{
		return new PQStatement(this->conn, sql);
	}
	virtual void begin(int characteristics)
	{
		std::string cmd = "BEGIN";
		if (characteristics & TRANS_ISOLATION_READ_UNCOMMITED)
		{
			cmd.append(" ISOLATION LEVEL READ UNCOMMITED");
		}
		else if (characteristics & TRANS_ISOLATION_READ_COMMITED)
		{
			cmd.append(" ISOLATION LEVEL READ COMMITED");
		}
		else if (characteristics & TRANS_ISOLATION_REPEATABLE_READ)
		{
			cmd.append(" ISOLATION LEVEL REPEATABLE READ");
		}
		else if (characteristics & TRANS_ISOLATION_SERIALIZABLE)
		{
			cmd.append(" ISOLATION LEVEL SERIALIZABLE");
		}
		if (characteristics & TRANS_READ_WRITE)
		{
			cmd.append(" READ WRITE");
		}
		else if (characteristics & TRANS_READ_ONLY)
		{
			cmd.append(" READ ONLY");
		}
		if (characteristics & TRANS_DEFERRABLE)
		{
			cmd.append(" DEFERRABLE");
		}
		else if (characteristics & TRANS_NOT_DEFERRABLE)
		{
			cmd.append(" NOT DEFERRABLE");
		}
		cmd.append(";");
		this->execCommand(cmd);
		inside_transaction = true;
	}
	virtual void commit()
	{
		this->execCommand("COMMIT;");
		inside_transaction = false;
	}
	virtual void rollback()
	{
		this->execCommand("ROLLBACK;");
		inside_transaction = false;
	}
	ResultHandler *execQuery(const std::string &stmt)
	{
		PGresult *res = PQexec(conn, stmt.c_str());
		return new PQResult(res);
	}
	virtual void releaseAll()
	{
		if (inside_transaction)
		{
			this->rollback();
		}
		//PGresult *res = PQexec(conn, "DISCARD ALL;");
		//PQPrivate::check_res_for_exception(res, PGRES_COMMAND_OK, true);
		//PQclear(res);
	}
	CopyOutputStreamHandler *openCopyOutputStream(const std::string &cmd)
	{
		PGresult *res = PQexec(conn, cmd.c_str());
		PQPrivate::check_res_for_exception(res, PGRES_COPY_OUT, true);
		return new PQCopyOutputStream(conn, res);
	}
	CopyInputStreamHandler *openCopyInputStream(const std::string &cmd)
	{
		PGresult *res = PQexec(conn, cmd.c_str());
		PQPrivate::check_res_for_exception(res, PGRES_COPY_IN, true);
		return new PQCopyInputStream(conn, res);
	}
	std::string quoteString(const std::string &original) const
	{
		return PQFormatter::quoteString(original);
	}
	std::string quoteIdentifier(const std::string &original) const
	{
		return PQFormatter::quoteIdentifier(original);
	}
	std::string copyValue(const std::string &original, const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const
	{
		return PQFormatter::copyValue(original, handler);
	}
	std::string copyNullValue(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const
	{
		return PQFormatter::copyNullValue(handler);
	}
	std::string copyFieldSeparator(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const
	{
		return PQFormatter::copyFieldSeparator(handler);
	}
	std::string copyRecordSeparator(const CopyOutputStreamHandler *handler = (CopyOutputStreamHandler*)0) const
	{
		return PQFormatter::copyRecordSeparator(handler);
	}
	static std::string escapeConnectionParameter(const std::string &value)
	{
		return PQFormatter::escapeConnectionParameter(value);
	}
};

} // namespace Internal

typedef Internal::PQConnection PQConnection;

} // namespace Db

END_APP_NAMESPACE

#endif // __DB_PQ_H__

