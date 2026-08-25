#include "ByteBuffer.hpp"

#include <climits>
#include <cstring>

FIO::ByteBuffer FIO::ByteBuffer::Copy(const void* buffer, size_t size, int endian)
{
	ByteBuffer value(size, endian);

	memcpy(value.buffer.data(), buffer, size);

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
	: buffer(0, 0),
	buffer_endian(Endian::MACHINE),
	buffer_capacity(0),
	buffer_allocated(true),
	buffer_read(buffer.data()),
	buffer_read_position(0),
	buffer_write(buffer.data()),
	buffer_write_position(0)
{
}
FIO::ByteBuffer::ByteBuffer(ByteBuffer&& buffer)
	: buffer(std::move(buffer.buffer)),
	buffer_endian(buffer.buffer_endian),
	buffer_capacity(buffer.buffer_capacity),
	buffer_allocated(buffer.buffer_allocated),
	buffer_read(buffer.buffer_read),
	buffer_read_position(buffer.buffer_read_position),
	buffer_write(buffer.buffer_write),
	buffer_write_position(buffer.buffer_write_position)
{
	buffer.buffer_endian         = Endian::MACHINE;
	buffer.buffer_capacity       = 0;
	buffer.buffer_allocated      = true;
	buffer.buffer_read           = buffer.buffer.data();
	buffer.buffer_read_position  = 0;
	buffer.buffer_write          = buffer.buffer.data();
	buffer.buffer_write_position = 0;
}
FIO::ByteBuffer::ByteBuffer(const ByteBuffer& buffer)
	: buffer(buffer.buffer),
	buffer_endian(buffer.buffer_endian),
	buffer_capacity(buffer.buffer_capacity),
	buffer_allocated(buffer.buffer_allocated),
	buffer_read(this->buffer_allocated ? this->buffer.data() : buffer.buffer_read),
	buffer_read_position(buffer.buffer_read_position),
	buffer_write(this->buffer_allocated ? this->buffer.data() : buffer.buffer_write),
	buffer_write_position(buffer.buffer_write_position)
{
}
FIO::ByteBuffer::ByteBuffer(size_t capacity, int endian)
	: buffer(capacity, 0),
	buffer_endian(endian),
	buffer_capacity(capacity),
	buffer_allocated(true),
	buffer_read(buffer.data()),
	buffer_read_position(0),
	buffer_write(buffer.data()),
	buffer_write_position(0)
{
}

FIO::ByteBuffer::~ByteBuffer()
{
}

uint32_t FIO::ByteBuffer::GetNextBlockSize() const
{
	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

	if (!buffer_read || ((offset + sizeof(uint8_t)) > capacity))
		return 0;

	uint8_t  chunk;
	uint32_t value = 0;

	for (size_t i = 0; i < 32; i += 7)
	{
		if (!(chunk = buffer_read[offset]))
			break;

		value <<= 7;
		value  |= chunk & 0x7F;

		if (!(chunk & 0x80))
			break;

		if (++offset == capacity)
			return 0;
	}

	if (GetEndian() != Endian::MACHINE)
		value = Endian::Flip(value);

	return value;
}

void FIO::ByteBuffer::SetEndian(int value)
{
	buffer_endian = value;
}

void FIO::ByteBuffer::SetCapacity(size_t value)
{
	buffer.resize(value);
	buffer_capacity  = value;
	buffer_allocated = true;
	buffer_read      = buffer.data();
	buffer_write     = buffer.data();

	if (buffer_read_position > value)
		buffer_read_position = value;
	if (buffer_write_position > value)
		buffer_write_position = value;
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

	return (size <= UINT32_MAX) && WriteBlock(value.data(), (uint32_t)size);
}
bool FIO::ByteBuffer::Write(std::wstring_view value)
{
	auto size = value.length() * sizeof(wchar_t);

	return (size <= UINT32_MAX) && WriteBlock(value.data(), (uint32_t)size);
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

bool FIO::ByteBuffer::PeekBlock(void* buffer, uint32_t size) const
{
	auto offset            = GetReadPosition();
	auto block_size        = GetNextBlockSize();
	auto block_size_chunks = (block_size / 0x7F) + ((block_size % 0x7F) ? 1 : 0);

	if (!buffer_read || ((offset + block_size_chunks + block_size) > GetCapacity()) || (size < block_size))
		return false;

	memcpy(buffer, buffer_read + offset + block_size_chunks, block_size);

	return true;
}

bool FIO::ByteBuffer::ReadBlock(void* buffer, uint32_t size)
{
	auto offset            = GetReadPosition();
	auto block_size        = GetNextBlockSize();
	auto block_size_chunks = (block_size / 0x7F) + ((block_size % 0x7F) ? 1 : 0);

	if (!buffer_read || ((offset + block_size_chunks + block_size) > GetCapacity()) || (size < block_size))
		return false;

	memcpy(buffer, buffer_read + offset + block_size_chunks, block_size);

	buffer_read_position += block_size_chunks + block_size;

	return true;
}

bool FIO::ByteBuffer::WriteBlock(const void* buffer, uint32_t size)
{
	auto offset            = GetWritePosition();
	auto block_size        = size;
	auto block_size_chunks = (size / 0x7F) + ((size % 0x7F) ? 1 : 0);

	if (GetEndian() != Endian::MACHINE)
		size = Endian::Flip(size);

	if (!buffer_write || ((offset + block_size_chunks + block_size) > GetCapacity()))
		return 0;

	for (size_t i = 0, j = 1; i < block_size_chunks; ++i, ++j, size >>= 7)
	{
		uint8_t chunk = size & 0x7F;

		if (j < block_size_chunks)
			chunk |= 0x80;

		buffer_write[offset + i] = chunk;
	}

	memcpy(buffer_write + offset + block_size_chunks, buffer, block_size);

	buffer_write_position += block_size_chunks + block_size;

	return true;
}

FIO::ByteBuffer& FIO::ByteBuffer::operator = (ByteBuffer&& buffer)
{
	this->buffer                 = std::move(buffer.buffer);

	this->buffer_endian          = buffer.buffer_endian;
	buffer.buffer_endian         = Endian::MACHINE;

	this->buffer_capacity        = buffer.buffer_capacity;
	buffer.buffer_capacity       = 0;

	this->buffer_allocated       = buffer.buffer_allocated;
	buffer.buffer_allocated      = true;

	this->buffer_read            = buffer.buffer_read;
	buffer.buffer_read           = buffer.buffer.data();

	this->buffer_read_position   = buffer.buffer_read_position;
	buffer.buffer_read_position  = 0;

	this->buffer_write           = buffer.buffer_write;
	buffer.buffer_write          = buffer.buffer.data();

	this->buffer_write_position  = buffer.buffer_write_position;
	buffer.buffer_write_position = 0;

	return *this;
}
FIO::ByteBuffer& FIO::ByteBuffer::operator = (const ByteBuffer& buffer)
{
	this->buffer                = buffer.buffer;
	this->buffer_endian         = buffer.buffer_endian;
	this->buffer_capacity       = buffer.buffer_capacity;
	this->buffer_allocated      = buffer.buffer_allocated;
	this->buffer_read           = this->buffer_allocated ? this->buffer.data() : buffer.buffer_read;
	this->buffer_read_position  = buffer.buffer_read_position;
	this->buffer_write          = this->buffer_allocated ? this->buffer.data() : buffer.buffer_write;
	this->buffer_write_position = buffer.buffer_write_position;

	return *this;
}
