#include "gtest/gtest.h"
#include "util/time.h"
#include "util/string.h"
#include "util/app.h"
#include "StateManager.h"
#include "StorageManager.h"
#include "collectors.h"
#include "backend/importer.h"
#include "common.h"
#include "debug.h"
#include <string>

#define TEST_CONF_CONTENT \
	"customer \"test\"\n" \
	"bucket \"pganalytics_test\"\n" \
	"collect_dir \"/tmp/pganalytics/\"\n" \
	"server_name \"testsrv\"\n" \
	"databases \"postgres\"\n" \

using namespace pga;

class TesterLogCollector {
public:
	MemoryStateManager state_mgr;
	MemoryStorageManager storage_mgr;
	PgLogCollector collector;
	std::stringstream str;
	size_t calls;
	bool truncated;
	TesterLogCollector(const std::string &statename)
		: state_mgr(statename), calls(0), truncated(false)
	{}
	std::string default_metadata() {
		/* TODO: Rethink how to proper handle this... Seriously, this way is just stupid! */
		std::ostringstream ostr;
		ostr
			<< "# snap_type pg_log\n"
			<< "# customer_name " << Util::escapeString(ServerInfo::instance()->currentServerConfig()->userConfig()->customer()) << "\n"
			<< "# server_name " << Util::escapeString(ServerInfo::instance()->currentServerConfig()->hostname()) << "\n"
			<< "# datetime " << Util::MainApplication::instance()->startTime() << "\n"
			<< "# real_datetime " << Util::time::now() << "\n"
			<< "\n"
			;
		return ostr.str();
	}
	void truncate() {
		truncated = true;
		str.str("");
		str.clear();
	}
	void feed(
		const std::string &content,
		const std::string &last_line,
		const std::string &last_line_complete,
		size_t last_line_pos,
		const std::string &expected_content,
		size_t call_source
	) {
		StateManager::Map expected_state;
		StateManager::Map state;
		std::string expected_storage;
		std::string oldpos;
		state = state_mgr.load();
		if (truncated || state.find("llp") == state.end()) {
			oldpos = "0";
		} else {
			oldpos = state["llp"];
		}
		truncated = false;
		state = state_mgr.load();
		expected_state["ll"] = last_line;
		expected_state["llc"] = last_line_complete;
		expected_state["llp"] = Util::numberToString(last_line_pos);
		expected_storage =
			"# filename <stream>\n"
			"# oll " + state["ll"] + "\n"
			"# ollp " + oldpos + "\n"
			"# nll " + expected_state["ll"] + "\n"
			"# nllp " + expected_state["llp"] + "\n"
			+ default_metadata()
			+ expected_content
			;
		/* Feed the content */
		str << content;
		str.seekg(0, std::ios::beg);
		//DMSG(str.str());
		/* Call the collector */
		collector.processLogStream(str, state_mgr, storage_mgr);
		str.clear();
		str.seekg(0, std::ios::end);
		calls++;
		/* validation */
		std::vector<std::string> &d = storage_mgr.data();
		state = state_mgr.load();
		ASSERT_EQ(calls, d.size()) << "From: " << call_source;
		ASSERT_EQ(expected_storage, d[calls-1]) << "From: " << call_source;
		ASSERT_EQ(expected_state, state) << "From: " << call_source;
	}
	void feed_the_same(size_t call_source) {
		StateManager::Map state_before = state_mgr.load();
		StateManager::Map state_after;
		str.clear();
		str.seekg(0, std::ios::beg);
		collector.processLogStream(str, state_mgr, storage_mgr);
		std::vector<std::string> &d = storage_mgr.data();
		state_after = state_mgr.load();
		/* The storage size didn't changed, which means that it did not created a storage file */
		ASSERT_EQ(calls, d.size()) << "From: " << call_source;
		ASSERT_EQ(state_before, state_after);
	}
};

