#pragma once
#include <bit>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace FIO
{
	template<typename T>
	constexpr bool is_enum_v    = std::is_enum_v<T>;
	template<typename T>
	constexpr bool is_float_v   = std::is_floating_point_v<T>;
#ifdef __SIZEOF_INT128__
	template<typename T>
	constexpr bool is_integer_v = std::is_integral_v<T> || std::is_same_v<T, __int128_t> || std::is_same_v<T, __uint128_t>;
#else
	template<typename T>
	constexpr bool is_integer_v = std::is_integral_v<T>;
#endif

	class Endian
	{
#pragma pack(push, 1)
		template<typename T>
		struct Buffer
		{
			uint8_t Bytes[sizeof(T)];
		};
#pragma pack(pop)

		Endian() = delete;

	public:
		enum
		{
			BIG     = (int)std::endian::big,
			LITTLE  = (int)std::endian::little,
			MACHINE = (int)std::endian::native
		};

		static constexpr bool IsBig()
		{
			return MACHINE == BIG;
		}

		static constexpr bool IsLittle()
		{
			return MACHINE == LITTLE;
		}

		template<typename T>
		requires(is_enum_v<T> || is_float_v<T> || is_integer_v<T>)
		static constexpr auto Flip(T value)
		{
			auto buffer = std::bit_cast<Buffer<T>>(value);

			std::ranges::reverse(buffer.Bytes);

			return std::bit_cast<T>(buffer);
		}

		template<typename T>
		static constexpr auto HostToNetwork(T value)
		{
			if constexpr (MACHINE != BIG)
				return Flip(value);
			else
				return value;
		}
		template<typename T>
		static constexpr auto NetworkToHost(T value)
		{
			if constexpr (MACHINE != BIG)
				return Flip(value);
			else
				return value;
		}
	};
}
