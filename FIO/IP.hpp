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
	enum ADDRESS_FAMILY
	{
		ADDRESS_FAMILY_IP_V4 = AF_INET,
		ADDRESS_FAMILY_IP_V6 = AF_INET6
	};

#pragma pack(push, 1)
	union IPAddress4
	{
		uint8_t  Byte[4];
		uint16_t Word[2];
		uint32_t DWord;
	};
	union IPAddress6
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

	struct IPAddress
	{
		static IPAddress Any;
		static IPAddress Any6;

		static IPAddress Loopback;
		static IPAddress Loopback6;

		static bool FromString(IPAddress& ip_address, std::string_view string);
		static bool FromString(IPAddress& ip_address, int family, std::string_view string);

		static bool FromAddress(IPAddress& ip_address, const sockaddr& address, socklen_t size);

		int            Family;

		union
		{
			IPAddress4 IPv4;
			IPAddress6 IPv6;
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
