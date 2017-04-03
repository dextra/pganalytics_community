#ifndef _PGA_IMPORTER_H_
#define _PGA_IMPORTER_H_

#include "common.h"
#include "config.h"
#include "util/app.h"

BEGIN_APP_NAMESPACE

DECLARE_SMART_CLASS(PgaImporterApp);
class PgaImporterApp : public Util::MainApplication
{
public:
	int main();
	void help();
	void version();
	int importStream(std::istream &in, std::ostream &out, const std::string &filename, bool input_gziped = false);
	inline void updateProcessTitle(const std::string &new_title)
	{
		Util::MainApplication::updateProcessTitle(new_title);
	}
};

/* Exceptions */

class ImporterException : public std::runtime_error
{
public:
	ImporterException(const std::string &what)
	: std::runtime_error(what)
	{}
};

#define DECLARE_IMPORTER_EXCEPTION(name, what) \
	class name : public ImporterException      \
	{                                          \
	public:                                    \
		name() : ImporterException(what){}     \
		name(const std::string &msg)           \
		: ImporterException(                   \
			std::string(what) + ": " + msg)    \
		{}                                     \
	};

DECLARE_IMPORTER_EXCEPTION(NoDataFoundException, "No data found");
DECLARE_IMPORTER_EXCEPTION(ConnectionClosedException, "Connection closed");
DECLARE_IMPORTER_EXCEPTION(UnsafeCommandException, "Unsafe command (SQL injection?)");
DECLARE_IMPORTER_EXCEPTION(KeyNotFoundException, "Key not found");
DECLARE_IMPORTER_EXCEPTION(ParserException, "Parser error");

END_APP_NAMESPACE

#endif // _PGA_IMPORTER_H_

