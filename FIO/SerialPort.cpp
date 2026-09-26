#include "SerialPort.hpp"

#if defined(FIO_LINUX)
	#define INVALID_SERIAL_PORT_HANDLE -1

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

bool FIO::SerialPort::Open(ThreadPool* pool)
{
	if (IsOpen())
		return false;

#if defined(FIO_LINUX)
	if ((handle = open(GetPath().c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
	{
		error = errno;

		return false;
	}

	termios tio;

	if (tcgetattr(handle, &tio) < 0)
	{
		error = errno;

		close(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}

	tio.c_cflag |= CS8 | CLOCAL | CREAD;
	tio.c_cflag &= ~CSIZE;

	// tio.c_cc[VMIN]  = 0;
	// tio.c_cc[VTIME] = 0;

	cfsetspeed(&tio, (speed_t)GetBaud());

	if (auto flags = GetFlags())
	{
		tio.c_cflag &= ~(PARENB | PARODD);

		     if (flags & FLAG_PARITY_ODD)         tio.c_cflag |= PARENB | PARODD;
		else if (flags & FLAG_PARITY_EVEN)        tio.c_cflag |= PARENB;

		if (flags & FLAG_TWO_STOP_BITS)           tio.c_cflag |= CSTOPB;
		else                                      tio.c_cflag &= ~CSTOPB;

		     if (flags & FLAG_CTS_CONTROL_ENABLE) tio.c_cflag |= CRTSCTS;
		else if (flags & FLAG_CTS_CONTROL_ENABLE) tio.c_cflag &= ~CRTSCTS;

		if (flags & FLAG_RTS_CONTROL_ENABLE)
		{
			int mcs;
			ioctl(handle, TIOCMGET, &mcs);
			mcs |= TIOCM_RTS;
			ioctl(handle, TIOCMSET, &mcs);
		}
		else if (flags & FLAG_RTS_CONTROL_DISABLE)
		{
			int mcs;
			ioctl(handle, TIOCMGET, &mcs);
			mcs &= ~TIOCM_RTS;
			ioctl(handle, TIOCMSET, &mcs);
		}
	}

	if (tcsetattr(handle, TCSANOW, &tio) < 0)
	{
		error = errno;

		close(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}
#elif defined(FIO_WIN32)
	DWORD cf_flags = FILE_ATTRIBUTE_NORMAL;

	if (pool != nullptr)
		cf_flags |= FILE_FLAG_OVERLAPPED;

	if ((handle = CreateFileW(GetPath().c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, cf_flags, 0)) == INVALID_HANDLE_VALUE)
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
		     if (flags & FLAG_PARITY_ODD)         { dcb.Parity = ODDPARITY;   dcb.fParity = TRUE;  }
		else if (flags & FLAG_PARITY_EVEN)        { dcb.Parity = EVENPARITY;  dcb.fParity = TRUE;  }
		else if (flags & FLAG_PARITY_DISABLED)    { dcb.Parity = NOPARITY;    dcb.fParity = FALSE; }

		if (flags & FLAG_TWO_STOP_BITS)             dcb.StopBits = TWOSTOPBITS;

		     if (flags & FLAG_CTS_CONTROL_ENABLE)   dcb.fOutxCtsFlow = TRUE;
		else if (flags & FLAG_CTS_CONTROL_ENABLE)   dcb.fOutxCtsFlow = FALSE;

		     if (flags & FLAG_RTS_CONTROL_ENABLE)   dcb.fRtsControl = RTS_CONTROL_ENABLE;
		else if (flags & FLAG_RTS_CONTROL_DISABLE)  dcb.fRtsControl = RTS_CONTROL_DISABLE;
	}

	if (!SetCommState(handle, &dcb))
	{
		error = ::GetLastError();

		CloseHandle(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}

	COMMTIMEOUTS timeouts = { .ReadIntervalTimeout = MAXDWORD };

	if (!SetCommTimeouts(handle, &timeouts))
	{
		error = ::GetLastError();

		CloseHandle(handle);
		handle = INVALID_SERIAL_PORT_HANDLE;

		return false;
	}
#endif

	if (pool != nullptr)
	{
#if defined(FIO_WIN32)
		if (!pool->Associate(GetHandle()))
		{
			error = ::GetLastError();

			CloseHandle(handle);
			handle = INVALID_SERIAL_PORT_HANDLE;

			return false;
		}
#endif

		thread_pool   = pool;
		is_associated = true;
	}

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

bool FIO::SerialPort::Read(void* buffer, size_t size, size_t& number_of_bytes_read)
{
	if (!IsOpen() || is_closing || IsAssociated())
		return false;

#if defined(FIO_LINUX)
	ssize_t num_bytes_read;

	if ((num_bytes_read = read(GetHandle(), buffer, size)) == -1)
	{
		error = errno;

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
	if (!IsOpen() || is_closing || !IsAssociated())
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
	if (!IsOpen() || is_closing || IsAssociated())
		return false;

#if defined(FIO_LINUX)
	ssize_t num_bytes_written;

	if ((num_bytes_written = write(GetHandle(), buffer, size)) == -1)
	{
		error = errno;

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
	if (!IsOpen() || is_closing || !IsAssociated())
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
