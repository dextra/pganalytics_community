#ifndef __PGANALYTICS_MAIN_H__
#define __PGANALYTICS_MAIN_H__

#include "config.h"

BEGIN_APP_NAMESPACE

class PgAnalyticsApp : public pga::Util::MainApplication
{
public:
	int main();
	UserConfigPtr userConfig() const;
	virtual ~PgAnalyticsApp();
protected:
	void version();
	void help(const char *progname);
	void help_cron(const char *progname);
	int action_cron();
	void help_collect(const char *progname);
	int action_collect();
	void help_push(const char *progname);
	int action_push();
	void validate_extra_args();
	void loadConfigFile(const std::string &path = "");
};

END_APP_NAMESPACE

#endif // __PGANALYTICS_MAIN_H__

