#pragma once
#if defined(FIO_LINUX)
	#include <sys/socket.h>

	#include <netinet/in.h>
#elif defined(FIO_WIN32)
	#include <WinSock2.h>

	typedef int socklen_t;
#endif

#include <string>
#include <cstdint>

namespace FIO
{
	struct IPAddress
	{
		enum FAMILY
		{
			FAMILY_V4 = AF_INET,
			FAMILY_V6 = AF_INET6
		};

#pragma pack(push, 1)
		union V4
		{
			uint8_t  Byte[4];
			uint16_t Word[2];
			uint32_t DWord;
		};
		union V6
		{
			uint8_t     Byte[16];
			uint16_t    Word[8];
			uint32_t    DWord[4];
			uint64_t    QWord[2];
#ifdef __SIZEOF_INT128__
			__uint128_t OWord;
#endif
		};
#pragma pack(pop)

		static IPAddress Any;
		static IPAddress Any6;

		static IPAddress Loopback;
		static IPAddress Loopback6;

		static bool FromString(IPAddress& ip_address, std::string_view string);
		static bool FromString(IPAddress& ip_address, int family, std::string_view string);

		static bool FromAddress(IPAddress& ip_address, const sockaddr& address, socklen_t size);

		int    Family;

		union
		{
			V4 IPv4;
			V6 IPv6;
		};

		std::string ToString() const;

		void        ToStorage(sockaddr_storage& storage, socklen_t& size) const;
	};

	struct IPEndPoint
	{
		static IPEndPoint Any(uint16_t port);
		static IPEndPoint Any6(uint16_t port);

		static IPEndPoint Loopback(uint16_t port);
		static IPEndPoint Loopback6(uint16_t port);

		static bool FromAddress(IPEndPoint& ip_end_point, const sockaddr& address, socklen_t size);

		IPAddress Host;
		uint16_t  Port;

		std::string ToString() const;

		void        ToStorage(sockaddr_storage& storage, socklen_t& size) const;
	};
}
