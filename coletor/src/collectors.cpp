#include <sstream>
#include <iomanip>
#include <ctime>

#include "collectors.h"
#include "StorageManager.h"
#include "ServerInfo.h"
#include "debug.h"
#include "UserConfig.h"
#include "DbConnection.h"
#include "util/fs.h"
#include "db/pq.h"
#include "util/string.h"
#include "util/streams.h"
#include "util/fs.h"
#include "util/app.h"
#include "util/time.h"
#include "StateManager.h"
#include "common.h"
#include "backend_shared.h"

#define PG_VERSION(x,y) ((x)*100 + (y))*100
#define PG_VERSION_8_0 PG_VERSION(8,0)
#define PG_VERSION_8_2 PG_VERSION(8,2)
#define PG_VERSION_8_3 PG_VERSION(8,3)
#define PG_VERSION_8_4 PG_VERSION(8,4)
#define PG_VERSION_9_0 PG_VERSION(9,0)
#define PG_VERSION_9_1 PG_VERSION(9,1)
#define PG_VERSION_9_2 PG_VERSION(9,2)
#define PG_VERSION_9_3 PG_VERSION(9,3)
#define PG_VERSION_9_4 PG_VERSION(9,4)

BEGIN_APP_NAMESPACE

namespace CollectorPrivate
{

void startSnapshot(StorageManager &storage, const std::string &snap_type, InstanceConfigPtr instance = NULL, const std::string &dbname = "")
{
	ServerInfoPtr server_info = ServerInfo::instance();
	storage.stream()
			<< "# snap_type " << Util::escapeString(snap_type) << "\n"
			<< "# customer_name " << Util::escapeString(server_info->currentServerConfig()->userConfig()->customer()) << "\n"
			<< "# server_name " << Util::escapeString(server_info->currentServerConfig()->hostname()) << "\n"
			<< "# datetime " << Util::MainApplication::instance()->startTime() << "\n"
			<< "# real_datetime " << Util::time::now() << "\n"
			;
	if (!instance.isNull())
	{
		storage.stream() << "# instance_name " << Util::escapeString(instance->name()) << "\n";
	}
	if (!dbname.empty())
	{
		storage.stream() << "# datname " << Util::escapeString(dbname) << "\n";
	}
	storage.stream() << "\n";
	/*
	storage.stream()
		<< COMMAND_COPY_TAB_DELIMITED << " sn_import_snapshot snap_type,customer_name,server_name,instance_name,datname,datetime,real_datetime\n";
	storage.stream()
		<< Db::Internal::PQFormatter::copyValue(snap_type)
		<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(server_info->currentServerConfig()->userConfig()->customer())
		<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(server_info->currentServerConfig()->hostname())
		<< Db::Internal::PQFormatter::copyFieldSeparator() <<
			(instance.isNull() ? Db::Internal::PQFormatter::copyNullValue() : Db::Internal::PQFormatter::copyValue(instance->name()))
		<< Db::Internal::PQFormatter::copyFieldSeparator() <<
			(dbname.empty() ? Db::Internal::PQFormatter::copyNullValue() : Db::Internal::PQFormatter::copyValue(dbname))
		<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(Util::timeToString(Util::MainApplication::instance()->startTime()))
		<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(Util::timeToString(Util::time::now()))
		<< Db::Internal::PQFormatter::copyRecordSeparator();
	storage.stream() << "\\.\n";
	*/
}

} // namespace CollectorPrivate

