#include "File.hpp"
#include "Path.hpp"
#include "ThreadPool.hpp"

#include <climits>

#if defined(FIO_LINUX)
	#define GetLastError()      errno

	#define INVALID_FILE_HANDLE -1
 
	#include <fcntl.h>
	#include <unistd.h>

	#include <sys/stat.h>
	#include <sys/types.h>
	#include <sys/sendfile.h>
#elif defined(FIO_WIN32)
	#define INVALID_FILE_HANDLE INVALID_HANDLE_VALUE

	#include <shlwapi.h>
	// #include <ntstatus.h>
	// #include <winternl.h>
#endif

int  FIO::File::Copy(std::string_view source, std::string_view destination)
{
#if defined(FIO_LINUX)
	File input(std::string(source),       MODE_READ);
	File output(std::string(destination), MODE_TRUNCATE);

	switch (input.Open())
	{
		case 0:  return 0;
		case -1: return -1;
	}

	switch (output.Open())
	{
		case 0:  return 0;
		case -2: return -2;
	}

	for (loff_t size = input.GetSize(), offset = 0; offset < size; )
	{
		loff_t chunk = size - offset;

		if (chunk > SIZE_MAX)
			chunk = SIZE_MAX;

		if (sendfile64(output.GetHandle(), input.GetHandle(), &offset, chunk) == -1)
			return 0;
	}
#elif defined(FIO_WIN32)
	if (!CopyFileA(source.data(), destination.data(), TRUE))
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}
#endif

	return 1;
}
#ifdef FIO_WIN32
int  FIO::File::Copy(std::wstring_view source, std::wstring_view destination)
{
	if (!CopyFileW(source.data(), destination.data(), TRUE))
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}

	return 1;
}
#endif

int  FIO::File::Move(std::string_view source, std::string_view destination)
{
#if defined(FIO_LINUX)
	File input(std::string(source),       MODE_READ);
	File output(std::string(destination), MODE_TRUNCATE);

	switch (input.Open())
	{
		case 0:  return 0;
		case -1: return -1;
	}

	switch (output.Open())
	{
		case 0:  return 0;
		case -2: return -2;
	}

	for (loff_t size = input.GetSize(), offset = 0; offset < size; )
	{
		loff_t chunk = size - offset;

		if (chunk > SIZE_MAX)
			chunk = SIZE_MAX;

		if (sendfile64(output.GetHandle(), input.GetHandle(), &offset, chunk) == -1)
			return 0;
	}

	input.Close();

	File::Delete(source);
#elif defined(FIO_WIN32)
	if (!MoveFileA(source.data(), destination.data()))
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}
#endif

	return 1;
}
#ifdef FIO_WIN32
int  FIO::File::Move(std::wstring_view source, std::wstring_view destination)
{
	if (!MoveFileW(source.data(), destination.data()))
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}

	return 1;
}
#endif

int  FIO::File::Create(std::string_view path)
{
#if defined(FIO_LINUX)
	int handle;

	if ((handle = open(path.data(), O_CREAT | O_EXCL | O_RDWR)) == INVALID_FILE_HANDLE)
	{
		switch (errno)
		{
			case ENOENT: return -1;
			case EEXIST: return -2;
		}

		return 0;
	}

	close(handle);
#elif defined(FIO_WIN32)
	HANDLE handle;

	if ((handle = CreateFileA(path.data(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)) == INVALID_FILE_HANDLE)
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}

	CloseHandle(handle);
#endif

	return 1;
}
#ifdef FIO_WIN32
int  FIO::File::Create(std::wstring_view path)
{
	HANDLE handle;

	if ((handle = CreateFileW(path.data(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)) == INVALID_FILE_HANDLE)
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}

	CloseHandle(handle);

	return 1;
}
#endif

int  FIO::File::Delete(std::string_view path)
{
#if defined(FIO_LINUX)
	if (unlink(path.data()))
	{
		if (errno == ENOENT)
			return -1;

		return 0;
	}
#elif defined(FIO_WIN32)
	if (!DeleteFileA(path.data()))
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;
		}

		return 0;
	}
