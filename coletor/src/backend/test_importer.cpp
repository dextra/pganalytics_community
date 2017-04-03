#include "poco_regexp.h"
#include "db/Database.h"
#include "db/pq.h"
#include "backend_shared.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <typeinfo>
#include <execinfo.h>

namespace Util
{
std::string unescapeString(const std::string &original, const std::string &quotes = "")
{
	std::string ret;
	for (size_t i = 0; i < original.size(); i++)
	{
		if (original[i] == '\\')
		{
			/* It is escaping, check next character */
			i++;
			switch(original[i])
			{
				/* XXX: This could be on another function if we need to use again */
			case 'a':
				ret += '\a';
				break;
			case 'r':
				ret += '\r';
				break;
			case 'n':
				ret += '\n';
				break;
			case 't':
				ret += '\t';
				break;
			case '\\':
				ret += '\\';
				break;
			default:
				if (quotes.find(original[i]) == std::string::npos)
				{
					std::ostringstream err;
					err
							<< "Invalid escape '" << original[i] << "' at " << i << " of \"" << original << "\"";
					throw std::runtime_error(err.str());
				}
				ret += original[i];
			}
		}
		else
		{
			ret += original[i];
		}
	}
	return ret;
}

std::string escapeString(const std::string &original, const std::string &quotes)
{
	std::string ret;
	for (size_t i = 0; i < original.size(); i++)
	{
		switch(original[i])
		{
		case '\a':
			ret += "\\a";
			break;
		case '\r':
			ret += "\\r";
			break;
		case '\n':
			ret += "\\n";
			break;
		case '\t':
			ret += "\\t";
			break;
		case '\\':
			ret += "\\\\";
			break;
		default:
			if (quotes.find(original[i]) != std::string::npos)
			{
				ret += "\\";
			}
			ret += original[i];
		}
	}
	return ret;
}
}

void print_stack_trace()
{
	const int STACK_SIZE = 5;
	void *buffer[STACK_SIZE];
	char **stack_item;
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(buffer, STACK_SIZE);

	// print out all the frames to stderr
	//fprintf(stderr, "Stack trace:\n");
	//backtrace_symbols_fd(buffer, size, STDERR_FILENO);
	stack_item = backtrace_symbols(buffer, size);
	if (stack_item == NULL)
	{
		perror("backtrace_symbols");
	}
	else
	{
		for (size_t i = 0; i < size; i++)
			printf("#%d: %s\n", (size - i), stack_item[i]);
		free(stack_item);
	}
}

class exception_stack : public std::exception
{
public:
	exception_stack()
	{
		print_stack_trace();
	}
	virtual ~exception_stack() throw() {}
	virtual const char* what() const throw()
	{
		return "exception_stack";
	}
};

class ImportOutputHandler
{
public:
	virtual void execute(const std::string &stmt) = 0;
	virtual void callFunction(const std::string &stmt) = 0;
	virtual void copyBegin(const std::string &stmt) = 0;
	virtual void copyPutLine(const std::string &line) = 0;
	/* virtual void copyPutBuffer(const char *buffer, size_t n) = 0; */
	virtual void copyEnd() = 0;
	virtual std::string quoteString(const std::string &source) = 0;
};

class ImportToStream : public ImportOutputHandler
{
protected:
	std::ostream &m_ostr;
public:
	ImportToStream(std::ostream &ostr)
		: m_ostr(ostr)
	{}
	void execute(const std::string &stmt)
	{
		this->m_ostr << "/* execute SQL */ " << stmt << std::endl;
	}
	void callFunction(const std::string &stmt)
	{
		this->execute("SELECT " + stmt);
	}
	void copyBegin(const std::string &stmt)
	{
		this->m_ostr << "/* execute COPY */ " << stmt << std::endl;
	}
	void copyPutLine(const std::string &line)
	{
		this->m_ostr << line << std::endl;
	}
	void copyEnd()
	{
		this->m_ostr << "\\." << std::endl;
	}
	std::string quoteString(const std::string &source)
	{
		std::string ret = "E'";
		ret.append(Util::escapeString(source, "'"));
		ret.append("'");
		return ret;
	}
};