namespace PgStatsCollectorPrivate
{
/**
 * @brief add columns of given table to output stream for COPY
 *
 * @param con
 * @param table_name
 * @param ostr
 */
static void addCopyColumnNames(Db::Connection con, std::string &table_name, std::ostream &ostr);

/**
 * @brief Add COPY value of file system device as returned by `df spclocation`
 *
 * @param ostr						output stream to feed the fsdevice to
 * @param con						connection used to escape copy value (using Db::Connection::copyValue)
 * @param server_info				the current server_info to be used to grab fsdevice information
 * @param data_directory			the PGDATA location
 * @param spcname					the name of the tablespace
 * @param spclocation				the tablespace location
 * @param save_spclocation_abs		if the caller wants the absolute spclocation, it must give this pointer to save it
 */
static void addSpcFsDeviceToCopy(std::ostream &ostr, Db::Connection con, ServerInfoPtr server_info, const std::string &data_directory, const std::string &spcname, const std::string &spclocation, std::string *save_spclocation_abs = NULL);

/**
 * @brief This is a magic function that gets a given query and stream it as:
 *        COPY <target_table> ( <alias of columns>, ... ) FROM stdin;
 *        <the COPY data>
 *        \.
 *
 * @param con				the connection where the COPY will be performed from
 * @param ostr				the ostream where the result will be feed to
 * @param target_table		the name of the table for COPY...FROM to use
 * @param query				the query used to generate the data and column names
 *        					(the latter is based on the resulting aliases)
 */
static void addQueryAsCopyFrom(Db::Connection con, std::ostream &ostr, const std::string &target_table, const std::string &query);

/**
 * @brief Collect PostgreSQL statistics of every database of an instance
 *        given by `config_instance`
 *
 * @param config_instance	the current instance to dump the statistics
 */
static void dumpAllDatabases(const InstanceConfigPtr config_instance);

static bool getDbConnInformation(
	/* IN: */
	Db::Connection con,
	/* OUT: */
	std::string &datname,
	std::string &datetime,
	int &version_num,
	std::string &version,
	std::string &data_directory
	);

static std::string sqlCalcXlogPosFor9_1(const std::string &lsn_expr);
static std::string sqlFuncXlogLocationDiff(int version_num, const std::string &lsn_expr1, const std::string &lsn_expr2);

/**
 * @brief Collect PostgreSQL statistics of the instance `config_instance` and
 *        only global objects (tablespaces, checkpoints, archives, xlog, etc.),
 *        use `dbname` to connect in the instance
 *
 * @param config_instance	the current instance to dump the statistics
 * @param dbname			the name of the database to connect
 */
static void dumpGlobalObjects(const InstanceConfigPtr config_instance, const std::string &dbname);

/**
 * @brief Collect PostgreSQL statistics of the instance `config_instance`
 *        and only database `dbname`
 *
 * @param config_instance	the current instance to dump the statistics
 * @param dbname			the name of the database to dump the statistics
 */
static void dumpDatabase(const InstanceConfigPtr config_instance, const std::string &dbname);

/********** PgStatsCollectorPrivate functions implementation **********/

/* We are not using this function now, but it works fine and may be helpful in the future */
void IGNORE_UNUSED_WARN addCopyColumnNames(Db::Connection con, std::string &table_name, std::ostream &ostr)
{
	Db::ResultSet rs;
	Db::PreparedStatement st;
	std::string ret;
	/* XXX: Can't use string_agg because ORDER BY inside agg functions only work after 9.1 */
	st = con->prepare(
			 "SELECT pg_catalog.quote_ident(attname) "
			 " FROM pg_attribute "
			 " WHERE attrelid = ((?)::text)::regclass "
			 "       AND NOT attisdropped "
			 " ORDER BY attnum"
		 );
	st->bind(0u, table_name);
	rs = st->execQuery();
	/* Construct "COPY sn_<table>(<columns>, ...) FROM stdin;\n" */
	for (bool first = true; rs->next(); first = false)
	{
		std::string col;
		rs->fetch(0u, col);
		if (!first)
		{
			ostr << ",";
		}
		ostr << col;
	}
}

void addSpcFsDeviceToCopy(std::ostream &ostr, Db::Connection con, ServerInfoPtr server_info, const std::string &data_directory, const std::string &spcname, const std::string &spclocation, std::string *save_spclocation_abs)
{
	std::string spclocation_abs;
	bool got_abs_location = true;
	try
	{
		/**
		 * Look for the absolute location of each tablespace, that way
		 * we can guarantee that the directory exists, and that we can
		 * call df (by ServerInfo::diskUsageForPath()) to retrieve the
		 * filesystem
		 */
		if (spcname == "pg_default")
		{
			spclocation_abs = Util::fs::absPath(data_directory + DIRECTORY_SEPARATOR + "base" + DIRECTORY_SEPARATOR);
		}
		else if (spcname == "pg_global")
		{
			spclocation_abs = Util::fs::absPath(data_directory + DIRECTORY_SEPARATOR + "global" + DIRECTORY_SEPARATOR);
		}
		else if (spcname == "pg_xlog")
		{
			spclocation_abs = Util::fs::absPath(data_directory + DIRECTORY_SEPARATOR + "pg_xlog" + DIRECTORY_SEPARATOR);
		}
		else
		{
			/*
			 * We need to get the absPath relative to $PGDATA/pg_tblspc, because
			 * a smart DBA might have changed the symlinks directly to a relative
			 * location, and that would be relative to "pg_tblspc", where the symlink
			 * resides. If it is using an absolute path (as expected), the absPath
			 * function will check and handle it for us.
			 */
			spclocation_abs = Util::fs::absPath(data_directory + DIRECTORY_SEPARATOR + "pg_tblspc" + DIRECTORY_SEPARATOR, spclocation);
		}
	}
	catch (...)
	{
		/**
		 * If absPath gives an error, just ignore it. Seems better than
		 * stopping the entire snapshot.
		 */
		got_abs_location = false;
		spclocation_abs = spclocation;
	}
	/* Finally add the fsdevice information into the stream */
	if (!got_abs_location)
	{
		ostr << "\\N";
	}
	else
	{
		DiskUsagePtr df = server_info->diskUsageForPath(spclocation_abs);
		if (df.isNull())
		{
			ostr << "\\N";
		}
		else
		{
			std::string fsdevice = df->fsdevice();
			ostr << con->copyValue(fsdevice);
		}
	}
	if (save_spclocation_abs != NULL)
	{
		(*save_spclocation_abs) = spclocation_abs;
	}
}

void addQueryAsCopyFrom(Db::Connection con, std::ostream &ostr, const std::string &target_table, const std::string &query)
{
	/* Construct "COPY <table>(<columns>, ...) FROM stdin;\n" */
	Db::ResultSet rs = con->execQuery("SELECT * FROM (" + query + ") t LIMIT 0");
	Db::CopyOutputStream cout;
	ostr << COMMAND_COPY_TAB_DELIMITED << " " << target_table << " ";
	for (size_t i = 0; i < rs->ncols(); i++)
	{
		if (i > 0)
		{
			ostr << ",";
		}
		ostr << rs->colName(i);
	}
	ostr << "\n";
	/* Get COPY data */
	cout = con->openCopyOutputStream(
			   std::string("COPY (") + query + ") TO stdout;"
		   );
	cout->toStream(ostr);
	/* End of data */
	ostr << "\\.\n";
}

void dumpAllDatabases(const InstanceConfigPtr config_instance)
{
	bool first = true;
	if (config_instance->databases().size() == 0)
	{
		Db::Connection con = DbConnection::getConnection(config_instance);
		Db::ResultSet rs = con->execQuery(
							   "SELECT d.datname "
							   " FROM pg_catalog.pg_database d "
							   " WHERE d.datallowconn AND NOT d.datistemplate "
							   "       AND has_database_privilege(d.oid, 'CONNECT');"
						   );
		while (rs->next())
		{
			std::string dbname;
			rs->fetchByName("datname", dbname);
			if (first)
			{
				dumpGlobalObjects(config_instance, dbname);
				first = false;
			}
			dumpDatabase(config_instance, dbname);
		}
	}
	else
	{
		forall(db, config_instance->databases())
		{
			if (first)
			{
				dumpGlobalObjects(config_instance, *db);
				first = false;
			}
			dumpDatabase(config_instance, *db);
		}
	}
}

bool getDbConnInformation(
	/* IN: */
	Db::Connection con,
	/* OUT: */
	std::string &datname,
	std::string &datetime,
	int &version_num,
	std::string &version,
	std::string &data_directory
	)
{
	Db::ResultSet rs = con->execQuery(
						   "SELECT pg_catalog.current_database() AS datname, "
						   " pg_catalog.now() AS datetime, "
						   " COALESCE(("
						   "    SELECT setting::int "
						   "    FROM pg_catalog.pg_settings "
						   "    WHERE name = 'server_version_num' "
						   " ), 0) AS version_num, "
						   " pg_catalog.version() AS version,"
						   " pg_catalog.current_setting('data_directory') AS data_directory;"
					   );
	if (rs->next())
	{
		rs->fetchByName("datname", datname);
		rs->fetchByName("datetime", datetime);
		rs->fetchByName("version_num", version_num);
		rs->fetchByName("version", version);
		rs->fetchByName("data_directory", data_directory);
		return true;
	}
	return false;
}

std::string sqlCalcXlogPosFor9_1(const std::string &lsn_expr)
{
	return
		"(((('xFFFFFFFF'::bit(32)::bigint) / c.segsize) * c.segsize) * ('x'||pg_catalog.lpad(pg_catalog.split_part(" + lsn_expr + ", '/', 1), 8, '0'))::bit(32)::bigint + ('x'||pg_catalog.lpad(pg_catalog.split_part(" + lsn_expr + ", '/', 2), 8, '0'))::bit(32)::bigint)";
}

std::string sqlFuncXlogLocationDiff(int version_num, const std::string &lsn_expr1, const std::string &lsn_expr2)
{
	if (version_num >= PG_VERSION_9_2)
		return "pg_catalog.pg_xlog_location_diff(" + lsn_expr1 + ", " + lsn_expr2 + ")";
	else if (version_num >= PG_VERSION_9_0)
		return "(" + sqlCalcXlogPosFor9_1(lsn_expr1) + " - " + sqlCalcXlogPosFor9_1(lsn_expr2) + ")";
	else
		return "NULL::numeric";
}

void dumpGlobalObjects(const InstanceConfigPtr config_instance, const std::string &dbname)
{
	DMSG("Dumping global/shared data, using `" << dbname << "'");
	ServerInfoPtr server_info = ServerInfo::instance();
	Db::Connection con = DbConnection::getConnection(config_instance, dbname);
	Db::ScopedTransaction txDumpStats(con, Db::TRANS_ISOLATION_REPEATABLE_READ);
	PgStatsCollectorStorageManager storage(config_instance->serverConfig(), true);
	storage.begin();
	/* Get useful information in one query */
	std::string datname;
	std::string datetime;
	int version_num;
	std::string version;
	std::string data_directory;
	PgStatsCollectorPrivate::getDbConnInformation(con, datname, datetime, version_num, version, data_directory);
	/* Send snapshot information */
	CollectorPrivate::startSnapshot(storage, "pg_stats_global", config_instance, datname);
	/* pg_stat_bgwriter was born on 8.3 */
	if (version_num >= PG_VERSION_8_3)
	{
		PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stat_bgwriter",
				"SELECT * FROM pg_catalog.pg_stat_bgwriter"
												   );
	}
	/* xlog info */
	PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_instance", std::string() +
			"SELECT pg_catalog.pg_postmaster_start_time() AS pg_postmaster_start_time, "
			+ (
				version_num < PG_VERSION_9_0 ?
				"pg_catalog.pg_current_xlog_location()" :
				" CASE "
				"     WHEN pg_catalog.pg_is_in_recovery() THEN pg_catalog.pg_last_xlog_replay_location() "
				"     ELSE pg_catalog.pg_current_xlog_location() "
				" END"
			) + " AS pg_current_xlog_insert_location"
											   );
	/* TODO: pg_stat_activity */
	/* TODO: pg_prepared_xacts */
	/* pg_settings */
	PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_settings",
			"SELECT name, setting FROM pg_settings"
			);
	/* pg_stat_replication (9.1+) */
	if (version_num >= PG_VERSION_9_1)
	{
		std::string sql =
			"WITH stat_rep AS (\n"
			"    SELECT\n"
			"        CASE\n"
			"            WHEN NOT pg_catalog.pg_is_in_recovery() THEN pg_catalog.pg_current_xlog_location()\n"
			"            ELSE pg_catalog.pg_last_xlog_replay_location()\n"
			"        END AS sender_location,\n"
			"        sr.*\n"
			"    FROM pg_stat_replication sr\n"
			")\n"
			;
		if (version_num < PG_VERSION_9_2)
		{
			sql +=
				", config AS (\n"
				"    SELECT\n"
				"        max(\n"
				"            CASE WHEN name = 'wal_segment_size' THEN setting::bigint ELSE NULL END\n"
				"        ) * max(\n"
				"            CASE WHEN name = 'wal_block_size'   THEN setting::bigint ELSE NULL END\n"
				"        ) AS segsize\n"
				"    FROM pg_settings WHERE name IN ('wal_segment_size', 'wal_block_size')\n"
				")\n"
				;
		}
		sql =
			sql
			+ "SELECT sr.*,\n"
			+ sqlFuncXlogLocationDiff(version_num, "sr.sender_location", "sr.sent_location") + " AS sent_location_diff,\n"
			+ sqlFuncXlogLocationDiff(version_num, "sr.sender_location", "sr.write_location") + " AS write_location_diff,\n"
			+ sqlFuncXlogLocationDiff(version_num, "sr.sender_location", "sr.flush_location") + " AS flush_location_diff,\n"
			+ sqlFuncXlogLocationDiff(version_num, "sr.sender_location", "sr.replay_location") + " AS replay_location_diff\n"
			+ " FROM stat_rep sr";
		if (version_num < PG_VERSION_9_2)
		{
			sql += ", config AS c";
		}
		PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stat_replication", sql);
	} /* pg_stat_replication */
	/* pg_stat_archiver (9.4+) */
	if (version_num >= PG_VERSION_9_4) {
		PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stat_archiver",
				"SELECT * FROM pg_stat_archiver"
				);
	}
	/* Tablespaces are harder, we need to process many other things */
	{
		std::string spcquery = "";
		Db::ResultSet rs;
		Db::CopyOutputStream cout;
		if (version_num >= PG_VERSION_9_2)
		{
			/**
			 * From 9.2+ the spclocation column was dropped. We must use pg_tablespace_location function to get this value
			 */
			spcquery = ", pg_catalog.pg_tablespace_location(oid) AS spclocation";
		}
		spcquery = "SELECT *" + spcquery + ", pg_catalog.pg_tablespace_size(oid) AS spcsize FROM pg_catalog.pg_tablespace";
		rs = con->execQuery(spcquery);
		/* Generate COPY .. FROM .. */
		storage.stream() << COMMAND_COPY_TAB_DELIMITED << " sn_tablespace fsdevice";
		for (size_t i = 0; i < rs->ncols(); i++)
		{
			storage.stream() << "," << rs->colName(i);
		}
		storage.stream() << "\n";
		/* Get COPY data */
		while (rs->next())
		{
			/* Get fsdevice */
			std::string spclocation;
			std::string spcname;
			rs->fetchByName("spclocation", spclocation);
			rs->fetchByName("spcname", spcname);
			PgStatsCollectorPrivate::addSpcFsDeviceToCopy(storage.stream(), con, server_info, data_directory, spcname, spclocation);
			/* Get other columns */
			for (size_t i = 0; i < rs->ncols(); i++)
			{
				std::string data;
				rs->fetch(i, data);
				storage.stream() << con->copyFieldSeparator();
				if (rs->isNull(i))
				{
					storage.stream() << "\\N";
				}
				else
				{
					storage.stream() << con->copyValue(data);
				}
			}
			storage.stream() << con->copyRecordSeparator();
		}
		{
			// Grab information about pg_xlog as it was a tablespace (we call that "pg_xlog", as PostgreSQL disallow tablespace names prefixed with "pg_")
			std::string xlog_location_abs;

			/*
			 * Now, COPY information about pg_xlog. I decided to use the dumb
			 * calls of rs->ncols()/rs->colName() just to avoid errors and
			 * re-design for different versions of PostgreSQL.
			 */
			PgStatsCollectorPrivate::addSpcFsDeviceToCopy(storage.stream(), con, server_info, data_directory, "pg_xlog", "", &xlog_location_abs);
			for (size_t i = 0; i < rs->ncols(); i++)
			{
				std::string colname = rs->colName(i);
				std::string value = "";
				storage.stream() << con->copyFieldSeparator();
				if (colname == "spcname")
				{
					storage.stream() << con->copyValue("pg_xlog");
				}
				else if (colname == "spclocation")
				{
					storage.stream() << con->copyValue(xlog_location_abs);
				}
				else if (colname == "spcsize")
				{
					/*
					 * Summarize valid xlog segments files
					 */
					long long unsigned int spcsize = 0;
					Util::fs::DirReader dir(xlog_location_abs);
					while (dir.next())
					{
						std::string fname = dir.entry().d_name;
						/*
						 * Check if it is a valid xlog segment filename, e.g. there is only
						 * valid hex digits ([0-9A-F]). find_first_not_of is used to check
						 * if there is any character that is not one of those valids
						 */
						if (!fname.empty() && fname.find_first_not_of("0123456789ABCDEF") == std::string::npos)
						{
							Util::fs::Stat st = Util::fs::fileStat(xlog_location_abs + DIRECTORY_SEPARATOR + fname);
							if (S_ISREG(st.st_mode))
							{
								spcsize += st.st_size;
							}
						}
					}
					storage.stream() << spcsize;
				}
				else
				{
					storage.stream() << "\\N";
				}
			}
			storage.stream() << con->copyRecordSeparator();
		}
		// end of pg_xlog
		/* End of data */
		storage.stream() << "\\.\n";
	} // end of tablespace dump
	storage.commit();
}

