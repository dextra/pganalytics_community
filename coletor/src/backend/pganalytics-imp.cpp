#include "common.h"
#include "db/Database.h"
#include "db/pq.h"
#include "backend_shared.h"
#include "util/string.h"
#include "util/app.h"
#include "util/fs.h"
#include "util/streams.h"
#include "util/time.h"
#include "util/log.h"
#include "getopt_long.h"

#include "backend/LogParser.h"
#include "backend/LineHandler.h"

#include "backend/importer.h"

#include <libaws/aws.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <typeinfo>

#include <signal.h>
#include <unistd.h> // fork
#include <sys/types.h>
#include <sys/wait.h> // waitpid
#include <sys/prctl.h>


BEGIN_APP_NAMESPACE

struct pgaimporter_options
{
	enum OutputMode
	{
		OUTPUT_INVALID,
		OUTPUT_STDOUT,
		OUTPUT_FILE,
		OUTPUT_DATABASE
	};
	enum InputMode
	{
		INPUT_INVALID,
		INPUT_STDIN,
		INPUT_FILE,
		INPUT_S3_BUCKET,
		INPUT_CUSTOMERS,
		INPUT_GROUP
	};
	std::string output;
	OutputMode output_mode;
	std::string input;
	InputMode input_mode;
	std::string pgadb;
	std::string aws_access_key;
	std::string aws_secret_access_key;
	std::string filename;
	std::string format;
	std::string aws_bucket;
	std::string aws_path;
	std::string process_customer;
	int max_buckets;
	int max_files_per_bucket;
	int max_files_all;
	bool check_only;
	bool batch_loop;
	size_t batch_loop_sleep_interval;
	size_t batch_loop_sleep_noproc;
	bool batch_cancel_next_file;
	bool group_import;
	int group_id;
	bool in_master;
	bool signaled;
	int last_signal;
	pgaimporter_options() :
		output_mode(OUTPUT_INVALID), input_mode(INPUT_INVALID),
		format("gzip"),
		max_buckets(-1), max_files_per_bucket(-1), max_files_all(-1),
		check_only(false), batch_loop(false), batch_loop_sleep_interval(10000),
		batch_loop_sleep_noproc(300000), batch_cancel_next_file(false),
		group_import(false), group_id(0), in_master(false), signaled(false), last_signal(0)
	{}
};
struct pgaimporter_options options;

struct groups_info_t
{
	int group_id;
	std::string client_encoding;
	pid_t child_pid;
	pgaimporter_options child_options;
};


class ImporterContext
{
protected:
	PgaImporterAppPtr m_app;
	std::string m_customer;
	std::string m_action;
	size_t m_last_error_count;
	Db::Connection m_err_con;
public:
	ImporterContext()
		: m_last_error_count(0)
	{}
	void app(PgaImporterAppPtr app);
	void updateProcessTitle();
	void set(const std::string &customer, const std::string &action);
	void customer(const std::string &customer, bool update_process_title = false);
	void action(const std::string &action, bool update_process_title = false);
	void logError(const std::exception &e, const std::string &source = "", int line = 0);
	inline void sleep_noproc()
	{
		this->action("waiting itens", true);
		Util::time::sleep(options.batch_loop_sleep_noproc);
	}
	inline void sleep_batch()
	{
		this->action("waiting batch sleep", true);
		Util::time::sleep(options.batch_loop_sleep_interval);
	}
};
ImporterContext context;
Util::log::Logger logger;

#define LOG_ERROR(e) context.logError((e), __PRETTY_FUNCTION__, __LINE__)

void ImporterContext::app(PgaImporterAppPtr app)
{
	this->m_app = app;
}

void ImporterContext::updateProcessTitle()
{
	if (!this->m_app.isNull())
	{
		std::ostringstream title;
		title << "pganalytics-importer [";
		if (options.group_id > 0)
		{
			title << options.group_id << "/";
		}
		title << this->m_customer << "] " << this->m_action;
		this->m_app->updateProcessTitle(title.str());
		::prctl(PR_SET_NAME, (unsigned long)title.str().c_str(), 0, 0, 0);
		logger.log() << title.str() << std::endl;
	}
}

void ImporterContext::set(const std::string &customer, const std::string &action)
{
	this->m_customer = customer;
	this->m_action = action;
	this->updateProcessTitle();
	/* After 50 imports with no error, then close the error connection: */
	if (++(this->m_last_error_count) > 50 && !this->m_err_con.isNull())
	{
		this->m_err_con = NULL;
	}
}

void ImporterContext::customer(const std::string &customer, bool update_process_title)
{
	this->m_customer = customer;
	if (update_process_title)
	{
		this->updateProcessTitle();
	}
}
void ImporterContext::action(const std::string &action, bool update_process_title)
{
	this->m_action = action;
	if (update_process_title)
	{
		this->updateProcessTitle();
	}
}

void ImporterContext::logError(const std::exception &e, const std::string &source, int line)
{
	logger.error() << typeid(e).name() << " - " << e.what() << std::endl << "LOCATION: " << source << ":" << line << std::endl;
	if (!options.pgadb.empty())
	{
		try
		{
			Db::ResultSet rs;
			Db::PreparedStatement stmt;
			if (this->m_err_con.isNull())
			{
				this->m_err_con = new Db::PQConnection(options.pgadb);
				this->m_last_error_count = 0;
			}
			stmt = this->m_err_con->prepare("SELECT pganalytics.log_import_error(customer := ?, action := ?, errcode := ?, message := ?, source := ?, line := ?)");
			stmt->bind(0, this->m_customer);
			stmt->bind(1, this->m_action);
			stmt->bind(2, std::string(typeid(e).name()));
			stmt->bind(3, std::string(e.what()));
			stmt->bind(4, source);
			stmt->bind(5, line);
			rs = stmt->execQuery();
			(void)rs;
		}
		catch (std::exception &e)
		{
			logger.log() << "Error while logging previous message: " << e.what() << std::endl;
		}
	}
}

