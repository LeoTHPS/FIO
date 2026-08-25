#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <type_traits>

#include "Endian.hpp"

namespace FIO
{
	class ByteBuffer
	{
		template<typename T>
		static constexpr bool is_enum_or_numeric_v = std::is_enum_v<T> || std::is_integral_v<T> || std::is_floating_point_v<T>;

		std::vector<uint8_t> buffer;
		int                  buffer_endian;
		size_t               buffer_capacity;
		bool                 buffer_allocated;

		const uint8_t*       buffer_read;
		size_t               buffer_read_position;

		uint8_t*             buffer_write;
		size_t               buffer_write_position;

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

		uint32_t       GetNextBlockSize() const;

		void SetEndian(int value);

		void SetCapacity(size_t value);

		void SetReadPosition(size_t value);

		void SetWritePosition(size_t value);

		template<typename T>
		requires(is_enum_or_numeric_v<T>)
		bool Peek(T& value) const
		{
			if (!Peek(&value, sizeof(T)))
				return false;

			if (GetEndian() != Endian::MACHINE)
				value = Endian::Flip(value);

			return true;
		}
		bool Peek(std::string& value) const;
		bool Peek(std::wstring& value) const;
		bool Peek(void* buffer, size_t size) const;

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
		bool Read(std::string& value);
		bool Read(std::wstring& value);
		bool Read(void* buffer, size_t size);

		template<typename T>
		requires(is_enum_or_numeric_v<T>)
		bool Write(T value)
		{
			if (GetEndian() != Endian::MACHINE)
				value = Endian::Flip(value);

			return Write(&value, sizeof(T));
		}
		bool Write(std::string_view value);
		bool Write(std::wstring_view value);
		bool Write(const void* buffer, size_t size);

		bool PeekBlock(void* buffer, uint32_t size) const;

		bool ReadBlock(void* buffer, uint32_t size);

		bool WriteBlock(const void* buffer, uint32_t size);

		ByteBuffer& operator = (ByteBuffer&& buffer);
		ByteBuffer& operator = (const ByteBuffer& buffer);
	};
}