void dumpDatabase(const InstanceConfigPtr config_instance, const std::string &dbname)
{
	DMSG("Dumping database `" << dbname << "'");
	ServerInfoPtr server_info = ServerInfo::instance();
	Db::Connection con = DbConnection::getConnection(config_instance, dbname);
	Db::ScopedTransaction txDumpStats(con, Db::TRANS_ISOLATION_REPEATABLE_READ);
	PgStatsCollectorStorageManager storage(config_instance->serverConfig());
	storage.begin();
	/* Get useful information in one query */
	std::string datname;
	std::string datetime;
	int version_num;
	std::string version;
	std::string data_directory;
	PgStatsCollectorPrivate::getDbConnInformation(con, datname, datetime, version_num, version, data_directory);
	/* Send snapshot information */
	CollectorPrivate::startSnapshot(storage, "pg_stats", config_instance, datname);
	/* Simple query tables snapshot ("simple" as in "SELECT * FROM table") */
	{
		const char *tables[] =
		{
			"stat_all_tables",
			"stat_all_indexes",
			"statio_all_indexes",
			"statio_all_sequences",
			"statio_all_tables",
			"namespace"
		};
		for (size_t i = 0; i < lengthof(tables); i++)
		{
			PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), std::string("sn_") + tables[i],
					std::string("SELECT * FROM pg_catalog.pg_") + tables[i]
													   );
		}
		if (version_num >= PG_VERSION_8_4)   /* pg_stat_user_functions was born on 8.4 */
		{
			PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stat_user_functions",
					"SELECT * FROM pg_catalog.pg_stat_user_functions"
													   );
		}
	}
	/* Not a simple query */
	PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stats",
			std::string("SELECT s.schemaname,s.tablename,s.attname,")
			+ (version_num >= PG_VERSION_9_0 ? "s.inherited," : "") +
			"s.null_frac,s.avg_width,s.n_distinct,s.correlation "
			" FROM pg_catalog.pg_stats AS s"
											   );
	PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stat_database",
			"SELECT * FROM pg_catalog.pg_stat_database "
			" WHERE datname = pg_catalog.current_database()"
											   );
	PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_database", std::string() +
			"SELECT *, pg_catalog.pg_database_size(oid) AS dbsize, "
			+ (
				version_num < PG_VERSION_9_0 ?
				"pg_catalog.age(datfrozenxid)" :
				" CASE WHEN pg_catalog.pg_is_in_recovery() THEN NULL ELSE pg_catalog.age(datfrozenxid) END "
			) + " AS age_datfrozenxid"
			" FROM pg_catalog.pg_database "
			" WHERE datname = pg_catalog.current_database()"
											   );
	if (version_num >= PG_VERSION_9_1)
	{
		PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_stat_database_conflicts",
				"SELECT * FROM pg_catalog.pg_stat_database_conflicts"
				" WHERE datname = pg_catalog.current_database()"
												   );
	}
	PgStatsCollectorPrivate::addQueryAsCopyFrom(con, storage.stream(), "sn_relations", std::string() +
			"SELECT r.oid AS relid, r.relname, n.nspname, r.relfilenode, t.spcname, r.relpages, "
			" r.reltuples, r.relkind, r.relfrozenxid, r.reloptions, "
			" (r.relpages::bigint * pg_catalog.current_setting('block_size')::bigint)::bigint AS relsize, "
			+ (
				version_num < PG_VERSION_9_0 ?
				"pg_catalog.age(r.relfrozenxid)" :
				" CASE WHEN pg_catalog.pg_is_in_recovery() THEN NULL ELSE pg_catalog.age(r.relfrozenxid) END "
			) + "  AS age_relfrozenxid "
			" FROM pg_catalog.pg_class AS r "
			" INNER JOIN pg_catalog.pg_namespace AS n ON r.relnamespace = n.oid "
			" LEFT JOIN pg_catalog.pg_tablespace AS t ON r.reltablespace = t.oid "
											   );
	storage.commit();
}
} // namespace PgStatsCollectorPrivate

