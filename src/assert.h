#ifndef _CORE_ASSERT_H_
#define _CORE_ASSERT_H_

#include <errno.h>
#include <iostream>
#include <string>
#include <string.h>
#include <errno.h>
#include <execinfo.h>

#define STD_ERROR std::cerr
#define STD_INFO std::cout
#define ENDL std::endl

class CompilerHelper
{
public:

	void static PrintBackTrace()
	{
		void * buffers[100];
		char ** strings;

		const int nptrs = backtrace(buffers, 100);
		strings = backtrace_symbols(buffers, nptrs);

		for (int i = 0; i < nptrs; ++i) {
			std::cerr << i << " " << strings[i] << std::endl;
		}

		free(strings);
	}
};

#define DEADEND {\
	STD_ERROR << "Unexpected code path reached. "\
		  << __FILE__ << " : " << __LINE__\
		  << ENDL;\
	abort();\
}

#define NOTIMPLEMENTED {\
	STD_ERROR << "Not implemented. "\
		  << __FILE__ << " : " << __LINE__\
		  << ENDL;\
	abort();\
}

#ifdef DEBUG_BUILD
#define ASSERT(x) {\
	if (!bool(x)) {\
		STD_ERROR << "ASSERT: " << #x << " . "\
			  << __FILE__ << " : " << __LINE__\
			  << " system-error: " << strerror(errno)\
			  << ENDL;\
		abort();\
	}\
}
#else
#define ASSERT(x)
#endif

#define DEFENSIVE_CHECK(x) ASSERT(x)

#define INVARIANT(x) {										\
	if (! bool(x)) {									\
		STD_ERROR << "Invariant condition violated."					\
			  << #x << " " << __FILE__ << ":" << __LINE__				\
			  << " syserror: " << strerror(errno)					\
			  << ENDL;								\
		CompilerHelper::PrintBackTrace();						\
		abort();									\
	}											\
}

#endif

