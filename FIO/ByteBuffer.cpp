#include "ByteBuffer.hpp"

#include <limits>
#include <cstring>

FIO::ByteBuffer FIO::ByteBuffer::Copy(const void* buffer, size_t size, int endian)
{
	ByteBuffer value(size, endian);

	memcpy(value.buffer, buffer, size);

	return value;
}

FIO::ByteBuffer FIO::ByteBuffer::Open(void* buffer, size_t size, int endian)
{
	ByteBuffer value;
	value.buffer_endian   = endian;
	value.buffer_capacity = size;
	value.buffer_read     = (const uint8_t*)buffer;
	value.buffer_write    = (uint8_t*)buffer;

	return value;
}
FIO::ByteBuffer FIO::ByteBuffer::Open(const void* buffer, size_t size, int endian)
{
	ByteBuffer value;
	value.buffer_endian   = endian;
	value.buffer_capacity = size;
	value.buffer_read     = (const uint8_t*)buffer;

	return value;
}

FIO::ByteBuffer::ByteBuffer()
	: buffer(nullptr),
	buffer_endian(Endian::MACHINE),
	buffer_capacity(0),
	buffer_allocated(false),
	buffer_read(nullptr),
	buffer_read_position(0),
	buffer_write(nullptr),
	buffer_write_position(0)
{
}
FIO::ByteBuffer::ByteBuffer(ByteBuffer&& buffer)
	: buffer(buffer.buffer),
	buffer_endian(buffer.buffer_endian),
	buffer_capacity(buffer.buffer_capacity),
	buffer_allocated(buffer.buffer_allocated),
	buffer_read(buffer.buffer_read),
	buffer_read_position(buffer.buffer_read_position),
	buffer_write(buffer.buffer_write),
	buffer_write_position(buffer.buffer_write_position)
{
	buffer.buffer                = nullptr;
	buffer.buffer_endian         = Endian::MACHINE;
	buffer.buffer_capacity       = 0;
	buffer.buffer_allocated      = false;
	buffer.buffer_read           = nullptr;
	buffer.buffer_read_position  = 0;
	buffer.buffer_write          = nullptr;
	buffer.buffer_write_position = 0;
}
FIO::ByteBuffer::ByteBuffer(const ByteBuffer& buffer)
	: buffer(buffer.buffer ? new uint8_t[buffer.buffer_capacity] : nullptr),
	buffer_endian(buffer.buffer_endian),
	buffer_capacity(buffer.buffer_capacity),
	buffer_allocated(buffer.buffer_allocated),
	buffer_read(this->buffer_allocated ? this->buffer : buffer.buffer_read),
	buffer_read_position(buffer.buffer_read_position),
	buffer_write(this->buffer_allocated ? this->buffer : buffer.buffer_write),
	buffer_write_position(buffer.buffer_write_position)
{
	if (buffer_allocated)
		memcpy(this->buffer, buffer.buffer, buffer.GetCapacity());
}
FIO::ByteBuffer::ByteBuffer(size_t capacity, int endian)
	: buffer(new uint8_t[capacity]),
	buffer_endian(endian),
	buffer_capacity(capacity),
	buffer_allocated(true),
	buffer_read(buffer),
	buffer_read_position(0),
	buffer_write(buffer),
	buffer_write_position(0)
{
}

FIO::ByteBuffer::~ByteBuffer()
{
	if (buffer_allocated)
		delete[] buffer;
}

uint64_t FIO::ByteBuffer::GetNextBlockSize() const
{
	uint64_t block_size;
	uint8_t  block_size_width;

	return GetNextBlockSize(block_size, block_size_width) ? block_size : 0;
}

void FIO::ByteBuffer::SetEndian(int value)
{
	buffer_endian = value;
}

void FIO::ByteBuffer::SetCapacity(size_t value)
{
	if (auto capacity = GetCapacity(); value != capacity)
	{
		auto buffer = new uint8_t[value];

		if (this->buffer_read)
			if (value <= capacity)
				memcpy(buffer, this->buffer_read, value);
			else
				memcpy(buffer, this->buffer_read, capacity);

		if (this->buffer_allocated)
			delete[] this->buffer;

		this->buffer           = buffer;
		this->buffer_capacity  = value;
		this->buffer_allocated = true;
		this->buffer_read      = buffer;
		this->buffer_write     = buffer;

		if (buffer_read_position > value)
			buffer_read_position = value;
		if (buffer_write_position > value)
			buffer_write_position = value;
	}
}