void PgStatsCollector::execute()
{
	ASSERT_EXCEPTION(!this->config_instance.isNull(), std::runtime_error, "instance not valid");
	PgStatsCollectorPrivate::dumpAllDatabases(this->config_instance);
}


void DiskUsageCollector::execute()
{
	ServerInfoPtr server_info = ServerInfo::instance();
	ServerConfigPtr config_server = server_info->currentServerConfig();
	DiskUsageCollectorStorageManager storage(config_server);
	storage.begin();
	/* Send snapshot information */
	CollectorPrivate::startSnapshot(storage, "df");
	/* COPY ... FROM stdin */
	storage.stream() << COMMAND_COPY_TAB_DELIMITED << " sn_disk_usage fsdevice,fstype,size,used,available,usage,mountpoint\n";
	/* COPY data */
	{
		const std::vector<DiskUsagePtr> df_result = server_info->diskUsage();
		forall(df_iterator, df_result)
		{
			const DiskUsagePtr df_item = (*df_iterator);
			storage.stream()
					<< Db::Internal::PQFormatter::copyValue(df_item->fsdevice())
					<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(df_item->fstype())
					<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(Util::numberToString(df_item->size()))
					<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(Util::numberToString(df_item->used()))
					<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(Util::numberToString(df_item->available()))
					<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(df_item->usage())
					<< Db::Internal::PQFormatter::copyFieldSeparator() << Db::Internal::PQFormatter::copyValue(df_item->mountpoint())
					<< Db::Internal::PQFormatter::copyRecordSeparator();
		}
	}
	/* End of data */
	storage.stream() << "\\.\n";
	storage.commit();
}

