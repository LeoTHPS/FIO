#include "Console.hpp"

#if defined(FIO_LINUX)
	#include <errno.h>
	#include <unistd.h>
#endif

#if defined(FIO_LINUX)
std::string   FIO::Console::eol   = "\n";
FIO::SpinLock FIO::Console::lock;
int           FIO::Console::error = 0;
#elif defined(FIO_WIN32)
std::string   FIO::Console::eol   = "\r\n";
FIO::SpinLock FIO::Console::lock;
DWORD         FIO::Console::error = ERROR_SUCCESS;
#endif

bool FIO::Console::Clear()
{
#if defined(FIO_LINUX)
	return Write("\033[2J\033[3J\033[H");
#elif defined(FIO_WIN32)
	HANDLE handle;

	if ((handle = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
	{
		error = ::GetLastError();

		return false;
	}

	CONSOLE_SCREEN_BUFFER_INFO buffer_info;

	if (!GetConsoleScreenBufferInfo(handle, &buffer_info))
	{
		error = ::GetLastError();

		return false;
	}

	CHAR_INFO  fill          = { .Char = { .AsciiChar = ' ' }, .Attributes = buffer_info.wAttributes };
	COORD      home          = { 0, 0 };
	SMALL_RECT scroll_rect   = { 0, 0, buffer_info.dwSize.X, buffer_info.dwSize.Y };
	COORD      scroll_target = { 0, (SHORT)(0 - buffer_info.dwSize.Y) };

	if (!ScrollConsoleScreenBufferA(handle, &scroll_rect, nullptr, scroll_target, &fill))
	{
		error = ::GetLastError();

		return false;
	}

	if (!SetConsoleCursorPosition(handle, home))
	{
		error = ::GetLastError();

		return false;
	}

	return true;
#endif
}

void FIO::Console::SetEOL(std::string_view value)
{
	eol.assign(value);
}

bool FIO::Console::SetTitle(std::string_view value)
{
#if defined(FIO_LINUX)
	return Write("\033]0;") && Write(value) && Write("\007");
#elif defined(FIO_WIN32)
	return SetConsoleTitleA(value.data()) != 0;
#endif
}

bool FIO::Console::Write(std::string_view value)
{
	auto buffer = (const uint8_t*)value.data();

#if defined(FIO_LINUX)
	ssize_t num_bytes_written   = 0;
	size_t  num_bytes_remaining = value.length() * sizeof(char);

	do
	{
		if ((num_bytes_written = write(STDOUT_FILENO, buffer, num_bytes_remaining)) == -1)
		{
			if (errno == EINTR)
				continue;

			error = errno;

			return false;
		}

		buffer              += num_bytes_written;
		num_bytes_remaining -= num_bytes_written;
	} while (num_bytes_remaining);
#elif defined(FIO_WIN32)
	DWORD  mode;
	HANDLE handle;

	if ((handle = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
	{
		error = ::GetLastError();

		return false;
	}

	if (GetConsoleMode(handle, &mode))
	{
		DWORD  num_chars_written;
		size_t num_chars_remaining = value.length();

		do
		{
			if (!WriteConsoleA(handle, buffer, num_chars_remaining, &num_chars_written, NULL))
			{
				error = ::GetLastError();

				return false;
			}

			buffer              += num_chars_written * sizeof(char);
			num_chars_remaining -= num_chars_written;
		} while (num_chars_remaining);
	}
	else if (::GetLastError() != ERROR_INVALID_HANDLE)
	{
		error = ::GetLastError();

		return false;
	}

	DWORD num_bytes_written   = 0;
	DWORD num_bytes_remaining = value.length() * sizeof(char);

	do
	{
		if (!WriteFile(handle, buffer, num_bytes_remaining, &num_bytes_written, nullptr))
		{
			error = ::GetLastError();

			return false;
		}

		buffer              += num_bytes_written;
		num_bytes_remaining -= num_bytes_written;
	} while (num_bytes_remaining);
#endif

	return true;
}

bool FIO::Console::WriteLine()
{
	return Write(GetEOL());
}
bool FIO::Console::WriteLine(std::string_view value)
{
	return Write(value) && Write(GetEOL());
}
