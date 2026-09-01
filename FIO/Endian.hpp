#pragma once
#include <bit>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace FIO
{
	class Endian
	{
		Endian() = delete;

	public:
		enum : uint16_t
		{
			BIG     = 0x0000,
			LITTLE  = 0x0001,
			MACHINE = ((0x0001 & 0xFFFF) == 0x0001) ? LITTLE : BIG
		};

		template<typename T>
		requires(std::is_enum_v<T> || std::is_integral_v<T> || std::is_floating_point_v<T>)
		static constexpr T Flip(T value)
		{
#pragma pack(push, 1)
			struct Buffer
			{
				uint8_t Bytes[sizeof(T)];
			};
#pragma pack(pop)

			auto buffer = std::bit_cast<Buffer>(value);

			std::ranges::reverse(buffer.Bytes);

			return std::bit_cast<T>(buffer);
		}

		template<typename T>
		static constexpr T HostToNetwork(T value)
		{
			if constexpr (MACHINE != BIG)
				return Flip(value);
			else
				return value;
		}
		template<typename T>
		static constexpr T NetworkToHost(T value)
		{
			if constexpr (MACHINE != BIG)
				return Flip(value);
			else
				return value;
		}
	};
}
