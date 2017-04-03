#ifndef __UTIL_APP_H__
#define __UTIL_APP_H__

#include <vector>
#include <string>
#include <ctime>
#include <stdexcept>

#include "config.h"
#include "SmartPtr.h"

struct option;

BEGIN_APP_NAMESPACE

namespace Util
{
DECLARE_SMART_CLASS(MainApplication);
class MainApplication : public SmartObject
{
protected:
	int m_argc;
	int m_optindex;
	char * *m_argv;
	mutable std::vector<std::string> m_args;
	mutable std::string m_absolutePath;
	static MainApplicationPtr m_instance;
	time_t m_startTime;
public:
	MainApplication();
	virtual ~MainApplication() {}
	static MainApplicationPtr instance();
	template<class T>
	static SmartPtr<T>* instance()
	{
		T *inst = static_cast<T*>(MainApplication::instance());
		if (inst == NULL)
		{
			throw std::runtime_error("invalid data type for MainApplication instance");
		}
		return inst;
	}
	void args(int argc, char *argv[]);
	const std::string &absolutePath() const;
	inline time_t startTime() const
	{
		return this->m_startTime;
	}
protected:
	int argc() const;
	const char * const*argv() const;
	const char *argv(int pos) const;
	const std::vector<std::string> args() const;
	char getopt(struct option *longopts, const std::string &opts);
	char getopt(struct option *longopts);
	const std::vector<std::string> extraArgs() const;
	int optIndex() const;
	void optIndex(int value);
	char *optarg();
	virtual int main() = 0;
	virtual void updateProcessTitle(const std::string &new_title);
public:
	static void printStackTrace(std::ostream &ostr, bool get_position = true);
};
} // namespace Util

END_APP_NAMESPACE

#define MAIN_APPLICATION_ENTRY(clsname)                          \
	int main(int argc, char *argv[]) {                           \
		pga::Util::MainApplicationPtr main_obj = new clsname();  \
		main_obj->args(argc, argv);                              \
		int ret = static_cast<clsname*>(main_obj.get())->main(); \
		return ret;                                              \
	}

#endif // __UTIL_APP_H__

