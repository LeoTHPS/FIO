#include "ByteBuffer.hpp"

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
	buffer_endian(ENDIAN_MACHINE),
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
	buffer.buffer_endian         = ENDIAN_MACHINE;
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
	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

	for (size_t i = 0, j = offset; j < capacity; ++i, j += sizeof(char))
		if (!*((const char*)&buffer_read[j]))
		{
			value.assign((const char*)&buffer_read[offset], i);

			return true;
		}

	return false;
}
bool FIO::ByteBuffer::Peek(std::wstring& value) const
{
	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

	for (size_t i = 0, j = offset; j < capacity; ++i, j += sizeof(wchar_t))
		if (!*((const wchar_t*)&buffer_read[j]))
		{
			value.assign((const wchar_t*)&buffer_read[offset], i);

			return true;
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
	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

	for (size_t i = 0, j = offset; j < capacity; ++i, j += sizeof(char))
		if (!*((const char*)&buffer_read[j]))
		{
			value.assign((const char*)&buffer_read[offset], i);

			buffer_read_position += (i * sizeof(char)) + sizeof(char);

			return true;
		}

	return false;
}
bool FIO::ByteBuffer::Read(std::wstring& value)
{
	auto offset   = GetReadPosition();
	auto capacity = GetCapacity();

	for (size_t i = 0, j = offset; j < capacity; ++i, j += sizeof(wchar_t))
		if (!*((const wchar_t*)&buffer_read[j]))
		{
			if (GetEndian() == ENDIAN_MACHINE)
				value.assign((const wchar_t*)&buffer_read[offset], i);
			else
			{
				value.resize(i);

				auto src  = (const wchar_t*)&buffer_read[offset];
				auto dest = value.data();

				for (size_t j = 0; j < i; ++j, ++src, ++dest)
					*dest = Flip(*src);
			}

			buffer_read_position += (i * sizeof(wchar_t)) + sizeof(wchar_t);

			return true;
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
	auto size   = value.length() * sizeof(char);
	auto offset = GetWritePosition();

	if (!buffer_write || ((offset + size + sizeof(char)) > GetCapacity()))
		return false;

	memcpy(buffer_write + offset, value.data(), size);
	*((char*)&buffer_write[offset + size]) = 0;

	buffer_write_position += size + sizeof(char);

	return true;
}
bool FIO::ByteBuffer::Write(std::wstring_view value)
{
	auto size   = value.length() * sizeof(wchar_t);
	auto offset = GetWritePosition();

	if (!buffer_write || ((offset + size + sizeof(wchar_t)) > GetCapacity()))
		return false;

	if (GetEndian() == ENDIAN_MACHINE)
		memcpy(buffer_write + offset, value.data(), size);
	else
	{
		auto src  = value.data();
		auto dest = (wchar_t*)&buffer_write[offset];

		for (size_t i = 0; i < value.length(); ++i, ++src, ++dest)
			*dest = Flip(*src);
	}

	*((wchar_t*)&buffer_write[offset + size]) = 0;

	buffer_write_position += size + sizeof(wchar_t);

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

FIO::ByteBuffer& FIO::ByteBuffer::operator = (ByteBuffer&& buffer)
{
	this->buffer                 = std::move(buffer.buffer);

	this->buffer_endian          = buffer.buffer_endian;
	buffer.buffer_endian         = ENDIAN_MACHINE;

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
