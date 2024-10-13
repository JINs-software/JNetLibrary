#pragma once

#define CRASH(cause)			\
{								\
	__debugbreak();				\
}

#define CRASH_ASSERT(expr)			\
{									\
	if (!(expr))					\
	{								\
		CRASH("ASSERT_CRASH");		\
		__analysis_assume(expr);	\
	}								\
}