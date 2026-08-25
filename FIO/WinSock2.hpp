#pragma once
#if !defined(FIO_WIN32)
	#error FIO_WIN32 not defined
#endif

#include <WinSock2.h>
#include <Windows.h>

namespace FIO
{
	class WinSock2
	{
		bool    is_loaded;

		WSAData data;

	public:
		WinSock2();
		WinSock2(WinSock2&& ws2);
		WinSock2(const WinSock2& ws2);

		~WinSock2();

		void Unload();

		constexpr operator bool () const
		{
			return is_loaded;
		}

		constexpr auto operator -> () const
		{
			return is_loaded ? &data : nullptr;
		}

		WinSock2& operator = (WinSock2&& ws2);
		WinSock2& operator = (const WinSock2& ws2);
	};
}
