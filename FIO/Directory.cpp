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

bool FIO::Directory::Exists(std::string_view path)
{
	return Path::Exists(path) && Path::IsDirectory(path);
}

bool FIO::Directory::Contains(std::string_view path, std::string_view value)
{
	return Path::Exists(Path::Combine(path, value));
}

bool FIO::Directory::Enumerate(std::string_view path, const DirectoryEnumCallback& callback)
{
#if defined(FIO_LINUX)
	DIR*      dir;
	dirent64* dir_entry;

	if (!(dir = opendir(path.data())))
		return false;

	while (dir_entry = readdir64(dir))
	{
		auto entry_path = Path::Combine(path, dir_entry->d_name);
		int  entry_type = (dir_entry->d_type == DT_DIR) ? DIRECTORY_ENTRY_TYPE_DIRECTORY : DIRECTORY_ENTRY_TYPE_FILE;

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
		int  entry_type = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DIRECTORY_ENTRY_TYPE_DIRECTORY : DIRECTORY_ENTRY_TYPE_FILE;

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