#endif

	return 1;
}
#ifdef FIO_WIN32
int  FIO::File::Delete(std::wstring_view path)
{
	if (!DeleteFileW(path.data()))
	{
		switch (::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;
		}

		return 0;
	}

	return 1;
}
#endif

bool FIO::File::Exists(std::string_view path)
{
	return Path::Exists(path) && Path::IsFile(path);
}
#ifdef FIO_WIN32
bool FIO::File::Exists(std::wstring_view path)
{
	return Path::Exists(path) && Path::IsFile(path);
}
#endif

FIO::File::File(std::string_view path, int mode)
	: is_open(false),
	is_closing(false),
	is_associated(false),
	mode(mode),
	size(0),
#if defined(FIO_LINUX)
	path(path),
	error(0),
	handle(INVALID_FILE_HANDLE),
#elif defined(FIO_WIN32)
	path(path.begin(), path.end()),
	error(0),
	handle(INVALID_FILE_HANDLE),
#endif
	position{ 0 },
	position_type(POSITION_TYPE_NONE),
	thread_pool(nullptr)
{
}
#ifdef FIO_WIN32
FIO::File::File(std::wstring_view path, int mode)
	: is_open(false),
	is_closing(false),
	is_associated(false),
	mode(mode),
	size(0),
	path(path),
	error(0),
	handle(INVALID_FILE_HANDLE),
	position{ 0 },
	position_type(POSITION_TYPE_NONE),
	thread_pool(nullptr)
{
}
#endif

FIO::File::~File()
{
	if (IsOpen())
		Close();
}

void FIO::File::SetReadPosition(uint64_t value)
{
	Position_Set(POSITION_TYPE_READ, value);
}

void FIO::File::SetWritePosition(uint64_t value)
{
	Position_Set(POSITION_TYPE_WRITE, value);
}

int  FIO::File::Open()
{
	if (IsOpen())
		return 0;

#if defined(FIO_LINUX)
	int  flags     = O_LARGEFILE;
	bool can_read  = GetMode() & MODE_READ;
	bool can_write = GetMode() & MODE_WRITE;

	if (can_read && can_write)
		flags |= O_RDWR;
	else if (can_read && !can_write)
		flags |= O_RDONLY;
	else if (!can_read && can_write)
		flags |= O_WRONLY;

	// if (GetMode() & MODE_APPEND)   flags |= O_APPEND;
	if (GetMode() & MODE_CREATE)   flags |= O_CREAT;
	if (GetMode() & MODE_TRUNCATE) flags |= O_TRUNC;

	if ((handle = open(GetPath().c_str(), flags)) == INVALID_FILE_HANDLE)
	{
		switch (error = errno)
		{
			case ENOENT:
				return -1;

			case EEXIST:
				return -2;
		}

		return 0;
	}

	struct stat64 info;

	if (fstat64(handle, &info) == -1)
	{
		error = errno;

		close(handle);
		handle = INVALID_FILE_HANDLE;

		return 0;
	}

	size = info.st_size;
#elif defined(FIO_WIN32)
	DWORD share       = 0;
	DWORD access      = 0;
	DWORD disposition = OPEN_EXISTING;

	if (GetMode() & MODE_READ)     access |= FILE_GENERIC_READ;
	if (GetMode() & MODE_WRITE)    access |= FILE_GENERIC_WRITE;
	// if (GetMode() & MODE_APPEND)   access |= FILE_APPEND_DATA;
	if (GetMode() & MODE_CREATE)   disposition = OPEN_ALWAYS;
	if (GetMode() & MODE_TRUNCATE) disposition = CREATE_ALWAYS;

	if ((handle = CreateFileW(GetPath().c_str(), access, share, nullptr, disposition, FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, nullptr)) == INVALID_FILE_HANDLE)
	{
		switch (error = ::GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return -1;

			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				return -2;
		}

		return 0;
	}

	WIN32_FILE_ATTRIBUTE_DATA file_attr_data;

	if (!GetFileAttributesExW(GetPath().c_str(), GET_FILEEX_INFO_LEVELS::GetFileExInfoStandard, &file_attr_data))
	{
		error = ::GetLastError();

		CloseHandle(handle);
		handle = INVALID_FILE_HANDLE;

		return 0;
	}

	size = ((uint64_t)file_attr_data.nFileSizeHigh << 32) | (uint64_t)file_attr_data.nFileSizeLow;
#endif

	is_open = true;

	if (GetMode() & MODE_APPEND)
		Position_Set(POSITION_TYPE_WRITE, GetSize());

	return 1;
}
void FIO::File::Close(bool wait_for_io)
{
	if (IsOpen())
	{
		is_closing = true;

		if (wait_for_io && IsAssociated())
			thread_pool_io.Wait();

#if defined(FIO_LINUX)
		close(handle);
#elif defined(FIO_WIN32)
		CloseHandle(handle);
#endif

		if (IsAssociated())
			thread_pool_io.Wait();

		Position_Reset();

		size          = 0;
		error         = 0;
		handle        = INVALID_FILE_HANDLE;
		thread_pool   = nullptr;

		is_open       = false;
		is_closing    = false;
		is_associated = false;
	}
}

