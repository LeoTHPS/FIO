#include "Path.hpp"

#include <cstring>

#if defined(FIO_LINUX)
	#include <unistd.h>

	#include <sys/stat.h>
	#include <sys/types.h>
#elif defined(FIO_WIN32)
	#include <Windows.h>
	#include <Shlwapi.h>
#endif

bool FIO::Path::IsFile(std::string_view path)
{
#if defined(FIO_LINUX)
	struct stat s;

	if (lstat(path.data(), &s) == -1)
		return false;

	if (!S_ISREG(s.st_mode))
		return false;
#elif defined(FIO_WIN32)
	DWORD attributes;

	if ((attributes = GetFileAttributesA(path.data())) == INVALID_FILE_ATTRIBUTES)
		return false;

	if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;
#endif

	return true;
}
#ifdef FIO_WIN32
bool FIO::Path::IsFile(std::wstring_view path)
{
	DWORD attributes;

	if ((attributes = GetFileAttributesW(path.data())) == INVALID_FILE_ATTRIBUTES)
		return false;

	if (attributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;

	return true;
}
#endif

bool FIO::Path::IsDirectory(std::string_view path)
{
#if defined(FIO_LINUX)
	struct stat s;

	if (lstat(path.data(), &s) == -1)
		return false;

	if (!S_ISDIR(s.st_mode))
		return false;
#elif defined(FIO_WIN32)
	DWORD attributes;

	if ((attributes = GetFileAttributesA(path.data())) == INVALID_FILE_ATTRIBUTES)
		return false;

	if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
		return false;
#endif

	return true;
}
#ifdef FIO_WIN32
bool FIO::Path::IsDirectory(std::wstring_view path)
{
	DWORD attributes;

	if ((attributes = GetFileAttributesW(path.data())) == INVALID_FILE_ATTRIBUTES)
		return false;

	if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
		return false;

	return true;
}
#endif

bool FIO::Path::Exists(std::string_view path)
{
#if defined(FIO_LINUX)
	if (access(path.data(), F_OK) == -1)
		return false;
#elif defined(FIO_WIN32)
	if (!PathFileExistsA(path.data()))
		return false;
#endif

	return true;
}
#ifdef FIO_WIN32
bool FIO::Path::Exists(std::wstring_view path)
{
	if (!PathFileExistsW(path.data()))
		return false;

	return true;
}
#endif