DECLARE_SMART_CLASS(ImporterOutputHandler)
class ImporterOutputHandler : public SmartObject
{
public:
	virtual void execute(const std::string &stmt) = 0;
	virtual void callFunction(const std::string &stmt) = 0;
	virtual void copyBegin(const std::string &stmt) = 0;
	virtual void copyPutLine(const std::string &line) = 0;
	/* virtual void copyPutBuffer(const char *buffer, size_t n) = 0; */
	virtual void copyEnd() = 0;
	virtual std::string quoteString(const std::string &source) = 0;
	virtual void begin() = 0;
	virtual void commit() = 0;
	virtual void rollback() = 0;
	virtual std::string nullValue() const
	{
		return "NULL";
	}
};

class ImporterToStream : public ImporterOutputHandler
{
protected:
	std::ostream &m_ostr;
public:
	ImporterToStream(std::ostream &ostr)
		: m_ostr(ostr)
	{}
	void execute(const std::string &stmt)
	{
		this->m_ostr << stmt << std::endl;
	}
	void callFunction(const std::string &stmt)
	{
		this->execute("SELECT " + stmt);
	}
	void copyBegin(const std::string &stmt)
	{
		this->m_ostr << stmt << std::endl;
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
	void begin()
	{
		this->execute("BEGIN;");
	}
	void commit()
	{
		this->execute("COMMIT;");
	}
	void rollback()
	{
		this->execute("ROLLBACK;");
	}
};

class ImporterToDatabase : public ImporterOutputHandler
{
protected:
	pga::Db::Connection m_cn;
	pga::Db::CopyInputStream m_copy_in;
	std::string m_conninfo;
	/* Auxiliar functions */
	inline bool is_open()
	{
		return !(this->m_cn.isNull());
	}
	inline void force_opened_conn(const std::string &operation)
	{
		if (!this->is_open())
		{
			throw ConnectionClosedException("connection not opened for operation \"" + operation + "\"");
		}
	}
	inline void open()
	{
		this->m_cn = new Db::PQConnection(this->m_conninfo);
	}
	inline void close()
	{
		this->m_cn = NULL;
	}
public:
	ImporterToDatabase(const std::string conninfo)
		: m_conninfo(conninfo)
	{}
	virtual ~ImporterToDatabase()
	{
		if (this->is_open())
		{
			this->close();
		}
	}
	void execute(const std::string &stmt)
	{
		this->force_opened_conn(__func__);
		this->m_cn->execCommand(stmt);
	}
	void callFunction(const std::string &stmt)
	{
		this->force_opened_conn(__func__);
		pga::Db::ResultSet rs = this->m_cn->execQuery("SELECT " + stmt);
		(void)rs;
	}
	void copyBegin(const std::string &stmt)
	{
		this->force_opened_conn(__func__);
		this->m_copy_in = this->m_cn->openCopyInputStream(stmt);
	}
	void copyPutLine(const std::string &line)
	{
		this->force_opened_conn(__func__);
		this->m_copy_in->putLine(line);
	}
	void copyEnd()
	{
		this->force_opened_conn(__func__);
		this->m_copy_in->end();
		this->m_copy_in = NULL;
	}
	std::string quoteString(const std::string &source)
	{
		this->force_opened_conn(__func__);
		return this->m_cn->quoteString(source);
	}
	void begin()
	{
		this->open();
		this->m_cn->begin();
	}
	void commit()
	{
		this->force_opened_conn(__func__);
		this->m_cn->commit();
		this->close();
	}
	void rollback()
	{
		this->force_opened_conn(__func__);
		this->m_cn->rollback();
		this->close();
	}
};

struct cnt_reader
{
	std::istream &m_istr;
	size_t m_line;
	std::string m_buffer;
	cnt_reader(std::istream &istr)
		: m_istr(istr), m_line(0)
	{}
	std::istream &nextline()
	{
		if (std::getline(this->m_istr, this->m_buffer))
		{
			/* Remove trailing \r (so it can handle files from Windows) */
			if (!this->m_buffer.empty() && this->m_buffer[this->m_buffer.size()-1] == '\r')
			{
				this->m_buffer.resize(this->m_buffer.size() - 1);
			}
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
	inline const std::string &str() const
	{
		return this->m_buffer;
	}
};

class ImporterLogLineHandler : public LineHandler
{
protected:
	ImporterOutputHandler &m_out;
public:
	ImporterLogLineHandler(ImporterOutputHandler &out)
		: m_out(out)
	{}
	virtual ~ImporterLogLineHandler() {}
	inline void handleLine(const std::string &line)
	{
		this->m_out.copyPutLine(line);
	}
};

void sig_handler(int signum)
{
	printf("Received signal %d\n", signum);
	options.last_signal = signum;
	options.signaled = true;
	if (options.in_master)
	{
		/* Do nothing on master, the main-loop must handle it */
		return;
	}
	switch (signum)
	{
	case SIGTERM:
		printf("Scheduling the end after the current batch\n");
		options.batch_loop = false;
		break;
	case SIGINT:
		printf("Scheduling the end after the current file\n");
		options.batch_loop = false;
		options.batch_cancel_next_file = true;
		break;
	case SIGQUIT:
		printf("Exiting now...\n");
		exit(0);
		break;
	case SIGHUP:
		/* Do nothing, it is just to cancel the sleep task (if any) */
		break;
	}
}

inline void check_safety(const std::string &sql)
{
	/* Safe for me means, has no ";". We only need that now. */
	if (!(sql.find_first_of(";") == std::string::npos))
	{
		throw UnsafeCommandException("Not safe string: \"" + sql + "\"");
	}
}

inline void process_copy(ImporterOutputHandler &out, cnt_reader &istr, const std::string &command, std::string table, const std::string &columns)
{
	size_t n;
	std::ostringstream sql;
	/* check if safe */
	check_safety(table);
	check_safety(columns);
	/* TODO: Remove this hack, after proper catalog upgrade to the new version */
	struct rename_tables_t
	{
		const char *old_name;
		const char *new_name;
	};
	rename_tables_t rename_tables[] =
	{
		{"sn_stat_user_tables"     , "sn_stat_all_tables"},
		{"sn_stat_user_indexes"    , "sn_stat_all_indexes"},
		{"sn_statio_user_tables"   , "sn_statio_all_tables"},
		{"sn_statio_user_indexes"  , "sn_statio_all_indexes"},
		{"sn_statio_user_sequences", "sn_statio_all_sequences"}
	};
	for (size_t i = 0; i < lengthof(rename_tables); i++)
	{
		rename_tables_t *t = &(rename_tables[i]);
		if (table == t->new_name)
		{
			table = t->old_name;
			break;
		}
	}
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
	for (n = 0; istr.nextline(); n++)
	{
		if (istr.str() == "\\.")
		{
			break;
		}
		else if (command == COMMAND_COPY_KEY_VALUE)
		{
			const std::string &line = istr.str();
			std::string key;
			std::string value;
			size_t key_pos = line.find_first_of("\t");
			if (key_pos == std::string::npos)
			{
				throw KeyNotFoundException(std::string("key not found at line: ") + line);
			}
			key = line.substr(0, key_pos + 1); /* OBS: +1 to keep the tab character */
			for (size_t i = key_pos + 1; i < line.size(); i++)
			{
				if (line[i] == '\\')
				{
					value += "\\\\";
				}
				else
				{
					value += line[i];
				}
			}
			out.copyPutLine(key + value); /* OBS: key has the tab character already */
		}
		else
		{
			out.copyPutLine(istr.str());
		}
	}
	out.copyEnd();
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
		throw ParserException("invalid meta-data");
	}
	if (key[key.size()-1] == ':')
	{
		key.resize(key.size() - 1);
	}
	std::getline(istr, value); /* Doesn't matter if empty, value can be empty */
	meta[key] = Util::unescapeString(value);
}

void start_snapshot(ImporterOutputHandler &out, std::map<std::string, std::string> &meta, const std::string &filename)
{
	
	logger.log() << "Starting snapshot" << std::endl;
	for (std::map<std::string, std::string>::iterator it = meta.begin(); it != meta.end(); it++) {
		logger.log() << std::string("meta[\"" + it->first + "\"] = \"" + it->second + "\";") << std::endl;
	}
	
	/* Validate customer_name */
	std::string customer_name = meta["customer_name"];
	bool customer_name_valid = !customer_name.empty();
	for (size_t i = 0; i < customer_name.size(); i++)
	{
		if ((customer_name[i] < 'a' || customer_name[i] > 'z') && customer_name[i] != '_')
		{
			customer_name_valid = false;
			break;
		}
	}
	if (!customer_name_valid)
	{
		throw ParserException("Invalid customer name: \"" + customer_name + "\"");
	}
	out.begin();
	out.execute("SET LOCAL ROLE " + out.quoteString("ctm_" + customer_name + "_imp") + ";");
	out.execute("SET search_path TO " + out.quoteString("ctm_" + customer_name) + ", 'pganalytics', 'public';");
	if (meta["snap_type"] != "sysstat")
	{
		std::ostringstream sql;
		sql << "sn_import_snapshot("
			<< "snap_type     := " << out.quoteString(meta["snap_type"]) << ", "
			<< "customer_name := " << out.quoteString(customer_name) << ", "
			<< "server_name   := " << out.quoteString(meta["server_name"]) << ", "
			<< "snap_hash     := " << (filename.empty() ? out.nullValue() : out.quoteString(filename)) << ", "
			<< "datetime      := to_timestamp(float " << out.quoteString(meta["datetime"]) << "), "
			<< "real_datetime := to_timestamp(float " << out.quoteString(meta["real_datetime"]) << "), "
			<< "instance_name := " << (meta.find("instance_name") == meta.end() ? "NULL" : out.quoteString(meta["instance_name"])) << ", "
			<< "datname       := " << (meta.find("datname") == meta.end() ? "NULL" : out.quoteString(meta["datname"])) << " "
			<< ");";
		out.callFunction(sql.str());
	}
}

void finish_snapshot(ImporterOutputHandler &out, std::map<std::string, std::string> &meta, const std::string &filename)
{
	if (meta["snap_type"] == "sysstat")
	{
		std::ostringstream sql;
		sql << "sn_sysstat_import("
			<< "p_customer_name := " << out.quoteString(meta["customer_name"]) << ", "
			<< "p_server_name   := " << out.quoteString(meta["server_name"]) << " "
			<< ");";
		out.callFunction(sql.str());
	}
	else
	{
		out.callFunction("sn_stat_snapshot_finish(currval(pg_get_serial_sequence('sn_stat_snapshot', 'snap_id'))::integer);");
	}
	{ /* INSERT in the logs */
		std::ostringstream sql;
		sql << "INSERT INTO log_imports(customer_id, server_id, instance_id, database_id, snap_hash, start_time, end_time, snap_type)\n"
			<< "SELECT s.customer_id, s.server_id, s.instance_id, s.database_id, "
			<< (filename.empty() ? out.nullValue() : out.quoteString(filename)) << ", "
			<< "now(), " /* begin of the transaction */
			<< "clock_timestamp(), " /* real current time */
			<< out.quoteString(meta["snap_type"]) << "\n"
			<< "FROM sn_stat_snapshot s\n"
			<< "WHERE s.snap_id = currval(pg_get_serial_sequence('sn_stat_snapshot', 'snap_id'))::integer"
			;
		out.execute(sql.str());
	}
	if (options.check_only)
	{
		out.rollback();
	}
	else
	{
		out.commit();
	}
}

void rollback_snapshot(ImporterOutputHandler &out, std::map<std::string, std::string> &meta, const std::string &filename)
{
	out.rollback();
}

void do_sql_snapshot(ImporterOutputHandler &out, cnt_reader &istr)
{
	do
	{
		const std::string &line = istr.str();
		std::istringstream iss_params(line);
		std::string command;
		iss_params >> command;
		if (iss_params)
		{
			if ((command == COMMAND_COPY_TAB_DELIMITED) || (command == COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE) || (command == COMMAND_COPY_KEY_VALUE))
			{
				std::string table;
				std::string columns;
				iss_params >> table >> columns;
				if (!iss_params)
				{
					std::ostringstream err;
					err << "Invalid parameters at line " << istr.line();
					throw ParserException(err.str());
				}
				process_copy(out, istr, command, table, columns);
			}
			else if (command[0] == '-' && command[1] == '-')
			{
				/* A comment */
				logger.log() << line << std::endl;
			}
			else
			{
				std::ostringstream err;
				err << "Unknown command \"" << command << "\" at line " << istr.line() << ": " << istr.str();
				throw ParserException(err.str());
			}
		}
	}
	while (istr.nextline());
}

void do_log_snapshot(ImporterOutputHandler &out, cnt_reader &istr, std::map<std::string, std::string> &meta)
{
	ImporterLogLineHandler handler(out);
	LogParser parser(handler);
	std::string line;
	parser.generateLineParserRE(meta["llp"]);
	out.copyBegin(
		"COPY sn_pglog("
		"application_name,"
		"user_name,"
		"database_name,"
		"remote_host_port,"
		"remote_host,"
		"pid,"
		"log_time,"
		"log_time_ms,"
		"command_tag,"
		"sql_state_code,"
		"session_id,"
		"session_line_num,"
		"session_start_time,"
		"virtual_transaction_id,"
		"transaction_id,"
		"error_severity,"
		"message,"
		"normalized"
		") FROM stdin;"
	);
	do
	{
		if (!istr.str().empty())
		{
			std::string copyline = istr.str();
			parser.consume(copyline);
		}
	}
	while(istr.nextline());
	parser.finalize();
	out.copyEnd();
}

void do_snapshot(ImporterOutputHandler &out, cnt_reader &istr, const std::string &filename)
{
	std::map<std::string, std::string> meta;
	bool reading_meta_data = true;
	while (reading_meta_data && istr.nextline())
	{
		const std::string &line = istr.str();
		//logger.log() << "line: " << line << std::endl;
		if (line[0] != '#')
		{
			reading_meta_data = false;
		}
		else
		{
			add_to_metadata(meta, line);
		}
	}
	if (reading_meta_data)
	{
		throw NoDataFoundException();
	}
	try
	{
		start_snapshot(out, meta, filename);
		if (meta["snap_type"] != "pg_log")
		{
			do_sql_snapshot(out, istr);
		}
		else
		{
			do_log_snapshot(out, istr, meta);
		}
		finish_snapshot(out, meta, filename);
	}
	catch (std::exception &e)
	{
		logger.log() << "Error: " << typeid(e).name() << ": " << e.what() << std::endl;
		rollback_snapshot(out, meta, filename);
		throw; /* rethrow to the caller */
	}
}

void process_bucket(ImporterOutputHandler &out, aws::S3ConnectionPtr s3conn, const std::string &bucketname, int &processed_items)
{
	/* TODO: Lock for concurrent process of the same bucket */
	logger.log() << "Processing bucket `" << bucketname << "'" << std::endl;
	context.action("s3 ls " + bucketname, true);
	aws::ListBucketResponsePtr ls;
	aws::ListBucketResponse::Object item;
	int max_files;
	/* Verify max files to process in this request */
	if (options.max_files_all > 0 && options.max_files_per_bucket > 0)
	{
		max_files = std::min(options.max_files_per_bucket, options.max_files_all - processed_items);
	}
	else if (options.max_files_all > 0)
	{
		max_files = options.max_files_all;
	}
	else if (options.max_files_per_bucket > 0)
	{
		max_files = options.max_files_per_bucket;
	}
	else
	{
		max_files = -1;
	}
	/* Loop to retry after failures */
	for (int connection_try_loop = 0; connection_try_loop >= 0 && connection_try_loop <= 10; connection_try_loop++)
	{
		if (connection_try_loop)
		{
			if (connection_try_loop == 10)
			{
				logger.error() << Util::time::now() << " - Some error found (see above)! Tried " << connection_try_loop << " times and didn't work... giving up" << std::endl;
				break; /* Get out */
			}
			if (options.batch_cancel_next_file)
			{
				break;
			}
			Util::time::sleep(500 * connection_try_loop); /* sleep for half second times number of tries */
			if (options.batch_cancel_next_file)
			{
				break;
			}
			logger.log() << Util::time::now() << " - Some error found (see above)! Trying process for " << connection_try_loop << "th time" << std::endl;
		}
		try
		{
			ls = s3conn->listBucket(bucketname, std::string(S3_NEW_FILES_DIR) + "/", "", "", max_files);
			ls->open();
			while (ls->next(item) && !options.batch_cancel_next_file)
			{
				std::stringstream message;
				Util::io::gunzipstream gunzip_in;
				std::istream *instr;
				std::string basename;
				std::string filename;
				bool import_success = true;
				if (item.KeyValue == "new/") {
					/* "new/" happens a lot, so just ignore it silently (to not flood the logs) */
					continue;
				}
				logger.log() << "Got key from S3: " + item.KeyValue << std::endl;
				if (Util::fs::fileExtension(item.KeyValue) != "pga")
				{
					logger.log() << "Invalid extension of file `" << item.KeyValue << "', ignoring..." << std::endl;
					continue;
				}
				context.action("processing s3://" + bucketname + "/" + item.KeyValue, true);
				basename = Util::fs::baseName(item.KeyValue);
				filename = "s3://" + bucketname + "/" + std::string(S3_DATA_DIR) + "/" + basename;
				/* Read the log file (should be small enough to fit in memory ) */
				{
					size_t log_size = 0;
					/* Get old reference information */
					aws::GetResponsePtr s3get = s3conn->get(bucketname, std::string(S3_NEW_FILES_DIR) + "/" + basename);
					std::istream &ref_in = s3get->getInputStream();
					char buffer[8192];
					while (ref_in)
					{
						ref_in.read(buffer, 8192);
						if (ref_in.gcount())
						{
							message.write(buffer, ref_in.gcount());
							log_size += ref_in.gcount();
							if (log_size > MAX_S3_QUEUE_LOG_FILE_SIZE)
							{
								/* Limit the memory to read log */
								logger.log() << "MAX_S3_QUEUE_LOG_FILE_SIZE reached: " << log_size << " bytes only read" << std::endl;
								message << std::endl; /* Forcibly add the last line break */
								break;
							}
						}
					}
				}
				message << Util::time::now() << " - starting file process" << std::endl;
				/* Read and process the data file */
				try
				{
					aws::GetResponsePtr s3get = s3conn->get(bucketname, std::string(S3_DATA_DIR) + "/" + basename);
					context.action("processing " + filename, true);
					instr = &(s3get->getInputStream());
					if (options.format == "gzip")
					{
						gunzip_in.open(*instr);
						instr = &gunzip_in;
					}
					cnt_reader istr(*instr);
					do_snapshot(out, istr, filename);
				}
				catch (ImporterException &e)
				{
					import_success = false;
					LOG_ERROR(e);
				}
				catch (Db::Exception &e)
				{
					import_success = false;
					LOG_ERROR(e);
				}
				if (import_success)
				{
					/* It succeeded, so move the new reference to done */
					context.action("commiting " + filename, true);
					message << Util::time::now() << " - file process succeeded" << std::endl;
					if (options.check_only)
					{
						logger.log() << "Processing of " << basename << " succeeded, should move this to \"done/\" now: " << std::endl;
						logger.log() << message.str() << std::endl;
					}
					else
					{
						/* Processed successfully, so move the reference to done */
						logger.log() << "Processing of " << basename << " succeeded! Moving to \"done/\"." << std::endl;
						/* Now, create a new reference on done and delete the old one */
						(void)s3conn->put(bucketname, std::string(S3_DONE_FILES_DIR) + "/" + basename, message, "text/plain");
						(void)s3conn->del(bucketname, std::string(S3_NEW_FILES_DIR) + "/" + basename);
					}
				}
				else
				{
					/* It failed, so move the new reference to err */
					message << Util::time::now() << " - file process FAILED" << std::endl;
					if (options.check_only)
					{
						logger.log() << "Processing of " << basename << " failed, should move this to \"err/\" now: " << std::endl;
						logger.log() << message.str() << std::endl;
					}
					else
					{
						logger.log() << "Processing of " << basename << " failed! Moving to \"err/\"." << std::endl;
						/* Now, create a new reference on err and delete the old one */
						(void)s3conn->put(bucketname, std::string(S3_ERR_FILES_DIR) + "/" + basename, message, "text/plain");
						(void)s3conn->del(bucketname, std::string(S3_NEW_FILES_DIR) + "/" + basename);
					}
				}
				processed_items++;
			}
			break; /* Everything succeed (at least no error in AWS connection), so finish the loop! */
		}
		catch (aws::AWSException &e)
		{
			LOG_ERROR(e);
		}
	}
}

void process_bucket(ImporterOutputHandler &out, aws::S3ConnectionPtr s3conn, const std::string &bucketname)
{
	static int processed_items = 0;
	process_bucket(out, s3conn, bucketname, processed_items);
}

void do_snapshot_from_s3(ImporterOutputHandler &out)
{
	/* TODO: Handle exceptions */
	aws::AWSConnectionFactory* awsfactory = aws::AWSConnectionFactory::getInstance();
	aws::S3ConnectionPtr s3conn =  awsfactory->createS3Connection(options.aws_access_key, options.aws_secret_access_key);
	if (!options.aws_path.empty())
	{
		Util::io::gunzipstream gunzip_in;
		std::istream *instr;
		/* Get one specific file */
		aws::GetResponsePtr s3get = s3conn->get(options.aws_bucket, options.aws_path);
		instr = &(s3get->getInputStream());
		if (options.format == "gzip")
		{
			gunzip_in.open(*instr);
			instr = &gunzip_in;
		}
		cnt_reader istr(*instr);
		do_snapshot(out, istr, options.input);
	}
	else if (!options.aws_bucket.empty())
	{
		/* Files from one bucket */
		do
		{
			int processed_items_count = 0;
			process_bucket(out, s3conn, options.aws_bucket, processed_items_count);
			if (options.batch_loop)
			{
				if (processed_items_count == 0 && options.batch_loop_sleep_noproc > 0)
				{
					context.sleep_noproc();
				}
				else if (options.batch_loop_sleep_interval > 0)
				{
					context.sleep_batch();
				}
			}
		}
		while(options.batch_loop);
	}
	else
	{
		std::ostringstream sql;
		sql << "SELECT c.name_id, c.s3bucket\n"
			<< "FROM pganalytics.pm_customer c\n"
			<< "WHERE c.s3bucket IS NOT NULL\n"
			<< "    AND c.is_active\n";
		if (!options.process_customer.empty())
		{
			size_t next_comma = 0;
			size_t last_comma = 0;
			bool first = true;
			sql << "    AND c.name_id IN (";
			do
			{
				if (!first)
				{
					sql << ",";
					last_comma = next_comma + 1;
				}
				else
				{
					first = false;
					last_comma = 0;
				}
				next_comma = options.process_customer.find(',', last_comma);
				if (next_comma != std::string::npos)
				{
					sql << (Db::Internal::PQFormatter::quoteString(options.process_customer.substr(last_comma, next_comma - last_comma)));
				}
				else
				{
					sql << (Db::Internal::PQFormatter::quoteString(options.process_customer.substr(last_comma)));
				}
			}
			while (next_comma != std::string::npos);
			sql << ")\n";
		}
		if (options.group_id > 0)
		{
			sql << "    AND c.customer_id IN (SELECT ig.customer_id FROM pganalytics.pm_import_group_customer ig WHERE ig.group_id = " << options.group_id << ")";
		}
		do
		{
			int processed_items_count = 0;
			std::vector<std::pair<std::string, std::string> > customers;
			context.set("none", "reading customers");
			/**
			 * Read customers.
			 * Open the connection in a small scope, so we handle forcible disconnections and also get table changes
			 */
			try
			{
				logger.log() << "Reading customers from \"" << options.pgadb << "\"" << std::endl;
				Db::Connection con = new Db::PQConnection(options.pgadb);
				Db::ResultSet rs = con->execQuery(sql.str());
				while (rs->next())
				{
					std::string s3bucket;
					std::string customer;
					rs->fetchByName("name_id", customer);
					rs->fetchByName("s3bucket", s3bucket);
					customers.push_back(std::make_pair(customer, s3bucket));
				}
			}
			catch (std::exception &e)
			{
				LOG_ERROR(e);
				Util::time::sleep(1000);
				continue;
			}
			/**
			 * Process bucket of each customer
			 */
			forall(c, customers)
			{
				context.customer(c->first);
				process_bucket(out, s3conn, c->second, processed_items_count);
				if (options.max_files_all > 0 && processed_items_count >= options.max_files_all)
				{
					break;
				}
			}
			context.set("none", "");
			/**
			 * Check sleeps
			 */
			if (options.batch_loop)
			{
				if (processed_items_count == 0 && options.batch_loop_sleep_noproc > 0)
				{
					context.sleep_noproc();
				}
				else if (options.batch_loop_sleep_interval > 0)
				{
					context.sleep_batch();
				}
			}
		}
		while(options.batch_loop);
	}
}

void PgaImporterApp::help()
{
	const char *progname = argv(0);
	version();
	std::cout
			<< "Usage:\n"
			<< "  " << progname << " [OPTIONS] { -i INPUT -o OUTPUT | -g -p CN }\n"
			<< "\n"
			<< "Required options:\n"
			<< "  -g, --groups            import based on database groups (requires --pgadb)\n"
			<< " or\n"
			<< "  -i, --input=INPUT       input file to process, INPUT can be:\n"
			<< "                          - \"stdin\": to process from standard input\n"
			<< "                          - \"file://FILE\": to process from given FILE location\n"
			<< "                          - \"customer://NAME_ID1[,NAME_ID2 ...]\": to process buckets of given customers\n"
			<< "                          - \"s3://BUCKET\": to process a S3 BUCKET\n"
			<< "                                  (OBS: for the two above, new processed will be moved to done/err)\n"
			<< "                          - \"s3://BUCKET/FILE\": to process from given FILE at S3 BUCKET\n"
			<< "                                  (OBS: new processed will *NOT* be moved to done/err)\n"
			<< "  -o, --output=OUTPUT     where to save to, OUTPUT can be:\n"
			<< "                          - \"stdout\": save at standard output\n"
			<< "                          - \"file://FILE\": save at FILE location\n"
			<< "                          - any else, consider as a database connection (e.g. \"user=ctm_imp dbname=pganalytics\")\n"
			<< "\n"
			<< "Bucket import options:\n"
			<< "  -a, --aws-access-key=KEY\n"
			<< "                          AWS Access Key ID\n"
			<< "  -s, --aws-secret-access-key=KEY\n"
			<< "                          AWS Secret Access Key\n"
			<< "  -p, --pgadb=CN          pgAnalytics database connection string\n"
			<< "  -F, --max-files=N       maximum files to process per bucket\n"
			<< "  -A, --max-all-files=N   maximum files to process over all\n"
			//<< "  -B, --max-buckets=N     maximum customer buckets to process\n"
			<< "  -l, --loop[=ON|OFF]     if given or ON, do infinity loop to process each batch of max-* (default: false)\n"
			<< "                                  (available only for s3://BUCKET and customer inputs)\n"
			<< "  -n, --loop-interval=MS  sleep every MS milliseconds after each process batch loop (default: 10000 = 10sec)\n"
			<< "  -N, --noproc-sleep=MS   if no file has been processed over all buckets, sleep for this time in milliseconds (default: 300000 = 5min)\n"
			<< "\n"
			<< "General options:\n"
			<< "  -f, --format=FORMAT     input file format, can be \"gzip\" (the default) or \"plain\"\n"
			<< "  -d, --dry-run           run everything but with ROLLBACK instead of COMMIT at the end\n"
			<< "  -v, --version           version information\n"
			<< "  -h, --help              show this help, then exit\n"
			<< "\n"
			<< "Kill signals:\n"
			<< "    SIGTERM:\n"
			<< "        Finish the current process batch (max-*) and exit (e.g. do not go back in the infinity loop, but process until a max-* is reached)\n"
			<< "    SIGINT:\n"
			<< "        Finish the current file processing and exit (e.g. do not go back in the loop)\n"
			<< "    SIGQUIT:\n"
			<< "        Exit immediately (this is actually not handled, so it is a forcibly cut-off)\n"
			<< "    SIGHUP:\n"
			<< "        Does nothing in fact, but can be used to cancel current sleep task and get to work\n"
			<< "    SIGKILL:\n"
			<< "        Oh! Really? There is nothing I can do against `kill -9`... :(\n"
			;
	std::cout.flush();
}

void PgaImporterApp::version()
{
	std::cout << "pgAnalytics importer " << APP_MAJOR_VERSION << "." << APP_MINOR_VERSION << "." << APP_PATCH_VERSION << std::endl;
}

void install_signal_handlers()
{
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);
}

int run_child()
{
	ImporterOutputHandlerPtr out_handler;
	std::ofstream fout;
	std::ifstream fin;
	Util::io::gunzipstream gunzip_in;
	std::istream *in = NULL;
	switch (options.output_mode)
	{
		case pgaimporter_options::OUTPUT_STDOUT:
			{
				out_handler = new ImporterToStream(std::cout);
			}
			break;
		case pgaimporter_options::OUTPUT_FILE:
			{
				fout.exceptions(std::ios::failbit | std::ios::badbit);
				fout.open(options.output.c_str(), std::ios::binary | std::ios::out);
				fout.exceptions(std::ios::badbit);
				out_handler = new ImporterToStream(fout);
			}
			break;
		case pgaimporter_options::OUTPUT_DATABASE:
			{
				out_handler = new ImporterToDatabase(options.output);
			}
			break;
		case pgaimporter_options::OUTPUT_INVALID:
		default:
			logger.log() << "Internal logic error, unexpected value for options.output_mode " << (int)options.output_mode << std::endl;
			return 3;
	}
	switch (options.input_mode)
	{
		case pgaimporter_options::INPUT_STDIN:
			{
				in = &(std::cin);
			}
			break;
		case pgaimporter_options::INPUT_FILE:
			{
				fin.exceptions(std::ios::failbit | std::ios::badbit);
				fin.open(options.input.c_str(), std::ios::binary | std::ios::in);
				fin.exceptions(std::ios::badbit);
				in = &fin;
				options.input = Util::fs::baseName(options.input);
			}
			break;
		case pgaimporter_options::INPUT_S3_BUCKET:
			{
				logger.log() << "Bucket: \"" << options.aws_bucket << "\", key: \"" << options.aws_path << "\"" << std::endl;
			}
			break;
		case pgaimporter_options::INPUT_CUSTOMERS:
			{
				logger.log() << "Process customer \"" << options.process_customer << "\"" << std::endl;
				if (options.pgadb.empty() || options.process_customer.empty())
				{
					logger.log() << "Internal logic error, no connection given to --pgadb or no customer name" << std::endl;
					return 2;
				}
			}
			break;
		case pgaimporter_options::INPUT_GROUP:
			{
				if (!options.group_id)
				{
					logger.log() << "Internal logic error, group import required but no group given" << std::endl;
					return 3;
				}
				logger.log() << "Process group \"" << options.group_id << std::endl;
			}
			break;
		case pgaimporter_options::INPUT_INVALID:
		default:
			logger.log() << "Internal logic error, unexpected value for options.input_mode " << (int)options.input_mode << std::endl;
			return 3;
	}
	if (in && options.format == "gzip")
	{
		gunzip_in.open(*in);
		in = &gunzip_in;
	}
	/* Install signal handlers */
	install_signal_handlers();
	try
	{
		if (out_handler.isNull())
		{
			logger.log() << "Invalid output option" << std::endl;
			return 1;
		}
		else if (options.input_mode == pgaimporter_options::INPUT_S3_BUCKET || options.input_mode == pgaimporter_options::INPUT_CUSTOMERS || options.input_mode == pgaimporter_options::INPUT_GROUP)
		{
			if (options.aws_access_key.empty())
			{
				logger.log() << "AWS Access Key not given" << std::endl;
				return 1;
			}
			else if (options.aws_secret_access_key.empty())
			{
				logger.log() << "AWS Secrete Access Key not given" << std::endl;
				return 1;
			}
			else
			{
				do_snapshot_from_s3(*out_handler);
			}
		}
		else if (!in)
		{
			logger.log() << "Invalid input option" << std::endl;
			return 1;
		}
		else
		{
			cnt_reader istr(*in);
			do_snapshot(*out_handler, istr, options.input);
		}
	}
	catch (ImporterException &e)
	{
		LOG_ERROR(e);
		return 2;
	}
	catch (Db::Exception &e)
	{
		LOG_ERROR(e);
		return 2;
	}
	catch (std::exception &e)
	{
		logger.log() << "Unexpected exception!" << std::endl;
		LOG_ERROR(e);
		return 10;
	}
	return 0;
}

int run_parent()
{
	std::vector<groups_info_t> children;
	if (options.pgadb.empty())
	{
		logger.log() << "No connection given to --pgadb!" << std::endl;
		return 2;
	}
	options.in_master = true;
	/* Collect groups information */
	{
		Db::Connection con = new Db::PQConnection(options.pgadb);
		Db::ResultSet rs = con->execQuery(
			"SELECT group_id, aws_access_key, aws_secret_access_key, client_encoding "
			"FROM pganalytics.pm_import_group "
			"ORDER BY group_id"
			);
		while (rs->next())
		{
			groups_info_t g;
			g.child_options = options; /* Copy all options from parent */
			rs->fetchByName("group_id", g.group_id);
			rs->fetchByName("aws_access_key", g.child_options.aws_access_key);
			rs->fetchByName("aws_secret_access_key", g.child_options.aws_secret_access_key);
			rs->fetchByName("client_encoding", g.client_encoding);
			children.push_back(g);
		}
	}
	/* Fork for each group */
	forall(it, children)
	{
		pid_t p;
		/* Build remaining options */
		it->child_options.input_mode = pgaimporter_options::INPUT_GROUP;
		it->child_options.output_mode = pgaimporter_options::OUTPUT_DATABASE;
		it->child_options.output = options.output + " user=ctm_importer client_encoding=" + it->client_encoding;
		it->child_options.group_id = it->group_id;
		it->child_options.batch_loop = true;
		it->child_options.in_master = false;
		p = ::fork();
		/* If in parent */
		if (p)
		{
			it->child_pid = p;
			logger.log() << "Created " << p << " to handle group " << it->group_id << std::endl;
		}
		/* If in child */
		else
		{
			options = it->child_options;
			return run_child();
		}
	}
	install_signal_handlers();
	/* main-loop */
	while (true)
	{
		pid_t p;
		int status;
		p = ::waitpid(-1, &status, WNOHANG);
		if (p != 0)
		{
			std::ostringstream ostr;
			ostr << "Child " << p << " died unexpectedly with code " << WEXITSTATUS(status);
			LOG_ERROR(std::runtime_error(ostr.str()));
		}
		if (options.signaled)
		{
			/* Master was signaled, propagate it */
			forall (it, children)
			{
				if (::kill(it->child_pid, options.last_signal) != 0)
				{
					std::ostringstream ostr;
					ostr << "Could not signal " << it->child_pid << ": " << ::strerror(errno);
					LOG_ERROR(std::runtime_error(ostr.str()));
				}
			}
			options.signaled = false;
			if (options.last_signal == SIGQUIT || options.last_signal == SIGINT || options.last_signal == SIGTERM)
			{
				int ret = 0;
				/* Wait for all children to exit */
				errno = 0;
				while ((p = ::wait(&status)) > 0)
				{
					logger.log() << "Child with pid " << p << " finished" << std::endl;
					ret += WEXITSTATUS(status);
				}
				if (p == -1 && errno != ECHILD)
				{
					std::ostringstream ostr;
					ostr << "Wait failed with error: " << ::strerror(errno);
					LOG_ERROR(std::runtime_error(ostr.str()));
					ret += 1;
				}
				return ret;
			}
		}
		/* Check for children each minute */
		Util::time::sleep(60000);
	}
}


int PgaImporterApp::main()
{
	static struct option long_options[] =
	{
		{"dry-run", no_argument, NULL, 'd'},
		{"groups", no_argument, NULL, 'g'},
		{"input", required_argument, NULL, 'i'},
		{"output", required_argument, NULL, 'o'},
		{"aws-access-key", required_argument, NULL, 'a'},
		{"aws-secret-access-key", required_argument, NULL, 's'},
		{"pgadb", required_argument, NULL, 'p'},
		{"format", required_argument, NULL, 'f'},
		{"max-files", required_argument, NULL, 'F'},
		{"max-buckets", required_argument, NULL, 'B'},
		{"max-all-files", required_argument, NULL, 'A'},
		{"loop", optional_argument, NULL, 'l'},
		{"loop-interval", required_argument, NULL, 'n'},
		{"noproc-sleep", required_argument, NULL, 'N'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	char c;
	context.app(this);
	logger.addSink(std::cerr, Util::log::L_DEBUG);
	while ((c = getopt(long_options)) != -1)
	{
		switch (c)
		{
		case 'd':
			options.check_only = true;
			break;
		case 'g':
			options.group_import = true;
			break;
		case 'o':
			options.output = this->optarg();
			break;
		case 'i':
			options.input = this->optarg();
			break;
		case 'p':
			options.pgadb = this->optarg();
			break;
		case 'a':
			options.aws_access_key = this->optarg();
			break;
		case 's':
			options.aws_secret_access_key = this->optarg();
			break;
		case 'f':
			options.format = this->optarg();
			break;
		case 'F':
			options.max_files_per_bucket = Util::stringToNumber<int>(this->optarg());
			break;
		case 'B':
			options.max_buckets = Util::stringToNumber<int>(this->optarg());
			break;
		case 'A':
			options.max_files_all = Util::stringToNumber<int>(this->optarg());
			break;
		case 'l':
			if (this->optarg())
			{
				if (::strcmp(this->optarg(), "ON") == 0)
				{
					options.batch_loop = true;
				}
				else if (::strcmp(this->optarg(), "OFF") == 0)
				{
					options.batch_loop = false;
				}
				else
				{
					logger.log() << "Invalid option `" << this->optarg() << "' for --loop. Valid options are ON or OFF (case-sensitive)" << std::endl;
					return 2;
				}
			}
			else
			{
				options.batch_loop = true;
			}
			break;
		case 'n':
			options.batch_loop_sleep_interval = Util::stringToNumber<size_t>(this->optarg());
			break;
		case 'N':
			options.batch_loop_sleep_noproc = Util::stringToNumber<size_t>(this->optarg());
			break;
		case 'v':
			version();
			return 0;
		case 'h':
			help();
			return 0;
		default:
			logger.log() << "Unknown option '" << c << "'" << std::endl;
			return 2;
		}
	}
	/* Validate some options values */
	if (!options.group_import)
	{
		if (options.format != "gzip" && options.format != "plain")
		{
			logger.log() << "Invalid format \"" << options.format << "\"" << std::endl;
		}
		else
		{
			/* Process output */
			if (options.output.empty())
			{
				logger.log() << "No output option" << std::endl;
			}
			else
			{
				if (options.output == "stdout")
				{
					options.output_mode = pgaimporter_options::OUTPUT_STDOUT;
				}
				else if (options.output.find("file://") == 0)
				{
					options.output_mode = pgaimporter_options::OUTPUT_FILE;
					options.output = options.output.substr(7);
				}
				else
				{
					options.output_mode = pgaimporter_options::OUTPUT_DATABASE;
				}
			}
			/* Process input */
			if (options.input.empty())
			{
				logger.log() << "No input option" << std::endl;
			}
			else
			{
				if (options.input == "stdin")
				{
					options.input_mode = pgaimporter_options::INPUT_STDIN;
					options.filename = "";
				}
				else if (options.input.find("file://") == 0)
				{
					options.input_mode = pgaimporter_options::INPUT_FILE;
					options.input = options.input.substr(7);
				}
				else if (options.input.find("s3://") == 0)
				{
					options.input_mode = pgaimporter_options::INPUT_S3_BUCKET;
					size_t pos = options.input.find_first_of("/", 5);
					if (pos == std::string::npos)
					{
						pos = options.input.size();
					}
					options.aws_bucket = options.input.substr(5, pos - 5);
					options.aws_path = options.input.substr(options.aws_bucket.size() + 6);
				}
				else if (options.input.find("customer://") == 0)
				{
					options.input_mode = pgaimporter_options::INPUT_CUSTOMERS;
					options.process_customer = options.input.substr(11);
					if (options.pgadb.empty() || options.process_customer.empty())
					{
						logger.log() << "No connection given to --pgadb or no customer name" << std::endl;
						return 2;
					}
				}
			}
		}
		return run_child();
	}
	else
	{
		return run_parent();
	}
}

int PgaImporterApp::importStream(std::istream &in, std::ostream &out, const std::string &filename, bool input_gziped)
{
	ImporterOutputHandlerPtr out_handler = new ImporterToStream(out);
	cnt_reader istr(in);
	options.input_mode = pgaimporter_options::INPUT_FILE;
	options.input = filename;
	options.output_mode = pgaimporter_options::OUTPUT_FILE;
	options.format = (input_gziped ? "gzip" : "plain");
	do_snapshot(*out_handler, istr, options.input);
	return 0;
}

END_APP_NAMESPACE

