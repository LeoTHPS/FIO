#pragma once
#include <cstdint>

#if defined(FIO_LINUX)
	#include <chrono>
#endif

#include "TimeSpan.hpp"

namespace FIO
{
	class Timer
	{
#if defined(FIO_LINUX)
		std::chrono::steady_clock::duration start;
#elif defined(FIO_WIN32)
		double                              start;
		double                              frequency;
#endif

	public:
		Timer();

		TimeSpan GetElapsed() const;

		void     Reset();
	};
}
