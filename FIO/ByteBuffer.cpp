#include "ByteBuffer.hpp"

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

bool FIO::ByteBuffer::Read(void* buffer, size_t size)
{
	auto offset = GetReadPosition();

	if (!buffer_read || ((offset + size) > GetCapacity()))
		return false;

	memcpy(buffer, buffer_read + offset, size);

	buffer_read_position += size;

	return true;
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

bool FIO::ByteBuffer::ReadBlock(void* buffer, size_t size, size_t& number_of_bytes_read)
{
	auto     offset     = GetReadPosition();
	uint64_t block_size = 0;

	if (ReadPacked(block_size))
	{
		if (block_size <= size)
			if (Read(buffer, (size_t)block_size))
			{
				number_of_bytes_read = block_size;

				return true;
			}

		SetReadPosition(offset);
	}

	return false;
}
bool FIO::ByteBuffer::WriteBlock(const void* buffer, size_t size)
{
	auto offset = GetWritePosition();

	if (WritePacked((uint64_t)size))
	{
		if (Write(buffer, size))
			return true;

		SetWritePosition(offset);
	}

	return false;
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
