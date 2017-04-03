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

BEGIN_APP_NAMESPACE

namespace CollectorPrivate
{

extern void startSnapshot(StorageManager &storage, const std::string &snap_type, InstanceConfigPtr instance = NULL, const std::string &dbname = "");

} // namespace CollectorPrivate

namespace SysstatCollectorPrivate
{
struct sadf_file_metadata
{
	time_t first_time;
	time_t last_time;
	int ret_code;
	bool is_empty;
};
/**
 * @brief Check if given option is valid for sadf program
 *
 * @param sadf_option	options to try (it won't be escaped)
 *
 * @return true if sadf_option is valid, and false otherwise
 */
bool trySadfOption(const std::string &sadf_option, const std::string &sar_option = "");
std::vector<std::string> splitCSVLine(const std::string &line);
sadf_file_metadata addSadfDataToStream(
	std::ostream &ostr,
	const std::string &output_table,
	const std::string &sadf_option,
	const std::string &filename,
	const std::string &sar_option,
	const std::string &start_date,
	const std::string &end_date
);

bool trySadfOption(const std::string &sadf_option, const std::string &sar_option)
{
	/* Just open the stream and close it immediately */
	std::ostringstream call;
	call << "LC_ALL=C ";
	if (sar_option.empty())
	{
		call << "sadf " << sadf_option << " 1 1 > /dev/null 2>&1";
	}
	else
	{
		call << "sadf " << sadf_option << " 1 1 -- " << sar_option << " > /dev/null 2>&1";
	}
	Util::io::iprocstream sadf(call.str());
	return (sadf.close() == 0);
}

std::vector<std::string> splitCSVLine(const std::string &line)
{
	size_t last_start = 0;
	size_t i;
	std::vector<std::string> ret;
	for (i = 0; i < line.size(); i++)
	{
		if (line[i] == ';' || line[i] == ',')
		{
			ret.push_back(line.substr(last_start, (i - last_start)));
			last_start = i + 1;
		}
	}
	ret.push_back(line.substr(last_start, (i - last_start)));
	return ret;
}

sadf_file_metadata addSadfDataToStream(
	std::ostream &ostr,
	const std::string &output_table,
	const std::string &sadf_option,
	const std::string &filename,
	const std::string &sar_option,
	const std::string &start_date,
	const std::string &end_date
)
{
	std::ostringstream sadf_call;
	int timestamp_col_pos = -1;
	sadf_file_metadata ret;
	ret.is_empty = true;
	ret.first_time = ret.last_time = (time_t)(-1);
	sadf_call
			<< "LC_ALL=C "
			<< "sadf " << sadf_option << " " << Util::io::quoteProcArgument(filename)
			<< " -- " << sar_option;
	if (!start_date.empty())
	{
		sadf_call << " -s " << Util::io::quoteProcArgument(start_date);
	}
	if (!end_date.empty())
	{
		sadf_call << " -e " << Util::io::quoteProcArgument(end_date);
	}
	DMSG(sadf_call.str());
	Util::io::iprocstream sadfstream(sadf_call.str());
	std::string buffer;
	/**
	 * Find the line with header, that is the line that starts with "#".
	 * It will generally be the first, but after a restart, a line with
	 * LINUX-RESTART can be found first.
	 */
	while (std::getline(sadfstream, buffer))
	{
		ret.is_empty = false;
		if (buffer[0] == '#')
		{
			std::vector<std::string> items;
			/* That is the header line, let's work on that and get out of the loop */
			/* First, remove the '#' and space (if there is an space after) */
			buffer = buffer.substr((buffer[1] == ' ' ? 2 : 1));
			/* XXX: We don't care at all about non-ASCII characters, they will just become "_" */
			for (size_t ibuf = 0; ibuf < buffer.size(); ibuf++)
			{
				/* Convert upper to small caps */
				if (buffer[ibuf] >= 'A' && buffer[ibuf] <= 'Z')
				{
					buffer[ibuf] = buffer[ibuf] - ('A' - 'a');
				}
				if (buffer[ibuf] == ';')
				{
					/* Convert semicolon (;) to comma (,) -- which is expected as delimiter for COPY columns */
					buffer[ibuf] = ',';
				}
				else if (
					!(
						(buffer[ibuf] >= '0' && buffer[ibuf] <= '9')
						|| (buffer[ibuf] >= 'a' && buffer[ibuf] <= 'z')
					)
				)
				{
					/* Check if not a-z0-9 */
					buffer[ibuf] = '_';
				}
			}
			/* Get timestamp column position */
			items = splitCSVLine(buffer);
			for (size_t i = 0; i < items.size(); i++)
			{
				if (items[i] == "timestamp")
				{
					timestamp_col_pos = i;
				}
			}
			/* Append: COPY <tablename>(<columns>) FROM stdin WITH (FORMAT CSV, DELIMITER ';'); */
			ostr << COMMAND_COPY_CSV_SEMICOLON_TO_TEMP_TABLE << " " << output_table << " " << buffer << "\n";
			/* OK, that is all we need with this buffer */
			break;
		}
	}
	if (ret.is_empty)
	{
		ret.ret_code = sadfstream.close();
		return ret;
	}
	else if (timestamp_col_pos < 0)
	{
		throw std::runtime_error("\"timestamp\" column not found");
	}
	/* COPY data */
	while (std::getline(sadfstream, buffer))
	{
		std::vector<std::string> items;
		/* Ignore LINUX-RESTART and header lines */
		if (buffer[0] == '#' || buffer.find("LINUX") != std::string::npos)
		{
			continue;
		}
		ostr << buffer << "\n";
		items = splitCSVLine(buffer);
		ret.last_time = Util::stringToNumber<time_t>(items[timestamp_col_pos]);
		if (ret.first_time == (time_t)(-1))
		{
			ret.first_time = ret.last_time;
		}
	}
	/* End of data */
	ostr << "\\.\n";
	ret.ret_code = sadfstream.close();
	return ret;
}

time_t processSadfForFile(
	CollectorStateManager &statemgr,
	std::map<std::string, std::string> &state_map,
	const std::string &sadf_option,
	const std::string &filename,
	time_t start_time,
	time_t end_time = (time_t)(-1)
)
{
	SysstatCollectorStorageManager storage(ServerInfo::instance()->currentServerConfig());
	std::ostringstream start_time_str;
	std::ostringstream end_time_str;
	std::tm start_time_tm = Util::time::localtime(start_time);
	std::tm end_time_tm;
	sadf_file_metadata processed;
	struct sadf_calls_t
	{
		const char *output_table;
		const char *sar_option;
		bool try_first;
	};
	const struct sadf_calls_t sadf_calls[] =
	{
		/* output_table               sar_option   try_first */
		{"sn_sysstat_io",           "-b",     false},
		{"sn_sysstat_paging",       "-B",     false},
		{"sn_sysstat_disks",        "-d -p",  false},
		{"sn_sysstat_network",      "-n DEV", false},
		{"sn_sysstat_cpu",          "-P ALL", false},
		{"sn_sysstat_loadqueue",    "-q",     false},
		{"sn_sysstat_memusage",     "-r",     false},
		{"sn_sysstat_memstats",     "-R",     false},
		{"sn_sysstat_swapusage",    "-S",     false},
		{"sn_sysstat_kerneltables", "-v",     false},
		{"sn_sysstat_tasks",        "-w",     false},
		{"sn_sysstat_swapstats",    "-W",     false},
		{"sn_sysstat_hugepages",    "-H",     true}
	};
	storage.begin();
	start_time_str
			<< std::setfill('0') << std::setw(2) << start_time_tm.tm_hour << ":"
			<< std::setfill('0') << std::setw(2) << start_time_tm.tm_min  << ":"
			<< std::setfill('0') << std::setw(2) << start_time_tm.tm_sec
			;
	if (end_time != (time_t)(-1))
	{
		end_time_tm = Util::time::localtime(end_time);
		end_time_str
				<< std::setfill('0') << std::setw(2) << end_time_tm.tm_hour << ":"
				<< std::setfill('0') << std::setw(2) << end_time_tm.tm_min  << ":"
				<< std::setfill('0') << std::setw(2) << end_time_tm.tm_sec
				;
	}
	CollectorPrivate::startSnapshot(storage, "sysstat");
	storage.stream() << "-- file: " << Util::escapeString(filename) << "\n";
	for (size_t i = 0; i < lengthof(sadf_calls); i++)
	{
		std::tm first_ret_tm;
		bool is_same_day;
		if (sadf_calls[i].try_first && !trySadfOption(sadf_option, sadf_calls[i].sar_option))
		{
			continue;
		}
		processed = SysstatCollectorPrivate::addSadfDataToStream(
						storage.stream(),
						sadf_calls[i].output_table,
						sadf_option,
						filename,
						sadf_calls[i].sar_option,
						start_time_str.str(),
						end_time_str.str()
					);
		if (processed.ret_code != 0)
		{
			throw std::runtime_error(std::string("sadf call failed with code ") + Util::numberToString(processed.ret_code));
		}
		else if (processed.is_empty)
		{
			DMSG("Ignoring empty sysstat file `" << filename << "'");
			storage.rollback();
			return start_time;
		}
		first_ret_tm = Util::time::localtime(processed.first_time);
		is_same_day = (
						  (start_time_tm.tm_year == first_ret_tm.tm_year)
						  && (start_time_tm.tm_mon == first_ret_tm.tm_mon)
						  && (start_time_tm.tm_mday == first_ret_tm.tm_mday)
					  );
		if (processed.first_time < start_time || !is_same_day)
		{
			DMSG("Ignoring results of `" << filename << "'");
			storage.rollback();
			return start_time;
		}
	}
	/*
	storage.stream()
		<< "SELECT sn_sysstat_import("
			<< Db::Internal::PQFormatter::quoteString(ServerInfo::instance()->currentServerConfig()->userConfig()->customer())
			<< ","
			<< Db::Internal::PQFormatter::quoteString(ServerInfo::instance()->currentServerConfig()->hostname())
			<< ");"
			<< "\n";
	*/
	state_map["last"] = Util::numberToString(processed.last_time);
	statemgr.save(state_map, storage);
	return processed.last_time;
}
} // namespace SysstatCollectorPrivate

