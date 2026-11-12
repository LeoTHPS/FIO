#pragma once
#include <format>
#include <string>

#if defined(FIO_WIN32)
	#include <Windows.h>
#endif

#include "SpinLock.hpp"

namespace FIO
{
	class Console
	{
		static std::string eol;
		static SpinLock    lock;
#if defined(FIO_LINUX)
		static int         error;
#elif defined(FIO_WIN32)
		static DWORD       error;
#endif

		Console() = delete;

	public:
		static auto& GetEOL()
		{
			return eol;
		}

		static auto  GetLastError()
		{
			return error;
		}

		static bool Clear();

		static void SetEOL(std::string_view value);

		static bool SetTitle(std::string_view value);

		static bool Write(std::string_view value);
		template<typename ... TArgs>
		static bool Write(std::string_view format, TArgs ... args)
		{
			return Write(std::vformat(format, std::make_format_args(args ...)));
		}

		static bool WriteLine(std::string_view value);
		template<typename ... TArgs>
		static bool WriteLine(std::string_view format, TArgs ... args)
		{
			return WriteLine(std::vformat(format, std::make_format_args(args ...)));
		}
	};
}
