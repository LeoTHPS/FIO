#include "Path.hpp"
#include "Directory.hpp"

#if defined(FIO_LINUX)
	#include <fcntl.h>
	#include <dirent.h>
	#include <unistd.h>

	#include <sys/stat.h>
	#include <sys/types.h>
#elif defined(FIO_WIN32)
	#include <format>

	#include <Windows.h>
	#include <Shlwapi.h>
#endif

int  FIO::Directory::Create(std::string_view path)
{
#if defined(FIO_LINUX)
	if (mkdir(path.data(), S_IRUSR | S_IWUSR) == -1)
	{
		switch (errno)
		{
			case ENOENT: return -1;
			case EEXIST: return -2;
		}

		return 0;
	}
#elif defined(FIO_WIN32)
	if (!CreateDirectoryA(path.data(), nullptr))
	{
		switch (GetLastError())
		{
			case ERROR_PATH_NOT_FOUND: return -1;
			case ERROR_ALREADY_EXISTS: return -2;
		}

		return 0;
	}
#endif

	return 1;
}
#if defined(FIO_WIN32)
int  FIO::Directory::Create(std::wstring_view path)
{
	if (!CreateDirectoryW(path.data(), nullptr))
	{
		switch (GetLastError())
		{
			case ERROR_PATH_NOT_FOUND: return -1;
			case ERROR_ALREADY_EXISTS: return -2;
		}

		return 0;
	}

	return 1;
}
#endif

int  FIO::Directory::Delete(std::string_view path)
{
#if defined(FIO_LINUX)
	if (rmdir(path.data()) == -1)
	{
		if (errno == ENOENT)
			return -1;

		return 0;
	}
#elif defined(FIO_WIN32)
	if (!RemoveDirectoryA(path.data()))
	{
		if (GetLastError() == ERROR_PATH_NOT_FOUND)
			return -1;

		return 0;
	}
#endif

	return 1;
}
#if defined(FIO_WIN32)
int  FIO::Directory::Delete(std::wstring_view path)
{
	if (!RemoveDirectoryW(path.data()))
	{
		if (GetLastError() == ERROR_PATH_NOT_FOUND)
			return -1;

		return 0;
	}

	return 1;
}
#endif

bool FIO::Directory::Exists(std::string_view path)
{
	return Path::Exists(path) && Path::IsDirectory(path);
}
#if defined(FIO_WIN32)
bool FIO::Directory::Exists(std::wstring_view path)
{
	return Path::Exists(path) && Path::IsDirectory(path);
}
#endif

bool FIO::Directory::Contains(std::string_view path, std::string_view value)
{
	return Path::Exists(Path::Combine(path, value));
}
#if defined(FIO_WIN32)
bool FIO::Directory::Contains(std::wstring_view path, std::wstring_view value)
{
	return Path::Exists(Path::Combine(path, value));
}
#endif

bool FIO::Directory::Enumerate(std::string_view path, const EnumCallback& callback)
{
#if defined(FIO_LINUX)
	DIR*      dir;
	dirent64* dir_entry;

	if (!(dir = opendir(path.data())))
		return false;

	while (dir_entry = readdir64(dir))
	{
		auto entry_path = Path::Combine(path, dir_entry->d_name);
		int  entry_type = (dir_entry->d_type == DT_DIR) ? ENTRY_TYPE_DIRECTORY : ENTRY_TYPE_FILE;

		if (!callback(entry_path, entry_type))
			break;
	}

	closedir(dir);
#elif defined(FIO_WIN32)
	WIN32_FIND_DATAA data;
	HANDLE           handle;
	auto             pattern = std::format("{}/*", path);

	if ((handle = FindFirstFileA(pattern.data(), &data)) == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		auto entry_path = Path::Combine(path, data.cFileName);
		int  entry_type = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? ENTRY_TYPE_DIRECTORY : ENTRY_TYPE_FILE;

		if (!callback(entry_path, entry_type))
			break;
	} while (FindNextFileA(handle, &data));

	if (auto error = GetLastError())
		if (error != ERROR_NO_MORE_FILES)
		{
			FindClose(handle);

			return false;
		}

	FindClose(handle);
#endif

	return true;
}
#if defined(FIO_WIN32)
bool FIO::Directory::Enumerate(std::wstring_view path, const EnumCallbackW& callback)
{
	WIN32_FIND_DATAW data;
	HANDLE           handle;
	auto             pattern = std::format(L"{}/*", path);

	if ((handle = FindFirstFileW(pattern.data(), &data)) == INVALID_HANDLE_VALUE)
		return false;

	do
	{
		auto entry_path = Path::Combine(path, data.cFileName);
		int  entry_type = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? ENTRY_TYPE_DIRECTORY : ENTRY_TYPE_FILE;

		if (!callback(entry_path, entry_type))
			break;
	} while (FindNextFileW(handle, &data));

	if (auto error = GetLastError())
		if (error != ERROR_NO_MORE_FILES)
		{
			FindClose(handle);

			return false;
		}

	FindClose(handle);

	return true;
}
#endif

bool FIO::Directory::GetCurrentPath(std::string& value)
{
#if defined(FIO_LINUX)
	if (auto path = getcwd(nullptr, 0))
	{
		value.assign(path);

		free(path);

		return true;
	}
#elif defined(FIO_WIN32)
	if (auto path_size = GetCurrentDirectoryA(0, nullptr))
	{
		value.resize(path_size - 1);

		if (path_size = GetCurrentDirectoryA(path_size, value.data()))
			return true;
	}
#endif

	return false;
}
#if defined(FIO_WIN32)
bool FIO::Directory::GetCurrentPath(std::wstring& value)
{
	if (auto path_size = GetCurrentDirectoryW(0, nullptr))
	{
		value.resize(path_size - 1);

		if (path_size = GetCurrentDirectoryW(path_size, value.data()))
			return true;
	}

	return false;
}
#endif

bool FIO::Directory::SetCurrentPath(std::string_view value)
{
#if defined(FIO_LINUX)
	if (chdir(value.data()) == -1)
		return false;
#elif defined(FIO_WIN32)
	if (!SetCurrentDirectoryA(value.data()))
		return false;
#endif

	return true;
}
#if defined(FIO_WIN32)
bool FIO::Directory::SetCurrentPath(std::wstring_view value)
{
	if (!SetCurrentDirectoryW(value.data()))
		return false;

	return true;
}
#endif