void SysstatCollector::execute()
{
	CollectorStateManager statemgr("sysstat");
	std::map<std::string, std::string> state = statemgr.load();
	std::string sadf_option;
	time_t last_time;
	time_t now;
	std::tm last_time_tm;
	std::tm now_tm;
	const char *sadf_available_options[] =
	{
		"-D",
		"-U -d",
		"-T -d",
		"" // empty string is the EOF marker
	};
	std::string sysstat_log_dir;
	const char *sysstat_available_log_dir[] =
	{
		"/var/log/sysstat",
		"/var/log/sa",
		"" // empty string is the EOF marker
	};
	/**
	 * Check which sadf options to use on this platform.
	 * - Some systems use -d for CVS-like data and -T for epoch
	 * - Others use -D for both CVS-like and epoch
	 *
	 * XXX: The order of the tests matters, as there are platforms that accept
	 *      them with other meanings
	 */
	for (int i = 0; sadf_available_options[i][0] != '\0'; i++)
	{
		if (SysstatCollectorPrivate::trySadfOption(sadf_available_options[i]))
		{
			sadf_option = sadf_available_options[i];
			break;
		}
	}
	if (sadf_option.empty())
	{
		throw std::runtime_error("could not determine valid sadf options");
	}
	/**
	 * Find for available sysstat log dirs
	 */
	for (int i = 0; sysstat_available_log_dir[i][0] != '\0'; i++)
	{
		std::string tmp;
		tmp = sysstat_available_log_dir[i];
		Util::fs::Stat st;
		if (Util::fs::fileExists(tmp, &st))
		{
			if (S_ISDIR(st.st_mode))
			{
				sysstat_log_dir = tmp;
				break;
			}
		}
	}
	if (sysstat_log_dir.empty())
	{
		throw std::runtime_error("sysstat log dir not found");
	}
	/**
	 * Load last processed item
	 */
	{
		std::map<std::string, std::string>::iterator lit = state.find("last");
		if (lit != state.end())
		{
			last_time = Util::stringToNumber<time_t>(lit->second);
		}
		else
		{
			/* No last time available, assume today at midnight */
			last_time = Util::MainApplication::instance()->startTime();
			last_time_tm = Util::time::localtime(last_time);
			last_time_tm.tm_hour = last_time_tm.tm_min = last_time_tm.tm_sec = 0; // 00:00:00
			last_time = Util::time::mktime(last_time_tm);
		}
	}
	DMSG("sadf_option: " << sadf_option << ", sysstat_log_dir: " << sysstat_log_dir);
	now = Util::MainApplication::instance()->startTime();
	now_tm = Util::time::localtime(now);
	/* Process files until gets today's file or overlaps now (should never happen) */
	while (last_time < now)
	{
		std::ostringstream filename;
		last_time_tm = Util::time::localtime(last_time);
		bool is_today = (
							(last_time_tm.tm_year == now_tm.tm_year)
							&& (last_time_tm.tm_mon == now_tm.tm_mon)
							&& (last_time_tm.tm_mday == now_tm.tm_mday)
						);
		/* Process file named with the day */
		filename << sysstat_log_dir << "/sa" << std::setfill('0') << std::setw(2) << last_time_tm.tm_mday;
		DMSG("Trying `" << filename.str() << "'");
		DMSG(last_time << " = {" << (last_time_tm.tm_year+1900) << "-" << (last_time_tm.tm_mon+1) << "-" << last_time_tm.tm_mday << " " << last_time_tm.tm_hour << ":" << last_time_tm.tm_min << ":" << last_time_tm.tm_sec << "}");
		if (Util::fs::fileExists(filename.str()))
		{
			last_time = SysstatCollectorPrivate::processSadfForFile(
							statemgr,
							state,
							sadf_option,
							filename.str(),
							last_time,
							(is_today ? now : (time_t)(-1)) /* For older files, process everything */
						);
			if (is_today)
			{
				/* If it is today's file, we are done */
				break;
			}
			else
			{
				/* If we are processing old files, we need to go to next day at midnight */
				last_time_tm.tm_mday += 1; // mktime solves month wrapping (if happened)
				last_time_tm.tm_hour = last_time_tm.tm_min = last_time_tm.tm_sec = 0; // 00:00:00
				last_time = Util::time::mktime(last_time_tm);
			}
		}
		else
		{
			/* The desired file does not exists, just go to the next day */
			DMSG("File `" << filename.str() << "' not found");
			last_time += 3600 * 24;
		}
	}
}

END_APP_NAMESPACE