namespace PgLogCollectorPrivate
{
struct pg_log_meta_data
{
	std::string first_line;
	std::string last_line;
	std::string last_line_complete;
	size_t last_line_pos;
	time_t first_time;
	time_t last_time;
	bool empty;
	bool valid;
	pg_log_meta_data()
		: valid(true)
	{}
};

pg_log_meta_data recoveryLogMetaFromState(const StateManager::Map &state)
{
	pg_log_meta_data ret;
	struct state_def_t
	{
		const char *key;
		std::string *str_ref;
		size_t *int_ref;
	};
	state_def_t state_def[] =
	{
		{"ll",  &ret.last_line,          NULL},
		{"llp", NULL,                    &ret.last_line_pos},
		{"llc", &ret.last_line_complete, NULL}
	};
	ret.valid = true;
	for(size_t i = 0; i < lengthof(state_def); i++)
	{
		const state_def_t &s = state_def[i];
		StateManager::Map::const_iterator it = state.find(s.key);
		if (it == state.end())
		{
			ret.valid = false;
		}
		else if (s.str_ref)
		{
			*s.str_ref = it->second;
		}
		else if (s.int_ref)
		{
			*s.int_ref = Util::stringToNumber<size_t>(it->second);
		}
	}
	return ret;
}

void saveLogMetaToState(StateManager &state_mgr, StorageManager &storage, pg_log_meta_data &meta)
{
	StateManager::Map state;
	state["ll"] = meta.last_line;
	state["llc"] = meta.last_line_complete;
	state["llp"] = Util::numberToString(meta.last_line_pos);
	state_mgr.save(state, storage);
}

class LogNavigator
{
public:
	std::istream &ifs;
	pg_log_meta_data &meta;
	bool eof;
	std::string read_ahead;
	std::string logitem;
	bool checkEOF(const std::string &buffer)
	{
		if (!this->meta.valid)
		{
			//DMSG("Not valid meta");
			this->eof = false;
		}
		else
		{
			size_t pos = buffer.find_first_of("\n\r");
			if (pos == std::string::npos)
			{
				pos = buffer.length();
			}
			if (buffer.compare(0, pos, this->meta.last_line) == 0)
			{
				//DMSG("EOF");
				this->eof = true;
			}
		}
		return this->eof;
	}
	void addLineToReadAhead(const std::string &buffer)
	{
		if (this->read_ahead.empty())
		{
			this->read_ahead = buffer;
		}
		else
		{
			this->read_ahead.append("\n");
			this->read_ahead.append(buffer);
		}
	}
	void swapReadAhead(const std::string &buffer)
	{
		//DMSG("swap \"" << this->read_ahead << "\" with \"" << buffer << "\"");
		this->checkEOF(this->read_ahead);
		this->logitem = this->read_ahead;
		this->read_ahead = buffer;
	}
	bool next()
	{
		std::string buffer;
		if (this->eof)
		{
			return false;
		}
		if (!ifs)
		{
			if (this->read_ahead.empty())
			{
				//DMSG("Read Ahead is empty and we are at EOF");
				return false;
			}
			else
			{
				//DMSG("Last one to go...");
				this->swapReadAhead("");
				return true;
			}
		}
		while (std::getline(ifs, buffer))
		{
			//DMSG("Processing line: \"" << buffer << "\"");
			if (buffer[0] == '\t')
			{
				/* Continuing previous log item */
				this->addLineToReadAhead(buffer);
			}
			else
			{
				/* It is a new log item */
				break;
			}
		}
		this->swapReadAhead(buffer);
		return true;
	}
	void clear()
	{
		this->read_ahead.clear();
	}
	const std::string &current() const
	{
		return this->logitem;
	}
	LogNavigator(std::istream &ifs, pg_log_meta_data &meta)
		: ifs(ifs), meta(meta), eof(false)
	{
		this->next();
	}
};

pg_log_meta_data processLogMeta(std::istream &ifs)
{
	pg_log_meta_data ret;
	char c;
	ifs.clear();
	if (!std::getline(ifs, ret.first_line))
	{
		ret.empty = true;
		ret.valid = true;
		return ret;
	}
	ret.valid = false;
	ret.empty = false;
	ifs.clear();
	ifs.seekg(0, std::ios::end);
	/* Check if the last line is finished */
	ifs.unget();
	c = ifs.get();
	if (c != '\n')
	{
		/**
		 * If the last line does not end with a \n, we assume we got into
		 * a racy issue, and this line is still being written. So, just
		 * skip this one and read the line above.
		 * OBS: I'm not sure if the partial-write can actually happen, but
		 *      it is safer to avoid such issue, as every PostgreSQL log
		 *      line must end with a \n.
		 */
		while(ifs)
		{
			ifs.unget();
			ifs.unget();
			c = ifs.get();
			if (c == '\n')
			{
				break;
			}
		}
	}
	while(ifs)
	{
		ifs.unget();
		if (ifs.tellg() == 0)
		{
			/* Happens if the log has only one line */
			c = '\n';
		}
		else
		{
			ifs.unget();
			c = ifs.get();
		}
		if (c == '\n')
		{
			size_t cur_pos = ifs.tellg();
			std::getline(ifs, ret.last_line);
			if (ret.last_line.empty() || ret.last_line[0] == '\t')
			{
				/* Ignore blank lines or those that not start a log */
				ifs.seekg(cur_pos, std::ios::beg);
			}
			else
			{
				ret.valid = true;
				pg_log_meta_data fake_meta;
				fake_meta.valid = false;
				ret.last_line_pos = cur_pos;
				ifs.seekg(cur_pos, std::ios::beg);
				LogNavigator ln(ifs, fake_meta);
				if (ln.next())
				{
					ret.last_line_complete = ln.current();
				}
				break;
			}
		}
	}
	if (!ret.valid)
	{
		pg_log_meta_data fake_meta;
		fake_meta.valid = false;
		ret.valid = true;
		ret.last_line = ret.first_line;
		ifs.seekg(0, std::ios::beg);
		LogNavigator ln(ifs, fake_meta);
		if (ln.next())
		{
			ret.last_line_complete = ln.current();
		}
	}
	// DMSG(
	// 		"first_line: \"" << ret.first_line << "\""
	// 		<< "\n           valid : \"" << (ret.valid ? "true" : "false") << "\""
	// 		<< "\n           empty : \"" << (ret.empty ? "true" : "false") << "\""
	// 		<< "\n           last_line : \"" << ret.last_line << "\""
	// 		<< "\n           last_line_complete : \"" << ret.last_line_complete << "\""
	// 		<< "\n           last_pos  : \"" << ret.last_line_pos << "\""
	// 		);
	// std::istringstream iss(ret.first_line);
	// std::tm ot;
	// std::string date, time, tz;
	// time_t oet;
	// iss
	// 	>> date
	// 	>> time
	// 	>> tz;
	// ::strptime((date + " " + time).c_str(), "%F %T", &ot);
	// oet = Util::time::mktime(ot, tz);
	// DMSG("Start epoch: " << oet);
	return ret;
}

void processLogStream(std::istream &ifs, StateManager &state_mgr, StorageManager &storage, const std::string &filepath, const std::string &log_line_prefix, InstanceConfigPtr config_instance = NULL)
{
	bool something_written = false;
	bool recheck_last_line = true;
	StateManager::Map state = state_mgr.load();
	pg_log_meta_data old_meta = recoveryLogMetaFromState(state);
	pg_log_meta_data new_meta = processLogMeta(ifs);
	//DMSG("last line pos: " << new_meta.last_line_pos);
	/* XXX: After processLogMeta call, the ifs cursor is at an undefined position */
	std::string line;
	if (old_meta.valid)
	{
		ifs.clear();
		ifs.seekg(old_meta.last_line_pos, std::ios::beg); /* go to the beginning */
		/** If old state is saved, we must check if that is a new file
		 * (e.g. if we have to process it entirely). That happens if the
		 * saved last line is different from the current or if we simple
		 * can't get to the saved last line position (means the file has
		 * been truncated)
		 */
		if (
			!std::getline(ifs, line)       /* Get the line, if EOF, then it is a new file */
			|| line != old_meta.last_line  /* The getline succeeded, check the content */
		)
		{
			/* So it is new file, process it all */
			old_meta.valid = false;
		}
	}
	/* Yes, we do need to recheck */
	if (!old_meta.valid)
	{
		/* Make sure we are at first line */
		old_meta.last_line_pos = 0;
		recheck_last_line = false; /* It is not the same, so don't need recheck */
	}
	else
	{
		recheck_last_line = true;
	}
	ifs.clear();
	ifs.seekg(old_meta.last_line_pos, std::ios::beg);
	new_meta.valid = true;
	LogNavigator ln(ifs, new_meta);
	while (ln.next())
	{
		//DMSG("Got item: " << ln.current());
		if (recheck_last_line)
		{
			recheck_last_line = false;
			if (ln.current() == old_meta.last_line_complete)
			{
				/**
				 * The last line haven't changed since last call, so
				 * we can skip it. It could possible be different if we
				 * read an incomplete log item the last time. So, send
				 * it again and let the server handle to update the old
				 * (stale) value. I think this is very unlikely to
				 * happen, but possible, and my friend Murphy told me
				 * that it is going to happen when I'm not expecting.
				 */
				continue;
			}
		}
		if (!something_written)
		{
			/* Delay the begin until we need something to write */
			storage.begin();
			storage.stream() << "# filename "  << Util::escapeString(filepath) << "\n";
			storage.stream() << "# oll "       << Util::escapeString(old_meta.last_line) << "\n";
			storage.stream() << "# ollp "      << old_meta.last_line_pos << "\n";
			storage.stream() << "# nll "       << Util::escapeString(new_meta.last_line) << "\n";
			storage.stream() << "# nllp "      << new_meta.last_line_pos << "\n";
			if (!log_line_prefix.empty())
			{
				storage.stream() << "# llp "  << Util::escapeString(log_line_prefix) << "\n";
			}
			CollectorPrivate::startSnapshot(storage, "pg_log", config_instance);
			something_written = true;
		}
		storage.stream() << ln.current() << "\n";
	}
	if (something_written)
	{
		saveLogMetaToState(state_mgr, storage, new_meta);
	}
	else
	{
		DMSG("Nothing written");
	}
}

void processLogFile(InstanceConfigPtr instance, const std::string &filepath, int port, const std::string &log_line_prefix)
{
	DMSG("Processing `" << filepath << "`");
	std::string filename = Util::fs::baseName(filepath);
	std::ostringstream state_file;
	state_file << "log-" << port << "-" << filename;
	CollectorStateManager state_mgr(state_file.str());
	PgLogCollectorStorageManager storage(instance->serverConfig());
	std::ifstream ifs;
	ifs.exceptions(std::ios::failbit | std::ios::badbit);
	ifs.open(filepath.c_str(), std::ios::in | std::ios::binary);
	ifs.exceptions(std::ios::badbit);
	processLogStream(ifs, state_mgr, storage, filepath, log_line_prefix, instance);
}
} // namespace PgLogCollectorPrivate