TEST(TestPgLogCollector, OneLine) {
	TesterLogCollector tester("OneLine");
	const std::string the_one_line = "2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line";
	tester.feed(the_one_line + "\n", the_one_line, the_one_line, 0, the_one_line + "\n", __LINE__);
}

TEST(TestPgLogCollector, NonChangedFile) {
	TesterLogCollector tester("NonChangedFile");
	const std::string the_one_line = "2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line";
	tester.feed(the_one_line + "\n", the_one_line, the_one_line, 0, the_one_line + "\n", __LINE__);
	tester.feed_the_same(__LINE__);
}

TEST(TestPgLogCollector, LastLineMultiLine) {
	TesterLogCollector tester("LastLineMultiLine");
	const std::string first_content = 
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm line 2\n"
		"\tline 2.1\n"
		"\tline 2.2"
		;
	const std::string last_line =
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the last line";
	const std::string last_line_complete =
		last_line + "\n"
		"\tI'm another log line of the last line\n"
		"\tThis is quite confusing, no?";
	const std::string add_to_last_line =
		"\tWhat? Wasn't I done?";
	const std::string add_one_more_line =
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  A new last line... uhuuu";
	tester.feed(
		first_content + "\n" + last_line_complete + "\n",
		last_line,
		last_line_complete,
		(first_content + "\n").size(),
		first_content + "\n" + last_line_complete + "\n",
		__LINE__
		);
	tester.feed(
		add_to_last_line + "\n",
		last_line,
		last_line_complete + "\n" + add_to_last_line,
		(first_content + "\n").size(),
		last_line_complete + "\n" + add_to_last_line + "\n",
		__LINE__
		);
}

TEST(TestPgLogCollector, LastLineIncomplete) {
	TesterLogCollector tester("LastLineIncomplete");
	const std::string first_content = 
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm line 2\n"
		"\tline 2.1\n"
		"\tline 2.2"
		;
	const std::string last_line =
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the last line";
	const std::string last_line_complete =
		last_line + "\n"
		"\tI'm another log line of the last line\n"
		"\tThis is quite confusing, no?";
	std::string last_line_incomplete =
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I could be the last line, but I'm not complete";
	/* Feed everything, but with an incomplete last line (e.g. no "\n" at the end) */
	tester.feed(
		first_content + "\n" + last_line_complete + "\n" + last_line_incomplete,
		last_line,
		last_line_complete,
		(first_content + "\n").size(),
		first_content + "\n" + last_line_complete + "\n",
		__LINE__
		);
	/* Well, if it got an incomplete line above, let's complete it here and see what happens */
	tester.feed(
		"...\n",
		last_line_incomplete + "...",
		last_line_incomplete + "...",
		(first_content + "\n" + last_line_complete + "\n").size(),
		last_line_incomplete + "...\n",
		__LINE__
		);
	/* Ok. Now add one more line within the same log item */
	tester.feed(
		"\tnew line\n",
		last_line_incomplete + "...",
		last_line_incomplete + "..." + "\n\tnew line",
		(first_content + "\n" + last_line_complete + "\n").size(),
		last_line_incomplete + "...\n\tnew line\n",
		__LINE__
		);
}

TEST(TestPgLogCollector, FileTruncated) {
	TesterLogCollector tester("OneLine");
	const std::string last_line_1 = "2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the last line";
	const std::string log1 =
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm line 2\n"
		"\tline 2.1\n"
		"\tline 2.2\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm line 3\n"
		"\tline 3.1\n"
		"\tline 3.2\n"
		;
	const std::string last_line_2 = "2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the last line";
	const std::string log2 =
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line\n"
		;
	tester.feed(log1 + last_line_1 + "\n", last_line_1, last_line_1, log1.size(), log1 + last_line_1 + "\n", __LINE__);
	/* Erase */
	tester.truncate();
	tester.feed(log2 + last_line_2 + "\n", last_line_2, last_line_2, log2.size(), log2 + last_line_2 + "\n", __LINE__);
}