bool FIO::File::Associate(ThreadPool& pool)
{
	if (!IsOpen() || is_closing)
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	if (!pool.Associate(GetHandle()))
		return false;
#endif

	thread_pool   = &pool;
	is_associated = true;

	return true;
}

bool FIO::File::Read(void* buffer, size_t size, size_t& number_of_bytes_read)
{
	if (!IsOpen() || IsWriteOnly() || is_closing)
		return false;

	if (!Position_Select(POSITION_TYPE_READ))
		return false;

#if defined(FIO_LINUX)
	ssize_t num_bytes_read;

	if ((num_bytes_read = read(GetHandle(), buffer, size)) == -1)
	{
		error = ::GetLastError();

		return false;
	}

	number_of_bytes_read = num_bytes_read;
#elif defined(FIO_WIN32)
	DWORD num_bytes_read;

	if (!ReadFile(GetHandle(), (LPVOID)buffer, (DWORD)size, &num_bytes_read, nullptr))
	{
		if ((error = ::GetLastError()) == ERROR_HANDLE_EOF)
		{
			number_of_bytes_read = 0;

			return true;
		}

		return false;
	}

	number_of_bytes_read = num_bytes_read;
#endif

	Position_Increment(POSITION_TYPE_READ, number_of_bytes_read);

	return true;
}
bool FIO::File::Read(void* buffer, size_t size, ReadCallback&& callback)
{
	if (!IsOpen() || IsWriteOnly() || is_closing)
		return false;

	if (!Position_SelectAsync(POSITION_TYPE_READ))
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	auto context = new IOContext_Read
	{
		.IO       = { .Callback = std::bind(&File::OnRead, this, std::placeholders::_1, std::placeholders::_2) },
		.Buffer   = { .Size = size, .Buffer = buffer },
		.Callback = std::move(callback)
	};

	if (auto offset = Position_Get(POSITION_TYPE_READ))
	{
		context->IO.O.Offset     = (DWORD)(offset & UINT32_MAX);
		context->IO.O.OffsetHigh = (DWORD)(offset >> 32);
	}

	thread_pool_io.Add(context->IO);

	if (!ReadFile(GetHandle(), (LPVOID)buffer, (DWORD)size, nullptr, &context->IO.O))
		if ((error = ::GetLastError()) != ERROR_IO_PENDING)
		{
			thread_pool_io.Remove(context->IO);

			delete context;

			return false;
		}
#endif

	return true;
}

