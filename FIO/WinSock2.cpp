#include "WinSock2.hpp"

FIO::WinSock2::WinSock2()
	: is_loaded(WSAStartup(MAKEWORD(2, 2), &data) == 0)
{
}
FIO::WinSock2::~WinSock2()
{
	if (is_loaded)
	{
		WSACleanup();

		is_loaded = false;
	}
}