#ifndef __WIN32__

TEST(TestImporter, ImportLog) {
	PgaImporterApp imp;
	std::stringstream sstr;
	sstr
		<< "# snap_type pg_log\n"
		<< "# llp %t [%p]: [%l-1] user=%u,db=%d,host=%r \n"
		<< "# customer_name " << Util::escapeString(ServerInfo::instance()->currentServerConfig()->userConfig()->customer()) << "\n"
		<< "# server_name " << Util::escapeString(ServerInfo::instance()->currentServerConfig()->hostname()) << "\n"
		<< "# datetime 1414758486\n"
		<< "# real_datetime 1414758486\n"
		<< "\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm the first line\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm line 2\n"
		"\tline 2.1\n"
		"\tline 2.2\n"
		"2014-06-06 12:26:28 BRT [2985]: [75-1] user=,db=,host= LOG:  I'm line 3\n"
		"\tline 3.1\n"
		"\tline 3.2\n"
		;
	std::string expected_result =
		"BEGIN;\n"
		"SET LOCAL ROLE E'ctm_test_imp';\n"
		"SET search_path TO E'ctm_test', 'pganalytics', 'public';\n"
		"SELECT sn_import_snapshot(snap_type     := E'pg_log', customer_name := E'test', server_name   := E'testsrv', snap_hash     := E'test', datetime      := to_timestamp(float E'1414758486'), real_datetime := to_timestamp(float E'1414758486'), instance_name := NULL, datname       := NULL );\n"
		"COPY sn_pglog(application_name,user_name,database_name,remote_host_port,remote_host,pid,log_time,log_time_ms,command_tag,sql_state_code,session_id,session_line_num,session_start_time,virtual_transaction_id,transaction_id,error_severity,message,normalized) FROM stdin;\n"
		"\\N\t\t\t\\N\t\\N\t2985\t2014-06-06 12:26:28 BRT\t\\N\t\\N\t\\N\t\\N\t75\t\\N\t\\N\t\\N\tLOG\tI'm the first line\tI?\n"
		"\\N\t\t\t\\N\t\\N\t2985\t2014-06-06 12:26:28 BRT\t\\N\t\\N\t\\N\t\\N\t75\t\\N\t\\N\t\\N\tLOG\tI'm line 2\\nline 2.1\\nline 2.2\tI?\n"
		"\\N\t\t\t\\N\t\\N\t2985\t2014-06-06 12:26:28 BRT\t\\N\t\\N\t\\N\t\\N\t75\t\\N\t\\N\t\\N\tLOG\tI'm line 3\\nline 3.1\\nline 3.2\tI?\n"
		"\\.\n"
		"SELECT sn_stat_snapshot_finish(currval(pg_get_serial_sequence('sn_stat_snapshot', 'snap_id'))::integer);\n"
		"INSERT INTO log_imports(customer_id, server_id, instance_id, database_id, snap_hash, start_time, end_time, snap_type)\n"
		"SELECT s.customer_id, s.server_id, s.instance_id, s.database_id, E'test', now(), clock_timestamp(), E'pg_log'\n"
		"FROM sn_stat_snapshot s\n"
		"WHERE s.snap_id = currval(pg_get_serial_sequence('sn_stat_snapshot', 'snap_id'))::integer\n"
		"COMMIT;\n";
	std::ostringstream ostr;
	int ret = imp.importStream(sstr, ostr, "test", false);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ(expected_result, ostr.str());
}

#endif

class TestMain : public pga::Util::MainApplication {
public:
	int main() {
		std::istringstream test_config(TEST_CONF_CONTENT);
		UserConfig::parse(test_config);
		return RUN_ALL_TESTS();
	}
};

int main(int argc, char *argv[]) {
	pga::Util::MainApplicationPtr main_obj = new TestMain();
	main_obj->args(argc, argv);
	testing::InitGoogleTest(&argc, argv);
	return static_cast<TestMain*>(main_obj.get())->main();
}

