#include "Timer.hpp"

#if defined(FIO_WIN32)
	#include <Windows.h>
#endif

auto fio_timer_calculate_start()
{
#if defined(FIO_LINUX)
	return std::chrono::steady_clock::now().time_since_epoch();
#elif defined(FIO_WIN32)
	LARGE_INTEGER integer;
	QueryPerformanceCounter(&integer);

	return (double)integer.QuadPart;
#endif
}
auto fio_timer_calculate_frequency()
{
#if defined(FIO_WIN32)
	LARGE_INTEGER integer;
	QueryPerformanceFrequency(&integer);

	return 1000000000.0 / integer.QuadPart;
#endif
}

FIO::Timer::Timer()
	: start(fio_timer_calculate_start())
#if defined(FIO_WIN32)
	,
	frequency(fio_timer_calculate_frequency())
#endif
{
}

FIO::TimeSpan FIO::Timer::GetElapsed() const
{
#if defined(FIO_LINUX)
	auto now = std::chrono::steady_clock::now().time_since_epoch();

	return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
#elif defined(FIO_WIN32)
	LARGE_INTEGER integer;
	QueryPerformanceCounter(&integer);

	return (uint64_t)((integer.QuadPart - start) * frequency);
#endif
}

void          FIO::Timer::Reset()
{
	start = fio_timer_calculate_start();
}
