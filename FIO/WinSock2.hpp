#pragma once
#if !defined(FIO_WIN32)
	#error FIO_WIN32 not defined
#endif

#include <WinSock2.h>

namespace FIO
{
	class WinSock2
	{
		bool is_loaded;

	public:
		WinSock2();
		WinSock2(WinSock2&& ws2);
		WinSock2(const WinSock2& ws2);

		~WinSock2();

		constexpr bool IsLoaded() const
		{
			return is_loaded;
		}

		const WSAData* GetData() const;

		size_t         GetReferenceCount() const;

		bool Load();
		void Unload();

		WinSock2& operator = (WinSock2&& ws2);
		WinSock2& operator = (const WinSock2& ws2);
	};
}