void FIO::ByteBuffer::SetReadPosition(size_t value)
{
	if (value > buffer_capacity)
		value = buffer_capacity;

	buffer_read_position = value;
}

void FIO::ByteBuffer::SetWritePosition(size_t value)
{
	if (value > buffer_capacity)
		value = buffer_capacity;

	buffer_write_position = value;
}

bool FIO::ByteBuffer::Peek(std::string& value) const
{
	if (auto size = GetNextBlockSize())
	{
		value.resize(size / sizeof(char));

		return PeekBlock(value.data(), size);
	}

	return false;
}
bool FIO::ByteBuffer::Peek(std::wstring& value) const
{
	if (auto size = GetNextBlockSize())
	{
		value.resize(size / sizeof(wchar_t));

		return PeekBlock(value.data(), size);
	}

	return false;
}
bool FIO::ByteBuffer::Peek(void* buffer, size_t size) const
{
	auto offset = GetReadPosition();

	if (!buffer_read || ((offset + size) > GetCapacity()))
		return false;

	memcpy(buffer, buffer_read + offset, size);

	return true;
}

bool FIO::ByteBuffer::Read(std::string& value)
{
	if (auto size = GetNextBlockSize())
	{
		value.resize(size / sizeof(char));

		return ReadBlock(value.data(), size);
	}

	return false;
}
bool FIO::ByteBuffer::Read(std::wstring& value)
{
	if (auto size = GetNextBlockSize())
	{
		value.resize(size / sizeof(wchar_t));

		return ReadBlock(value.data(), size);
	}

	return false;
}
bool FIO::ByteBuffer::Read(void* buffer, size_t size)
{
	auto offset = GetReadPosition();

	if (!buffer_read || ((offset + size) > GetCapacity()))
		return false;

	memcpy(buffer, buffer_read + offset, size);

	buffer_read_position += size;

	return true;
}

bool FIO::ByteBuffer::Write(std::string_view value)
{
	auto size = value.length() * sizeof(char);

	return (size <= UINT32_MAX) && WriteBlock(value.data(), size);
}
bool FIO::ByteBuffer::Write(std::wstring_view value)
{
	auto size = value.length() * sizeof(wchar_t);

	return (size <= UINT32_MAX) && WriteBlock(value.data(), size);
}
bool FIO::ByteBuffer::Write(const void* buffer, size_t size)
{
	auto offset = GetWritePosition();

	if (!buffer_write || ((offset + size) > GetCapacity()))
		return false;

	memcpy(buffer_write + offset, buffer, size);

	buffer_write_position += size;

	return true;
}

bool FIO::ByteBuffer::PeekBlock(void* buffer, size_t size) const
{
	auto     offset = GetReadPosition();
	uint64_t block_size;
	uint8_t  block_size_width;

	if (!buffer_read || ((offset + block_size_width + block_size) > GetCapacity()) || (size < block_size))
		return false;

	if (!GetNextBlockSize(block_size, block_size_width))
		return false;

	memcpy(buffer, buffer_read + offset + block_size_width, block_size);

	return true;
}

bool FIO::ByteBuffer::ReadBlock(void* buffer, size_t size)
{
	auto     offset = GetReadPosition();
	uint64_t block_size;
	uint8_t  block_size_width;

	if (!GetNextBlockSize(block_size, block_size_width))
		return false;

	if (!buffer_read || ((offset + block_size_width + block_size) > GetCapacity()) || (size < block_size))
		return false;

	memcpy(buffer, buffer_read + offset + block_size_width, block_size);

	buffer_read_position += block_size_width + block_size;

	return true;
}

bool FIO::ByteBuffer::WriteBlock(const void* buffer, size_t size)
{
	auto    offset = GetWritePosition();
	uint8_t block_size_width;

	if (!SetNextBlockSize(size, block_size_width))
		return false;

	if (!buffer_write || ((offset + block_size_width + size) > GetCapacity()))
		return false;

	memcpy(buffer_write + offset + block_size_width, buffer, size);

	buffer_write_position += block_size_width + size;

	return true;
}