void PgLogCollector::execute()
{
	ASSERT_EXCEPTION(!this->config_instance.isNull(), std::runtime_error, "instance not valid");
	std::string log_directory;
	std::string data_directory;
	std::string log_line_prefix;
	int port;
	{
		/* Connection scope, no need to left it open any longer */
		Db::Connection con = DbConnection::getConnection(this->config_instance);
		Db::ResultSet rs = con->execQuery("SELECT "
										  "current_setting('port') AS port, "
										  "current_setting('log_directory') AS log_directory, "
										  "current_setting('data_directory') AS data_directory, "
										  "current_setting('log_line_prefix') AS log_line_prefix "
										  ";"
										 );
		if (rs->next())
		{
			rs->fetchByName("port", port);
			rs->fetchByName("log_directory", log_directory);
			rs->fetchByName("data_directory", data_directory);
			rs->fetchByName("log_line_prefix", log_line_prefix);
		}
	} /* End of connection scope */
	std::string abs_log_directory = Util::fs::absPath(data_directory, log_directory);
	Util::fs::DirReader dr(abs_log_directory);
	while (dr.next())
	{
		std::string filename = dr.entry().d_name;
		std::string filepath = abs_log_directory + DIRECTORY_SEPARATOR + filename;
		if (Util::fs::fileExtension(filename) == "log")
		{
			PgLogCollectorPrivate::processLogFile(this->config_instance, filepath, port, log_line_prefix);
		}
	}
}

void PgLogCollector::processLogFile(const std::string &filepath, int port, const std::string &log_line_prefix)
{
	ASSERT_EXCEPTION(!this->config_instance.isNull(), std::runtime_error, "instance not valid");
	PgLogCollectorPrivate::processLogFile(this->config_instance, filepath, port, log_line_prefix);
}

void PgLogCollector::processLogStream(std::istream &istr, StateManager &state_mgr, StorageManager &storage_mgr, const std::string &filepath, const std::string &log_line_prefix)
{
	PgLogCollectorPrivate::processLogStream(istr, state_mgr, storage_mgr, filepath, log_line_prefix);
}

END_APP_NAMESPACE

