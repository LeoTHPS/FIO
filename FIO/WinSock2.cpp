#include "WinSock2.hpp"

#define FIO_WINSOCK2_VERSION MAKEWORD(2, 2)

FIO::WinSock2::WinSock2()
	: is_loaded(!WSAStartup(FIO_WINSOCK2_VERSION, &data))
{
}
FIO::WinSock2::WinSock2(WinSock2&& ws2)
	: is_loaded(ws2.is_loaded),
	data(ws2.data)
{
	ws2.is_loaded = false;
}
FIO::WinSock2::WinSock2(const WinSock2& ws2)
	: is_loaded(ws2.is_loaded && !WSAStartup(FIO_WINSOCK2_VERSION, &data))
{
}

FIO::WinSock2::~WinSock2()
{
	Unload();
}

void FIO::WinSock2::Unload()
{
	if (is_loaded)
	{
		WSACleanup();

		is_loaded = false;
	}
}

FIO::WinSock2& FIO::WinSock2::operator = (WinSock2&& ws2)
{
	if (is_loaded && !ws2.is_loaded)
		Unload();

	if (is_loaded = ws2.is_loaded)
	{
		data          = ws2.data;
		ws2.is_loaded = false;
	}

	return *this;
}
FIO::WinSock2& FIO::WinSock2::operator = (const WinSock2& ws2)
{
	if (is_loaded && !ws2.is_loaded)
		Unload();

	is_loaded = ws2.is_loaded && !WSAStartup(FIO_WINSOCK2_VERSION, &data);

	return *this;
}
