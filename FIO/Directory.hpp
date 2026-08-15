#pragma once
#include <string>
#include <functional>

namespace FIO
{
	enum DIRECTORY_ENTRY_TYPE
	{
		DIRECTORY_ENTRY_TYPE_FILE,
		DIRECTORY_ENTRY_TYPE_DIRECTORY
	};

	// @return false to stop enumeration
	typedef std::function<bool(std::string_view path, int type)> DirectoryEnumCallback;

	class Directory
	{
		Directory() = delete;

	public:
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Create(std::string_view path);

		// @return 0 on error
		// @return -1 on not found
		static int  Delete(std::string_view path);

		static bool Exists(std::string_view path);

		static bool Contains(std::string_view path, std::string_view value);

		static bool Enumerate(std::string_view path, const DirectoryEnumCallback& callback);

		static bool GetCurrentPath(std::string& value);
	};
}
