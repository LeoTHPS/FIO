#pragma once
#include <bit>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace FIO
{
	enum ENDIAN
	{
		ENDIAN_BIG     = 0x0000,
		ENDIAN_LITTLE  = 0x0001,
		ENDIAN_MACHINE = ((0x0001 & 0xFFFF) == 0x0001) ? ENDIAN_LITTLE : ENDIAN_BIG
	};

	constexpr int16_t     Flip(int16_t value)
	{
		return (value << 8) | ((value >> 8) & 0x00FF);
	}
	constexpr int32_t     Flip(int32_t value)
	{
		return (value << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | ((value >> 24) & 0x000000FF);
	}
	constexpr int64_t     Flip(int64_t value)
	{
		return (value << 56) | ((value & 0x000000000000FF00) << 40) | ((value & 0x0000000000FF0000) << 24) | ((value & 0x00000000FF000000) << 8) |
				((value & 0x000000FF00000000) >> 8) | ((value & 0x0000FF0000000000) >> 24) | ((value & 0x00FF000000000000) >> 40) | ((value >> 56) & 0x00000000000000FF);
	}
	constexpr uint16_t    Flip(uint16_t value)
	{
		return (value << 8) | (value >> 8);
	}
	constexpr uint32_t    Flip(uint32_t value)
	{
		return (value << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8) | (value >> 24);
	}
	constexpr uint64_t    Flip(uint64_t value)
	{
		return (value << 56) | ((value & 0x000000000000FF00) << 40) | ((value & 0x0000000000FF0000) << 24) | ((value & 0x00000000FF000000) << 8) |
				((value & 0x000000FF00000000) >> 8) | ((value & 0x0000FF0000000000) >> 24) | ((value & 0x00FF000000000000) >> 40) | (value >> 56);
	}
	constexpr float       Flip(float value)
	{
		static_assert(sizeof(float) == sizeof(int32_t));

#pragma pack(push, 1)
		union
		{
			int32_t int32;
			float   value;
		} tmp = { .value = value };
#pragma pack(pop)

		tmp.int32 = Flip(tmp.int32);

		return tmp.value;
	}
	constexpr double      Flip(double value)
	{
		static_assert(sizeof(double) == sizeof(int64_t));

#pragma pack(push, 1)
		union
		{
			int64_t int64;
			double  value;
		} tmp = { .value = value };
#pragma pack(pop)

		tmp.int64 = Flip(tmp.int64);

		return tmp.value;
	}
	constexpr long double Flip(long double value)
	{
		static_assert(!(sizeof(long double) % sizeof(uint64_t)));

#pragma pack(push, 1)
		union
		{
			uint64_t    uint64[sizeof(long double) / sizeof(uint64_t)];
			long double value;
		} tmp = { .value = value };
#pragma pack(pop)

		if constexpr ((sizeof(long double) / sizeof(uint64_t)) == 1)
		{
			tmp.uint64[0] = Flip(tmp.uint64[0]);

			return tmp.value;
		}
		else if constexpr ((sizeof(long double) / sizeof(uint64_t)) == 2)
		{
			tmp.uint64[0] = Flip(tmp.uint64[0]);
			tmp.uint64[1] = Flip(tmp.uint64[1]);

			std::swap(tmp.uint64[0], tmp.uint64[1]);

			return tmp.value;
		}
	}
	template<typename T>
	requires(std::is_enum_v<T> || std::is_integral_v<T> || std::is_floating_point_v<T>)
	constexpr T           Flip(T value)
	{
		if constexpr (std::is_enum<T>::value || std::is_integral<T>::value)
			if constexpr (sizeof(T) == sizeof(uint8_t))
				return value;
			else if constexpr (sizeof(T) == sizeof(uint16_t))
				return std::bit_cast<T>(Flip(std::bit_cast<uint16_t>(value)));
			else if constexpr (sizeof(T) == sizeof(uint32_t))
				return std::bit_cast<T>(Flip(std::bit_cast<uint32_t>(value)));
			else if constexpr (sizeof(T) == sizeof(uint64_t))
				return std::bit_cast<T>(Flip(std::bit_cast<uint64_t>(value)));
	}

	template<typename T>
	constexpr T           HostToNetwork(T value)
	{
		if constexpr (ENDIAN_MACHINE != ENDIAN_BIG)
			return Flip(value);
		else
			return value;
	}
	template<typename T>
	constexpr T           NetworkToHost(T value)
	{
		if constexpr (ENDIAN_MACHINE != ENDIAN_BIG)
			return Flip(value);
		else
			return value;
	}
}
