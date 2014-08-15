#pragma once

#include <inttypes.h>
#include <boost/lexical_cast.hpp>
#include <cassert>

#define STR(x) boost::lexical_cast<string>(x)

#define ALIGNED(x) __attribute__((aligned(sizeof(x))))

#if defined(DEBUG_BUILD)
#define DEBUG(path) LogMessage(Logger::LEVEL_DEBUG, path)
#else
#define DEBUG(path) if (1) {} else LogMessage(Logger::LEVEL_DEBUG, path)
#endif

#if !defined(DISABLE_VERBOSE)
#define VERBOSE(path) LogMessage(Logger::LEVEL_VERBOSE, path)
#else
#define VERBOSE(path) if (1) {} else LogMessage(Logger::LEVEL_VERBOSE, path)
#endif

#define ERROR(path) LogMessage(Logger::LEVEL_ERROR, path)
#define INFO(path) LogMessage(Logger::LEVEL_INFO, path)

#define STD_ERROR std::cerr
#define STD_INFO std::cout
#define ENDL std::endl

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



/*
 * Compile helpers
 */
#define __DISABLE_ASSIGN_AND_COPY__(X) \
    X(); \
    X(const X &); \
    X & operator=(const X &); \

/*
 * Computation helpers
 */
#define ROUNDUP(x,y) ((x / y) + (x % y ? 1 : 0)) * y

/*
 * Size notations
 */
#define KiB(x) (x * 1024)
#define MiB(x) (KiB(x) * 1024)
#define GiB(x) (MiB(x) * 1024)
#define TiB(x) (GiB(x) * 1024)

/*
 * Enumerable types T1, T2, ..., Tn
 */
#define COMMA ,
#define SEMICOLON ;
#define OPENBRACKET (
#define CLOSEBRACKET )

#define TDEF(T,n) TDEF_I_##n(T)

#define TDEF_I_1(T) class T##1
#define TDEF_I_2(T) class T##2 COMMA TDEF_I_1(T)
#define TDEF_I_3(T) class T##3 COMMA TDEF_I_2(T)
#define TDEF_I_4(T) class T##4 COMMA TDEF_I_3(T)

#define TENUM(T,n) TENUM_I_##n(T)

#define TENUM_I_1(T) T##1
#define TENUM_I_2(T) T##2 COMMA TENUM_I_1(T)
#define TENUM_I_3(T) T##3 COMMA TENUM_I_2(T)
#define TENUM_I_4(T) T##4 COMMA TENUM_I_3(T)

#define TPARAM(T,t,n) TPARAM_I_##n(T,t)

#define TPARAM_I_1(T,t) const T##1 t1
#define TPARAM_I_2(T,t) const T##2 t2 COMMA TPARAM_I_1(T,t)
#define TPARAM_I_3(T,t) const T##3 t3 COMMA TPARAM_I_2(T,t)
#define TPARAM_I_4(T,t) const T##4 t4 COMMA TPARAM_I_3(T,t)

#define TARG(t,n) TARGEX(t,/*nosfix*/,n)

#define TARGEX(t,sfix,n) TARGEX_I_##n(t,sfix)

#define TARGEX_I_1(t,sfix) t##1##sfix
#define TARGEX_I_2(t,sfix) t##2##sfix COMMA TARGEX_I_1(t,sfix)
#define TARGEX_I_3(t,sfix) t##3##sfix COMMA TARGEX_I_2(t,sfix)
#define TARGEX_I_4(t,sfix) t##4##sfix COMMA TARGEX_I_3(t,sfix)

#define TMEMBERDEF(T,t,n) TMEMBERDEF_I_##n(T,t)

#define TMEMBERDEF_I_1(T,t) T##1 t1_ SEMICOLON
#define TMEMBERDEF_I_2(T,t) T##2 t2_ SEMICOLON TMEMBERDEF_I_1(T,t)
#define TMEMBERDEF_I_3(T,t) T##3 t3_ SEMICOLON TMEMBERDEF_I_2(T,t)
#define TMEMBERDEF_I_4(T,t) T##4 t4_ SEMICOLON TMEMBERDEF_I_3(T,t)

#define TASSIGN(t,n) TASSIGN_I_##n(t)

#define TASSIGN_I_1(t) t##1_ OPENBRACKET t1 CLOSEBRACKET
#define TASSIGN_I_2(t) t##2_ OPENBRACKET t2 CLOSEBRACKET COMMA TASSIGN_I_1(t)
#define TASSIGN_I_3(t) t##3_ OPENBRACKET t3 CLOSEBRACKET COMMA TASSIGN_I_2(t)
#define TASSIGN_I_4(t) t##4_ OPENBRACKET t4 CLOSEBRACKET COMMA TASSIGN_I_3(t)

typedef int fd_t;
typedef uint64_t diskoff_t;
typedef uint64_t disksize_t;

