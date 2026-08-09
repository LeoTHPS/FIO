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
		~WinSock2();

		constexpr operator bool () const
		{
			return is_loaded;
		}

		constexpr const WSAData* operator -> () const
		{
			return is_loaded ? &data : nullptr;
		}
	};
}