class ImportToDatabase : public ImportOutputHandler
{
protected:
	pga::Db::Connection m_cn;
	pga::Db::CopyInputStream m_copy_in;
public:
	ImportToDatabase(pga::Db::Connection cn)
		: m_cn(cn)
	{}
	void execute(const std::string &stmt)
	{
		this->m_cn->execCommand(stmt);
	}
	void callFunction(const std::string &stmt)
	{
		pga::Db::ResultSet rs = this->m_cn->execQuery("SELECT " + stmt);
		(void)rs;
	}
	void copyBegin(const std::string &stmt)
	{
		this->m_copy_in = this->m_cn->openCopyInputStream(stmt);
	}
	void copyPutLine(const std::string &line)
	{
		this->m_copy_in->putLine(line);
	}
	void copyEnd()
	{
		this->m_copy_in->end();
		this->m_copy_in = NULL;
	}
	std::string quoteString(const std::string &source)
	{
		return this->m_cn->quoteString(source);
	}
};

struct cnt_reader
{
	std::istream &m_istr;
	size_t m_line;
	cnt_reader(std::istream &istr)
		: m_istr(istr), m_line(0)
	{}
	std::istream &getline(std::string &result)
	{
		if (std::getline(this->m_istr, result))
		{
			this->m_line++;
		}
		return this->m_istr;
	}
	inline size_t line() const
	{
		return this->m_line;
	}
	inline std::istream &istream()
	{
		return this->m_istr;
	}
};

inline void check_safety(const std::string &sql)
{
	/* Safe for me means, has no ";". We only need that now. */
	if (!(sql.find_first_of(";") == std::string::npos))
	{
		throw std::runtime_error("Not safe string: \"" + sql + "\"");
	}
}

void process_copy(ImportOutputHandler &out, cnt_reader &istr, const std::string &command, std::string table, const std::string &columns)
{
	size_t n;
	std::ostringstream sql;
	std::string line;
	/* check if safe */
	check_safety(table);
	check_safety(columns);
	/* options are given from the app itself, no need (and really can't) check */
	if (command == COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE)
	{
		bool add_comma_before = false;
		table = "_tmp_" + table;
		sql
				<< "CREATE TEMP TABLE " << table << "(";
		for (size_t i = 0; i < columns.size() + 1; i++)
		{
			if (i >= columns.size() || columns[i] == ',')
			{
				sql << " text";
				add_comma_before = true;
			}
			else
			{
				if (add_comma_before)
				{
					add_comma_before = false;
					sql << ", ";
				}
				sql << columns[i];
			}
		}
		sql << ") ON COMMIT DROP;\n";
		sql << "GRANT SELECT ON TABLE " << table << " TO PUBLIC;";
		out.execute(sql.str());
		sql.clear();
		sql.str("");
	}
	sql << "COPY " << table << "(" << columns << ") "
		<< "FROM stdin"
		<< (command == COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE ? " WITH (FORMAT CSV, DELIMITER ';')" : "")
		<< " /* at " << istr.line() << " */"
		<< ";";
	out.copyBegin(sql.str());
	for (n = 0; istr.getline(line); n++)
	{
		if (line == "\\.")
		{
			break;
		}
		else
		{
			out.copyPutLine(line);
		}
	}
	out.copyEnd();
}

void test_exception1()
{
	printf(__PRETTY_FUNCTION__);
	throw exception_stack();
}

void test_exception2()
{
	test_exception1();
	printf(__PRETTY_FUNCTION__);
}

void test_exception()
{
	printf(__PRETTY_FUNCTION__);
	test_exception2();
}

