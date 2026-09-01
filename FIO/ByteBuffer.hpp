#pragma once
#include <string>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "Endian.hpp"

namespace FIO
{
	class ByteBuffer
	{
		template<typename T>
		static constexpr bool is_integer_v         = std::is_integral_v<T> && !std::is_enum_v<T> && !std::is_floating_point_v<T>;
		template<typename T>
		static constexpr bool is_enum_or_decimal_v = (std::is_enum_v<T> || std::is_floating_point_v<T>) && !std::is_integral_v<T>;
		template<typename T>
		static constexpr bool is_enum_or_numeric_v = std::is_enum_v<T> || std::is_integral_v<T> || std::is_floating_point_v<T>;

		uint8_t*       buffer;
		int            buffer_endian;
		size_t         buffer_capacity;
		bool           buffer_allocated;

		const uint8_t* buffer_read;
		size_t         buffer_read_position;

		uint8_t*       buffer_write;
		size_t         buffer_write_position;

	public:
		static ByteBuffer Copy(const void* buffer, size_t size, int endian);

		static ByteBuffer Open(void* buffer, size_t size, int endian);
		static ByteBuffer Open(const void* buffer, size_t size, int endian);

		ByteBuffer();
		ByteBuffer(ByteBuffer&& buffer);
		ByteBuffer(const ByteBuffer& buffer);
		ByteBuffer(size_t capacity, int endian);

		virtual ~ByteBuffer();

		constexpr bool IsReadOnly() const
		{
			return buffer_write == nullptr;
		}

		constexpr auto GetBuffer() const
		{
			return buffer_read;
		}

		constexpr auto GetEndian() const
		{
			return buffer_endian;
		}

		constexpr auto GetCapacity() const
		{
			return buffer_capacity;
		}

		constexpr auto GetReadPosition() const
		{
			return buffer_read_position;
		}

		constexpr auto GetWritePosition() const
		{
			return buffer_write_position;
		}

		void SetEndian(int value);

		void SetCapacity(size_t value);

		void SetReadPosition(size_t value);

		void SetWritePosition(size_t value);

		template<typename T>
		requires(is_enum_or_numeric_v<T>)
		bool Read(T& value)
		{
			if (!Read(&value, sizeof(T)))
				return false;

			if (GetEndian() != Endian::MACHINE)
				value = Endian::Flip(value);

			return true;
		}
		template<typename T>
		bool Read(std::basic_string<T>& value)
		{
			auto offset = GetReadPosition();

			if (uint64_t size; ReadPacked(size))
			{
				value.resize((size_t)(size / sizeof(T)));

				if (Read(value.data(), (size_t)size))
					return true;

				SetReadPosition(offset);
			}

			return false;
		}
		bool Read(void* buffer, size_t size);

		template<typename T>
		requires(is_enum_or_numeric_v<T>)
		bool Write(T value)
		{
			if (GetEndian() != Endian::MACHINE)
				value = Endian::Flip(value);

			return Write(&value, sizeof(T));
		}
		template<typename T>
		bool Write(std::basic_string_view<T> value)
		{
			return WriteBlock(value.data(), value.length() * sizeof(T));
		}
		template<typename T>
		bool Write(const std::basic_string<T>& value)
		{
			return WriteBlock(value.data(), value.length() * sizeof(T));
		}
		bool Write(const void* buffer, size_t size);

		bool ReadBlock(void* buffer, size_t size, size_t& number_of_bytes_read);

		bool WriteBlock(const void* buffer, size_t size);

		template<typename T>
		requires(is_integer_v<T>)
		bool ReadPacked(T& value)
		{
			if (!buffer_read)
				return false;

			T    v        = 0;
			auto offset   = GetReadPosition();
			auto capacity = GetCapacity();

			for (size_t i = offset, j = 0; (j < (sizeof(T) * 8)) && (i < capacity); ++i, j += 7)
			{
				uint8_t byte = buffer_read[i];

				v |= (T)(byte & 0x7F) << j;

				if ((byte & 0x80) == 0)
				{
					value                = v;
					buffer_read_position = i + 1;

					return true;
				}
			}

			return false;
		}
		template<typename T>
		requires(is_enum_or_decimal_v<T>)
		bool ReadPacked(T& value)
		{
			if      constexpr (sizeof(T) == sizeof(uint8_t))     return ReadPacked((uint8_t&)value);
			else if constexpr (sizeof(T) == sizeof(uint16_t))    return ReadPacked((uint16_t&)value);
			else if constexpr (sizeof(T) == sizeof(uint32_t))    return ReadPacked((uint32_t&)value);
			else if constexpr (sizeof(T) == sizeof(uint64_t))    return ReadPacked((uint64_t&)value);
#ifdef __SIZEOF_INT128__
			else if constexpr (sizeof(T) == sizeof(__uint128_t)) return ReadPacked((__uint128_t&)value);
#endif

			return false;
		}

		template<typename T>
		requires(is_integer_v<T>)
		bool WritePacked(T value)
		{
			if (!buffer_write)
				return false;

			auto offset   = GetReadPosition();
			auto capacity = GetCapacity();

			for (size_t i = offset; i < capacity; ++i)
			{
				uint8_t byte = value & 0x7F;

				if (value >>= 7)
					byte |= 0x80;

				buffer_write[i] = byte;

				if (value == 0)
				{
					buffer_write_position = i + 1;

					return true;
				}
			}

			return false;
		}
		template<typename T>
		requires(is_enum_or_decimal_v<T>)
		bool WritePacked(T value)
		{
			if      constexpr (sizeof(T) == sizeof(uint8_t))     return WritePacked((uint8_t)value);
			else if constexpr (sizeof(T) == sizeof(uint16_t))    return WritePacked((uint16_t)value);
			else if constexpr (sizeof(T) == sizeof(uint32_t))    return WritePacked((uint32_t)value);
			else if constexpr (sizeof(T) == sizeof(uint64_t))    return WritePacked((uint64_t)value);
#ifdef __SIZEOF_INT128__
			else if constexpr (sizeof(T) == sizeof(__uint128_t)) return WritePacked((__uint128_t)value);
#endif

			return false;
		}

		ByteBuffer& operator = (ByteBuffer&& buffer);
		ByteBuffer& operator = (const ByteBuffer& buffer);
	};
}
