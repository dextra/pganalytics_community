
#define forall(i, v) for (typeof (v).begin() i = (v).begin(); i != (v).end(); i++)
#define lengthof(array) (sizeof (array) / sizeof ((array)[0]))

#ifndef TESTER
#include <sstream>
#include <exception>
#include <stdexcept>
#include <string>
#define ASSERT_EXCEPTION(test, exception, what) \
	if (!(test)) {                                 \
		std::ostringstream ostr;                \
		ostr << what;                           \
		throw exception(ostr.str());            \
	}
#else
#include <cassert>
#define ASSERT_EXCEPTION(test, exception, what) assert(test)
#endif

#define ASSERT_INTERNAL_EXCEPTION(test, what) \
	ASSERT_EXCEPTION(test, std::runtime_error, "Internal error - " << what)

