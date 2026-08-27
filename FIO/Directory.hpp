#pragma once
#include <string>
#include <functional>

namespace FIO
{
	class Directory
	{
		Directory() = delete;

	public:
		// @return false to stop enumeration
		typedef std::function<bool(std::string_view path, int type)>  EnumCallback;
#if defined(FIO_WIN32)
		// @return false to stop enumeration
		typedef std::function<bool(std::wstring_view path, int type)> EnumCallbackW;
#endif

		enum ENTRY_TYPE
		{
			ENTRY_TYPE_FILE,
			ENTRY_TYPE_DIRECTORY
		};

		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Create(std::string_view path);
#if defined(FIO_WIN32)
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Create(std::wstring_view path);
#endif

		// @return 0 on error
		// @return -1 on not found
		static int  Delete(std::string_view path);
#if defined(FIO_WIN32)
		// @return 0 on error
		// @return -1 on not found
		static int  Delete(std::wstring_view path);
#endif

		static bool Exists(std::string_view path);
#if defined(FIO_WIN32)
		static bool Exists(std::wstring_view path);
#endif

		static bool Contains(std::string_view path, std::string_view value);
#if defined(FIO_WIN32)
		static bool Contains(std::wstring_view path, std::wstring_view value);
#endif

		static bool Enumerate(std::string_view path, const EnumCallback& callback);
#if defined(FIO_WIN32)
		static bool Enumerate(std::wstring_view path, const EnumCallbackW& callback);
#endif

		static bool GetCurrentPath(std::string& value);
#if defined(FIO_WIN32)
		static bool GetCurrentPath(std::wstring& value);
#endif

		static bool SetCurrentPath(std::string_view value);
#if defined(FIO_WIN32)
		static bool SetCurrentPath(std::wstring_view value);
#endif
	};
}