bool FIO::File::Write(const void* buffer, size_t size, size_t& number_of_bytes_written)
{
	if (!IsOpen() || IsReadOnly() || is_closing)
		return false;

	if (!Position_Select(POSITION_TYPE_WRITE))
		return false;

#if defined(FIO_LINUX)
	ssize_t num_bytes_written;

	if ((num_bytes_written = write(GetHandle(), buffer, size)) == -1)
	{
		error = ::GetLastError();

		return false;
	}

	number_of_bytes_written = num_bytes_written;
#elif defined(FIO_WIN32)
	DWORD num_bytes_written;

	if (!WriteFile(GetHandle(), (LPCVOID)buffer, (DWORD)size, &num_bytes_written, nullptr))
	{
		error = ::GetLastError();

		return false;
	}

	number_of_bytes_written = num_bytes_written;
#endif

	Position_Increment(POSITION_TYPE_WRITE, number_of_bytes_written);

	return true;
}
bool FIO::File::Write(const void* buffer, size_t size, WriteCallback&& callback)
{
	if (!IsOpen() || IsReadOnly() || is_closing)
		return false;

	if (!Position_SelectAsync(POSITION_TYPE_WRITE))
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	auto context = new IOContext_Write
	{
		.IO       = { .Callback = std::bind(&File::OnWrite, this, std::placeholders::_1, std::placeholders::_2) },
		.Buffer   = { .Size = size, .Buffer = buffer },
		.Callback = std::move(callback)
	};

	if (auto offset = Position_Get(POSITION_TYPE_WRITE))
	{
		context->IO.O.Offset     = (DWORD)(offset & UINT32_MAX);
		context->IO.O.OffsetHigh = (DWORD)(offset >> 32);
	}

	thread_pool_io.Add(context->IO);

	if (!WriteFile(GetHandle(), (LPCVOID)buffer, (DWORD)size, nullptr, &context->IO.O))
		if ((error = ::GetLastError()) != ERROR_IO_PENDING)
		{
			thread_pool_io.Remove(context->IO);

			delete context;

			return false;
		}
#endif

	return true;
}

#define Position_IsValid(value) ((type > POSITION_TYPE_NONE) && (type < POSITION_TYPE_COUNT))
uint64_t FIO::File::Position_Get(int type) const
{
	return Position_IsValid(type) ? position[type].load() : 0;
}
void     FIO::File::Position_Set(int type, uint64_t value)
{
	if (Position_IsValid(type))
		position[type] = value;
}
void     FIO::File::Position_Reset()
{
	position[POSITION_TYPE_READ]  = 0;
	position[POSITION_TYPE_WRITE] = 0;
	position_type                 = POSITION_TYPE_NONE;
}
bool     FIO::File::Position_Select(int type)
{
	if (!Position_IsValid(type))
		return false;

#if defined(FIO_LINUX)
	if (lseek64(GetHandle(), (off_t)position[type], SEEK_SET) == -1)
#elif defined(FIO_WIN32)
	if (!SetFilePointerEx(GetHandle(), { .QuadPart = (LONGLONG)position[type] }, nullptr, FILE_BEGIN))
#endif
	{
		error = ::GetLastError();

		return false;
	}

	position_type = type;

	return true;
}
bool     FIO::File::Position_SelectAsync(int type)
{
	if (!Position_IsValid(type))
		return false;

	position_type = type;

	return true;
}
void     FIO::File::Position_Increment(int type, size_t value)
{
	if (Position_IsValid(type))
	{
		auto size = position[type].fetch_add(value) + value;

		if (type == POSITION_TYPE_WRITE)
			if (size > this->size)
				this->size = size;
	}
}
void     FIO::File::Position_IncrementAsync(int type, size_t value)
{
	if (Position_IsValid(type))
	{
		auto size = position[type].fetch_add(value) + value;

		if (type == POSITION_TYPE_WRITE)
			if (size > this->size)
				this->size = size;
	}
}

void FIO::File::OnRead(ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	Position_IncrementAsync(POSITION_TYPE_READ, number_of_bytes_transferred);

	auto context = (IOContext_Read*)&io;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	this->error = context->IO.O.Internal;

	context->Callback(*this, context->Buffer.Buffer, context->Buffer.Size, number_of_bytes_transferred);
#endif

	thread_pool_io.Remove(context->IO);

	delete context;
}
void FIO::File::OnWrite(ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	Position_IncrementAsync(POSITION_TYPE_WRITE, number_of_bytes_transferred);

	auto context = (IOContext_Write*)&io;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	this->error = context->IO.O.Internal;

	context->Callback(*this, context->Buffer.Buffer, context->Buffer.Size, number_of_bytes_transferred);
#endif

	thread_pool_io.Remove(context->IO);

	delete context;
}