FIO::ByteBuffer& FIO::ByteBuffer::operator = (ByteBuffer&& buffer)
{
	this->buffer                 = buffer.buffer;
	buffer.buffer                = nullptr;

	this->buffer_endian          = buffer.buffer_endian;
	buffer.buffer_endian         = Endian::MACHINE;

	this->buffer_capacity        = buffer.buffer_capacity;
	buffer.buffer_capacity       = 0;

	this->buffer_allocated       = buffer.buffer_allocated;
	buffer.buffer_allocated      = false;

	this->buffer_read            = buffer.buffer_read;
	buffer.buffer_read           = nullptr;

	this->buffer_read_position   = buffer.buffer_read_position;
	buffer.buffer_read_position  = 0;

	this->buffer_write           = buffer.buffer_write;
	buffer.buffer_write          = nullptr;

	this->buffer_write_position  = buffer.buffer_write_position;
	buffer.buffer_write_position = 0;

	return *this;
}
FIO::ByteBuffer& FIO::ByteBuffer::operator = (const ByteBuffer& buffer)
{
	this->buffer                = buffer.buffer ? new uint8_t[buffer.buffer_capacity] : nullptr;
	this->buffer_endian         = buffer.buffer_endian;
	this->buffer_capacity       = buffer.buffer_capacity;
	this->buffer_allocated      = buffer.buffer_allocated;
	this->buffer_read           = this->buffer_allocated ? this->buffer : buffer.buffer_read;
	this->buffer_read_position  = buffer.buffer_read_position;
	this->buffer_write          = this->buffer_allocated ? this->buffer : buffer.buffer_write;
	this->buffer_write_position = buffer.buffer_write_position;

	if (this->buffer_allocated)
		memcpy(this->buffer, buffer.buffer, buffer.GetCapacity());

	return *this;
}

bool FIO::ByteBuffer::GetNextBlockSize(uint64_t& size, uint8_t& width) const
{
	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

	if (!buffer_read || ((offset + sizeof(uint8_t)) > capacity))
		return false;

#define if_block_width(type, value) \
	static_assert(value <= std::numeric_limits<type>::max()); \
	if (buffer_read[offset] == value) \
	{ \
		if ((offset + sizeof(uint8_t) + value) > capacity) \
			return false; \
		\
		size  = *((const type*)&buffer_read[offset + sizeof(uint8_t)]); \
		width = sizeof(uint8_t) + value; \
		\
		if (GetEndian() != Endian::MACHINE) \
			size = Endian::Flip(size); \
		\
		return true; \
	}

	if_block_width(uint8_t,  1);
	if_block_width(uint16_t, 2);
	if_block_width(uint32_t, 3);
	if_block_width(uint32_t, 4);
	if_block_width(uint64_t, 5);
	if_block_width(uint64_t, 6);
	if_block_width(uint64_t, 7);
	if_block_width(uint64_t, 8);

#undef if_block_width

	return false;
}
bool FIO::ByteBuffer::SetNextBlockSize(uint64_t size, uint8_t& width)
{
	if (!buffer_write)
		return false;

	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

#define if_block_size(type, max_value) \
	static_assert(max_value <= std::numeric_limits<type>::max()); \
	if (size <= max_value) \
	{ \
		if ((offset + sizeof(uint8_t) + sizeof(type) + size) > capacity) \
			return false; \
		\
		if (GetEndian() != Endian::MACHINE) \
			size = Endian::Flip(size); \
		\
		width                                             = sizeof(uint8_t) + sizeof(type); \
		buffer_write[offset]                              = sizeof(type); \
		*((type*)&buffer_write[offset + sizeof(uint8_t)]) = (type)size; \
		\
		return true; \
	}

	if_block_size(uint8_t,  0xFF);
	if_block_size(uint16_t, 0xFFFF);
	if_block_size(uint32_t, 0xFFFFFF);
	if_block_size(uint32_t, 0xFFFFFFFF);
	if_block_size(uint64_t, 0xFFFFFFFFFF);
	if_block_size(uint64_t, 0xFFFFFFFFFFFF);
	if_block_size(uint64_t, 0xFFFFFFFFFFFFFF);
	if_block_size(uint64_t, 0xFFFFFFFFFFFFFFFF);

#undef if_block_size

	return false;
}