void add_to_metadata(std::map<std::string, std::string> &meta, const std::string &line)
{
	std::istringstream istr(line);
	std::string hash;
	std::string key;
	std::string value;
	istr >> hash >> key;
	(void)istr.get(); /* ignore space */
	if (!istr || hash != "#" || key.empty())
	{
		throw std::runtime_error("Invalid meta-data");
	}
	if (key[key.size()-1] == ':')
	{
		key = key.substr(0, key.size() - 1);
	}
	std::getline(istr, value); /* Doesn't matter if empty, value can be empty */
	meta[key] = Util::unescapeString(value);
}

void start_snapshot(ImportOutputHandler &out, std::map<std::string, std::string> &meta, const std::string &filename)
{
	std::cerr << "Starting snapshot" << std::endl;
	for (std::map<std::string, std::string>::iterator it = meta.begin(); it != meta.end(); it++)
	{
		std::cerr << std::string("meta[\"" + it->first + "\"] = \"" + it->second + "\";") << std::endl;
	}
	std::ostringstream sql;
	sql << "sn_import_snapshot("
		<< "snap_type     := " << out.quoteString(meta["snap_type"]) << ", "
		<< "customer_name := " << out.quoteString(meta["customer_name"]) << ", "
		<< "server_name   := " << out.quoteString(meta["server_name"]) << ", "
		<< "snap_hash     := " << out.quoteString(filename) << ", "
		<< "datetime      := to_timestamp(float " << out.quoteString(meta["datetime"]) << "), "
		<< "real_datetime := to_timestamp(float " << out.quoteString(meta["real_datetime"]) << "), "
		<< "instance_name := " << (meta.find("instance_name") == meta.end() ? "NULL" : out.quoteString(meta["instance_name"])) << ", "
		<< "datname       := " << (meta.find("datname") == meta.end() ? "NULL" : out.quoteString(meta["datname"])) << " "
		<< ");";
	out.callFunction(sql.str());
}

int main(int argc, char *argv[])
{
	cnt_reader istr(std::cin);
	std::string line;
	std::map<std::string, std::string> meta;
	bool reading_meta_data = true;
	//test_exception();
	//ImportToStream out(std::cout);
	ImportToDatabase out(new pga::Db::PQConnection("dbname=pganalytics user=ctm_tjpr_imp"));
	try
	{
		out.execute("BEGIN;");
		while (istr.getline(line))
		{
			std::cerr << "line: " << line << std::endl;
			if (reading_meta_data && line[0] != '#')
			{
				reading_meta_data = false;
				/* End of meta data, let's check it */
				start_snapshot(out, meta, "1408643235-0002f0-0001-pgs.pga");
			}
			if (reading_meta_data)
			{
				add_to_metadata(meta, line);
			}
			else
			{
				std::istringstream iss_params(line);
				std::string command;
				iss_params >> command;
				if (iss_params)
				{
					if (command == COMMAND_COPY_TAB_DELIMITED || command == COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE)
					{
						std::string table;
						std::string columns;
						iss_params >> table >> columns;
						if (!iss_params)
						{
							std::ostringstream err;
							err << "Invalid parameters at line " << istr.line();
							throw std::runtime_error(err.str());
						}
						process_copy(out, istr, command, table, columns);
					}
					else if (command[0] == '-' && command[1] == '-')
					{
						/* A comment */
						std::cerr << line << std::endl;
					}
					else
					{
						std::ostringstream err;
						err << "Unknown command \"" << command << "\" at line " << istr.line();
						throw std::runtime_error(err.str());
					}
				}
			}
		}
		out.execute("ROLLBACK;");
	}
	catch (std::exception &e)
	{
		try
		{
			out.execute("ROLLBACK;");
			std::cerr << "Error: " << typeid(e).name() << ": " << e.what() << std::endl;
			print_stack_trace();
		}
		catch(std::exception &e)
		{
		}
	}
	return 0;
}

