#pragma once

#include <errno.h>
#include <iostream>
#include <string>
#include <string.h>
#include <errno.h>
#include <execinfo.h>

namespace bblocks {

using namespace std;

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
			cerr << i << " " << strings[i] << endl;
		}

		free(strings);
	}
};


}
