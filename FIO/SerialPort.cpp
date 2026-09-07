#include "SerialPort.hpp"

#if defined(FIO_LINUX)
	#define INVALID_SERIAL_PORT_HANDLE -1

	#define GetLastError()             errno

	#include <fcntl.h>
	#include <unistd.h>
	#include <termios.h>

	#include <sys/ioctl.h>
#elif defined(FIO_WIN32)
	#define INVALID_SERIAL_PORT_HANDLE INVALID_HANDLE_VALUE
#endif

FIO::SerialPort::SerialPort(std::string_view path, uint32_t baud, int flags)
	: is_open(false),
	is_closing(false),
	is_associated(false),
	baud(baud),
	flags(flags),
#if defined(FIO_LINUX)
	path(path),
#elif defined(FIO_WIN32)
	path(path.begin(), path.end()),
#endif
	error(0),
	handle(INVALID_SERIAL_PORT_HANDLE),
	thread_pool(nullptr)
{
}
#if defined(FIO_WIN32)
FIO::SerialPort::SerialPort(std::wstring_view path, uint32_t baud, int flags)
	: is_open(false),
	is_closing(false),
	is_associated(false),
	baud(baud),
	flags(flags),
	path(path),
	error(0),
	handle(INVALID_SERIAL_PORT_HANDLE),
	thread_pool(nullptr)
{
}
#endif

FIO::SerialPort::~SerialPort()
{
	if (IsOpen())
		Close();
}

bool FIO::SerialPort::Open()
{
	if (IsOpen())
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	if ((handle = CreateFileW(GetPath().c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE)
	{
		error = ::GetLastError();

		return false;
	}

	DCB dcb = { .DCBlength = sizeof(DCB) };

	if (!GetCommState(handle, &dcb))
	{
		error = ::GetLastError();

		CloseHandle(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}

	dcb.fBinary       = TRUE;
	dcb.BaudRate      = (DWORD)GetBaud();
	dcb.ByteSize      = 8;
	dcb.StopBits      = ONESTOPBIT;
	dcb.fAbortOnError = TRUE;

	if (auto flags = GetFlags())
	{
		     if (flags & FLAG_PARITY_ODD)          { dcb.Parity = ODDPARITY;   dcb.fParity = TRUE;  }
		else if (flags & FLAG_PARITY_EVEN)         { dcb.Parity = EVENPARITY;  dcb.fParity = TRUE;  }
		else if (flags & FLAG_PARITY_MARK)         { dcb.Parity = MARKPARITY;  dcb.fParity = TRUE;  }
		else if (flags & FLAG_PARITY_SPACE)        { dcb.Parity = SPACEPARITY; dcb.fParity = TRUE;  }
		else if (flags & FLAG_PARITY_DISABLED)     { dcb.Parity = NOPARITY;    dcb.fParity = FALSE; }

		if (flags & FLAG_TWO_STOP_BITS)              dcb.StopBits = TWOSTOPBITS;

		     if (flags & FLAG_CTS_CONTROL_ENABLE)    dcb.fOutxCtsFlow = TRUE;
		else if (flags & FLAG_CTS_CONTROL_ENABLE)    dcb.fOutxCtsFlow = FALSE;

		     if (flags & FLAG_RTS_CONTROL_TOGGLE)    dcb.fRtsControl = RTS_CONTROL_TOGGLE;
		else if (flags & FLAG_RTS_CONTROL_ENABLE)    dcb.fRtsControl = RTS_CONTROL_ENABLE;
		else if (flags & FLAG_RTS_CONTROL_DISABLE)   dcb.fRtsControl = RTS_CONTROL_DISABLE;
		else if (flags & FLAG_RTS_CONTROL_HANDSHAKE) dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
	}

	if (!SetCommState(handle, &dcb))
	{
		error = ::GetLastError();

		CloseHandle(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}

	COMMTIMEOUTS timeouts = { .ReadIntervalTimeout = MAXWORD };

	if (!SetCommTimeouts(handle, &timeouts))
	{
		error = ::GetLastError();

		CloseHandle(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}
#endif

	is_open = true;

	return true;
}
void FIO::SerialPort::Close()
{
	if (IsOpen())
	{
		is_closing = true;

		if (IsAssociated())
			thread_pool_io.Wait();

#if defined(FIO_LINUX)
		close(handle);
#elif defined(FIO_WIN32)
		CloseHandle(handle);
#endif

		if (IsAssociated())
			thread_pool_io.Wait();

		error         = 0;
		handle        = INVALID_SERIAL_PORT_HANDLE;
		thread_pool   = nullptr;

		is_open       = false;
		is_closing    = false;
		is_associated = false;
	}
}

bool FIO::SerialPort::Associate(ThreadPool& pool)
{
	if (!IsOpen() || is_closing)
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	if (!pool.Associate(GetHandle()))
	{
		error = pool.GetLastError();

		return false;
	}
#endif

	thread_pool   = &pool;
	is_associated = true;

	return true;
}

bool FIO::SerialPort::Read(void* buffer, size_t size, size_t& number_of_bytes_read)
{
	if (!IsOpen() || is_closing)
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

	return true;
}
bool FIO::SerialPort::Read(void* buffer, size_t size, ReadCallback&& callback)
{
	if (!IsOpen() || !IsAssociated() || is_closing)
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	auto context = new IOContext_Read
	{
		.IO       = { .Callback = std::bind(&SerialPort::OnRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Buffer   = { .Size = size, .Buffer = buffer },
		.Callback = std::move(callback)
	};

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

bool FIO::SerialPort::Write(const void* buffer, size_t size, size_t& number_of_bytes_written)
{
	if (!IsOpen() || is_closing)
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

	return true;
}
bool FIO::SerialPort::Write(const void* buffer, size_t size, WriteCallback&& callback)
{
	if (!IsOpen() || !IsAssociated() || is_closing)
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	auto context = new IOContext_Write
	{
		.IO       = { .Callback = std::bind(&SerialPort::OnWrite, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Buffer   = { .Size = size, .Buffer = buffer },
		.Callback = std::move(callback)
	};

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

void FIO::SerialPort::OnRead(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
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
void FIO::SerialPort::OnWrite(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
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
