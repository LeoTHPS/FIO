#pragma once
#include <climits>
#include <cstdint>

#if defined(FIO_LINUX)
	#include <chrono>
#endif

namespace FIO
{
	class TimeSpan
	{
		uint64_t value;

	public:
		static constexpr uint64_t Zero     = 0;
		static constexpr uint64_t Infinite = UINT64_MAX;

		static constexpr TimeSpan FromNanoseconds(uint64_t value)
		{
			return TimeSpan(value);
		}

		static constexpr TimeSpan FromMicroseconds(uint64_t value)
		{
			return TimeSpan(value * 1000);
		}

		static constexpr TimeSpan FromMilliseconds(uint64_t value)
		{
			return TimeSpan(value * 1000000);
		}

		static constexpr TimeSpan FromSeconds(uint64_t value)
		{
			return TimeSpan(value * 1000000000);
		}

		static constexpr TimeSpan FromMinutes(uint64_t value)
		{
			return TimeSpan(value * 60000000000);
		}

		static constexpr TimeSpan FromHours(uint64_t value)
		{
			return TimeSpan(value * 3600000000000);
		}

		static constexpr TimeSpan FromDays(uint64_t value)
		{
			return TimeSpan(value * 86400000000000);
		}

		constexpr TimeSpan()
			: TimeSpan(Zero)
		{
		}
		constexpr TimeSpan(uint64_t nanoseconds)
			: value(nanoseconds)
		{
		}

		constexpr auto ToNanoseconds() const
		{
			return value;
		}

		constexpr auto ToMicroseconds() const
		{
			return value / 1000;
		}

		constexpr auto ToMilliseconds() const
		{
			return value / 1000000;
		}

		constexpr auto ToSeconds() const
		{
			return value / 1000000000;
		}

		constexpr auto ToMinutes() const
		{
			return value / 60000000000;
		}

		constexpr auto ToHours() const
		{
			return value / 3600000000000;
		}

		constexpr auto ToDays() const
		{
			return value / 86400000000000;
		}

		constexpr bool      operator > (TimeSpan time) const
		{
			return value > time.value;
		}
		constexpr bool      operator < (TimeSpan time) const
		{
			return value < time.value;
		}

		constexpr bool      operator >= (TimeSpan time) const
		{
			return value >= time.value;
		}
		constexpr bool      operator <= (TimeSpan time) const
		{
			return value <= time.value;
		}

		constexpr TimeSpan& operator += (TimeSpan time)
		{
			value += time.value;

			return *this;
		}
		constexpr TimeSpan& operator -= (TimeSpan time)
		{
			auto v = time.value;

			value = (value >= v) ? (value - v) : ((~0 - v) + value);

			return *this;
		}
		constexpr TimeSpan& operator /= (TimeSpan time)
		{
			value /= time.value;

			return *this;
		}
		constexpr TimeSpan& operator *= (TimeSpan time)
		{
			value *= time.value;

			return *this;
		}

		constexpr TimeSpan  operator + (TimeSpan time) const
		{
			TimeSpan _time = *this;
			_time += time;

			return _time;
		}
		constexpr TimeSpan  operator - (TimeSpan time) const
		{
			TimeSpan _time = *this;
			_time -= time;

			return _time;
		}
		constexpr TimeSpan  operator / (TimeSpan time) const
		{
			TimeSpan _time = *this;
			_time /= time;

			return _time;
		}
		constexpr TimeSpan  operator * (TimeSpan time) const
		{
			TimeSpan _time = *this;
			_time *= time;

			return _time;
		}

		constexpr bool      operator == (TimeSpan time) const
		{
			return value == time.value;
		}
		constexpr bool      operator != (TimeSpan time) const
		{
			return value != time.value;
		}
	};

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
