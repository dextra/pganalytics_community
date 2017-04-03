#ifndef __DB_CONNECTION_H__
#define __DB_CONNECTION_H__

/**
 * This file has utility functions to handle database connections.
 * Ideally, no connection should be opened without using these functions.
 * This could become a connection pool someday.
 */

#include "debug.h"
#include "UserConfig.h"
#include "db/Database.h"
#include "db/pq.h"

BEGIN_APP_NAMESPACE

namespace DbConnection
{
/**
 * @brief Get a direct connection given the conninfo parameter
 *
 * @param conninfo the connection info given to Db::PQConnection constructor
 *
 * @return returns a smart pointer to the opened connection
 */
inline Db::Connection getConnection(const std::string &conninfo)
{
	Db::Connection con = new Db::PQConnection(conninfo);
	try
	{
		/* Those may only be available for SUPERUSER, so just ignore error */
		con->execCommand("SET log_statement TO 'none';");
		con->execCommand("SET log_duration TO 'off';");
		con->execCommand("SET log_min_duration_statement TO -1;");
		Db::ResultSet rs = con->execQuery(
							   "SELECT set_config(text 'work_mem', max(work_mem)::text, false) FROM ( "
							   "     (SELECT 50*1024 AS work_mem) UNION ALL (SELECT setting::int FROM pg_settings WHERE name = 'work_mem') "
							   " ) AS s;"
						   );
		(void)rs;
		con->execCommand("SET log_temp_files TO -1;"); /* Doesn't exist on old versions, so ignore the error */
	}
	catch(...)
	{
		/* Ignores, not really important */
	}
	try
	{
		con->execCommand("SET standard_conforming_strings TO 'on';");
	}
	catch(...)
	{
	}
	return con;
}
/**
 * @brief get a connection given the instance configuration and a database name
 *
 * @param instance
 * @param dbname
 *
 * @return returns a smart pointer to the opened connection
 */
inline Db::Connection getConnection(InstanceConfigPtr instance, const std::string &dbname)
{
	return getConnection(instance->conninfo() + " dbname=" + Db::PQConnection::escapeConnectionParameter(dbname));
}
/**
 * @brief get a connection give the instance configuration
 *        (the connection is opened to the instance->maintenanceDatabase()
 *        database, if given, or "postgres" otherwise)
 *
 * @param instance
 *
 * @return returns a smart pointer to the opened connection
 */
inline Db::Connection getConnection(InstanceConfigPtr instance)
{
	if (instance->maintenanceDatabase().empty())
	{
		try
		{
			return getConnection(instance, "postgres");
		}
		catch(const Db::ConnectionException &e)
		{
			DMSG("couldn't connect to \"postgres\" database, trying \"template1\"");
			return getConnection(instance, "template1");
		}
	}
	else
	{
		return getConnection(instance, instance->maintenanceDatabase());
	}
}
} // namespace DbConnection

END_APP_NAMESPACE

#endif // __DB_CONNECTION_H__

