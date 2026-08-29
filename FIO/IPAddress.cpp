#include "Endian.hpp"
#include "IPAddress.hpp"

#if defined(FIO_LINUX)
	#include <arpa/inet.h>
#elif defined(FIO_WIN32)
	#include "WinSock2.hpp"

	#include <Ws2Tcpip.h>
#endif

#include <format>
#include <cstring>
#include <iterator>
#include <cinttypes>

struct fio_address_family
{
	int   family;

	bool(*ip_address_compare)(const FIO::IPAddress& ip_address1, const FIO::IPAddress& ip_address2);
	void(*ip_address_to_string)(const FIO::IPAddress& ip_address, std::string& string);
	void(*ip_address_to_storage)(const FIO::IPAddress& ip_address, sockaddr_storage& storage, socklen_t& size);
	int(* ip_address_from_string)(FIO::IPAddress& ip_address, std::string_view string);
	bool(*ip_address_from_address)(FIO::IPAddress& ip_address, const sockaddr& address, socklen_t size);

	void(*ip_end_point_to_string)(const FIO::IPEndPoint& ip_end_point, std::string& string);
	void(*ip_end_point_to_storage)(const FIO::IPEndPoint& ip_end_point, sockaddr_storage& storage, socklen_t& size);
	bool(*ip_end_point_from_address)(FIO::IPEndPoint& ip_end_point, const sockaddr& address, socklen_t size);
};

bool fio_ip_address_compare_ipv4(const FIO::IPAddress& ip_address1, const FIO::IPAddress& ip_address2)
{
	return ip_address1.IPv4.DWord == ip_address2.IPv4.DWord;
}
bool fio_ip_address_compare_ipv6(const FIO::IPAddress& ip_address1, const FIO::IPAddress& ip_address2)
{
#if defined(__SIZEOF_INT128__)
	return ip_address1.IPv6.OWord == ip_address2.IPv6.OWord;
#else
	return !memcmp(ip_address1.IPv6.Byte, ip_address2.IPv6.Byte, sizeof(ip_address1.IPv6.Byte));
#endif
}
void fio_ip_address_to_string_ipv4(const FIO::IPAddress& ip_address, std::string& string)
{
	string = std::format("{}.{}.{}.{}", ip_address.IPv4.Byte[0], ip_address.IPv4.Byte[1], ip_address.IPv4.Byte[2], ip_address.IPv4.Byte[3]);
}
void fio_ip_address_to_string_ipv6(const FIO::IPAddress& ip_address, std::string& string)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif

	string.resize(INET6_ADDRSTRLEN, 0);

	if (auto cstring = inet_ntop(AF_INET6, ip_address.IPv6.Byte, string.data(), INET6_ADDRSTRLEN))
		string.resize(strlen(cstring));
}
void fio_ip_address_to_storage_ipv4(const FIO::IPAddress& ip_address, sockaddr_storage& storage, socklen_t& size)
{
	size                                      = sizeof(sockaddr_in);
	((sockaddr_in*)&storage)->sin_port        = 0;
	((sockaddr_in*)&storage)->sin_addr.s_addr = ip_address.IPv4.DWord;
	((sockaddr_in*)&storage)->sin_family      = AF_INET;
	memset(((sockaddr_in*)&storage)->sin_zero, 0, sizeof(((sockaddr_in*)&storage)->sin_zero));
}
void fio_ip_address_to_storage_ipv6(const FIO::IPAddress& ip_address, sockaddr_storage& storage, socklen_t& size)
{
	size                                     = sizeof(sockaddr_in6);
	((sockaddr_in6*)&storage)->sin6_port     = 0;
	((sockaddr_in6*)&storage)->sin6_family   = AF_INET6;
	((sockaddr_in6*)&storage)->sin6_scope_id = 0;
	((sockaddr_in6*)&storage)->sin6_flowinfo = 0;
	memcpy(((sockaddr_in6*)&storage)->sin6_addr.s6_addr, ip_address.IPv6.Byte, 16);
}
int  fio_ip_address_from_string_ipv4(FIO::IPAddress& ip_address, std::string_view string)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif

	switch (inet_pton(AF_INET, string.data(), &ip_address.IPv4))
	{
		case 0:  return 0;
		case -1: return -1;
	}

	ip_address.Family = FIO::IPAddress::FAMILY_V4;

	return 1;
}
int  fio_ip_address_from_string_ipv6(FIO::IPAddress& ip_address, std::string_view string)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif

	switch (inet_pton(AF_INET6, string.data(), &ip_address.IPv6))
	{
		case 0:  return 0;
		case -1: return -1;
	}

	ip_address.Family = FIO::IPAddress::FAMILY_V6;

	return 1;
}
bool fio_ip_address_from_address_ipv4(FIO::IPAddress& ip_address, const sockaddr& address, socklen_t size)
{
	if (size == sizeof(sockaddr_in))
	{
		ip_address.Family     = FIO::IPAddress::FAMILY_V4;
		ip_address.IPv4.DWord = ((const sockaddr_in*)&address)->sin_addr.s_addr;

		return true;
	}

	return false;
}
bool fio_ip_address_from_address_ipv6(FIO::IPAddress& ip_address, const sockaddr& address, socklen_t size)
{
	if (size == sizeof(sockaddr_in6))
	{
		ip_address.Family = FIO::IPAddress::FAMILY_V6;
		memcpy(ip_address.IPv6.Byte, ((const sockaddr_in6*)&address)->sin6_addr.s6_addr, 16);

		return true;
	}

	return false;
}

void fio_ip_end_point_to_string_ipv4(const FIO::IPEndPoint& ip_end_point, std::string& string)
{
	string = std::format("{}.{}.{}.{}:{}", ip_end_point.Host.IPv4.Byte[0], ip_end_point.Host.IPv4.Byte[1], ip_end_point.Host.IPv4.Byte[2], ip_end_point.Host.IPv4.Byte[3], ip_end_point.Port);
}
void fio_ip_end_point_to_string_ipv6(const FIO::IPEndPoint& ip_end_point, std::string& string)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif

	static constexpr size_t PORT_LENGTH = 1 + 5;

	string.resize(INET6_ADDRSTRLEN + PORT_LENGTH, 0);

	if (auto cstring = inet_ntop(AF_INET6, ip_end_point.Host.IPv6.Byte, string.data(), string.size()))
	{
		size_t host_length = strlen(cstring);
		size_t port_length = sprintf(&string[host_length], ":%" PRIu16, ip_end_point.Port);

		string.resize(host_length + port_length);
	}
}
void fio_ip_end_point_to_storage_ipv4(const FIO::IPEndPoint& ip_end_point, sockaddr_storage& storage, socklen_t& size)
{
	size                                      = sizeof(sockaddr_in);
	((sockaddr_in*)&storage)->sin_port        = FIO::Endian::NetworkToHost(ip_end_point.Port);
	((sockaddr_in*)&storage)->sin_addr.s_addr = ip_end_point.Host.IPv4.DWord;
	((sockaddr_in*)&storage)->sin_family      = AF_INET;
	memset(((sockaddr_in*)&storage)->sin_zero, 0, sizeof(((sockaddr_in*)&storage)->sin_zero));
}
void fio_ip_end_point_to_storage_ipv6(const FIO::IPEndPoint& ip_end_point, sockaddr_storage& storage, socklen_t& size)
{
	size                                     = sizeof(sockaddr_in6);
	((sockaddr_in6*)&storage)->sin6_port     = FIO::Endian::HostToNetwork(ip_end_point.Port);
	((sockaddr_in6*)&storage)->sin6_family   = AF_INET6;
	((sockaddr_in6*)&storage)->sin6_scope_id = 0;
	((sockaddr_in6*)&storage)->sin6_flowinfo = 0;
	memcpy(((sockaddr_in6*)&storage)->sin6_addr.s6_addr, ip_end_point.Host.IPv6.Byte, 16);
}
bool fio_ip_end_point_from_address_ipv4(FIO::IPEndPoint& ip_end_point, const sockaddr& address, socklen_t size)
{
	if (size == sizeof(sockaddr_in))
	{
		ip_end_point.Host.Family     = FIO::IPAddress::FAMILY_V4;
		ip_end_point.Host.IPv4.DWord = ((const sockaddr_in*)&address)->sin_addr.s_addr;
		ip_end_point.Port            = FIO::Endian::NetworkToHost(((const sockaddr_in*)&address)->sin_port);

		return true;
	}

	return false;
}
bool fio_ip_end_point_from_address_ipv6(FIO::IPEndPoint& ip_end_point, const sockaddr& address, socklen_t size)
{
	if (size == sizeof(sockaddr_in6))
	{
		ip_end_point.Host.Family = FIO::IPAddress::FAMILY_V6;
		ip_end_point.Port        = FIO::Endian::NetworkToHost(((const sockaddr_in6*)&address)->sin6_port);
		memcpy(ip_end_point.Host.IPv6.Byte, ((const sockaddr_in6*)&address)->sin6_addr.s6_addr, 16);

		return true;
	}

	return false;
}

constexpr fio_address_family FIO_ADDRESS_FAMILY[] =
{
#define DEFINE_FIO_ADDRESS_FAMILY(family, f) \
	{ \
		family, \
		&fio_ip_address_compare_##f, \
		&fio_ip_address_to_string_##f, \
		&fio_ip_address_to_storage_##f, \
		&fio_ip_address_from_string_##f, \
		&fio_ip_address_from_address_##f, \
		&fio_ip_end_point_to_string_##f, \
		&fio_ip_end_point_to_storage_##f, \
		&fio_ip_end_point_from_address_##f \
	}

	DEFINE_FIO_ADDRESS_FAMILY(FIO::IPAddress::FAMILY_V4, ipv4),
	DEFINE_FIO_ADDRESS_FAMILY(FIO::IPAddress::FAMILY_V6, ipv6)
};

static_assert(sizeof(FIO::IPAddress::V4) == sizeof(in_addr));
static_assert(sizeof(FIO::IPAddress::V6) == sizeof(in6_addr));

FIO::IPAddress FIO::IPAddress::Any       = { .Family = FIO::IPAddress::FAMILY_V4, .IPv4 = { .Byte = { (uint8_t)0, 0, 0, 0 } } };
FIO::IPAddress FIO::IPAddress::Any6      = { .Family = FIO::IPAddress::FAMILY_V6, .IPv6 = { .Byte = { (uint8_t)0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } };
FIO::IPAddress FIO::IPAddress::Loopback  = { .Family = FIO::IPAddress::FAMILY_V4, .IPv4 = { .Byte = { (uint8_t)127, 0, 0, 1 } } };
FIO::IPAddress FIO::IPAddress::Loopback6 = { .Family = FIO::IPAddress::FAMILY_V6, .IPv6 = { .Byte = { (uint8_t)0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 } } };
bool           FIO::IPAddress::FromString(IPAddress& ip_address, std::string_view string)
{
	for (auto& af : FIO_ADDRESS_FAMILY)
	{
		switch (af.ip_address_from_string(ip_address, string))
		{
			case 0:  return false;
			case -1: continue;
		}

		return true;
	}

	return false;
}
bool           FIO::IPAddress::FromString(IPAddress& ip_address, std::string_view string, int family)
{
	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == family)
			return af.ip_address_from_string(ip_address, string) > 0;

	return false;
}
bool           FIO::IPAddress::FromAddress(IPAddress& ip_address, const sockaddr& address, socklen_t size)
{
	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == address.sa_family)
			return af.ip_address_from_address(ip_address, address, size);

	return false;
}
std::string    FIO::IPAddress::ToString() const
{
	std::string string;

	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == Family)
		{
			af.ip_address_to_string(*this, string);

			break;
		}

	return string;
}
void           FIO::IPAddress::ToStorage(sockaddr_storage& storage, socklen_t& size) const
{
	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == Family)
			return af.ip_address_to_storage(*this, storage, size);
}
bool           FIO::IPAddress::operator == (const IPAddress& ip_address) const
{
	if (Family != ip_address.Family)
		return false;

	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == Family)
			return af.ip_address_compare(*this, ip_address);

	return false;
}
bool           FIO::IPAddress::operator != (const IPAddress& ip_address) const
{
	return !operator==(ip_address);
}

FIO::IPEndPoint FIO::IPEndPoint::Any(uint16_t port)
{
	return IPEndPoint { .Host = IPAddress::Any, .Port = port };
}
FIO::IPEndPoint FIO::IPEndPoint::Any6(uint16_t port)
{
	return IPEndPoint { .Host = IPAddress::Any6, .Port = port };
}
FIO::IPEndPoint FIO::IPEndPoint::Loopback(uint16_t port)
{
	return IPEndPoint { .Host = IPAddress::Loopback, .Port = port };
}
FIO::IPEndPoint FIO::IPEndPoint::Loopback6(uint16_t port)
{
	return IPEndPoint { .Host = IPAddress::Loopback6, .Port = port };
}
bool            FIO::IPEndPoint::FromAddress(IPEndPoint& ip_end_point, const sockaddr& address, socklen_t size)
{
	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == address.sa_family)
			return af.ip_end_point_from_address(ip_end_point, address, size);

	return false;
}
std::string     FIO::IPEndPoint::ToString() const
{
	std::string string;

	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == Host.Family)
		{
			af.ip_end_point_to_string(*this, string);

			break;
		}

	return string;
}
void            FIO::IPEndPoint::ToStorage(sockaddr_storage& storage, socklen_t& size) const
{
	for (auto& af : FIO_ADDRESS_FAMILY)
		if (af.family == Host.Family)
			return af.ip_end_point_to_storage(*this, storage, size);
}
bool            FIO::IPEndPoint::operator == (const IPEndPoint& ip_end_point) const
{
	if (Port != ip_end_point.Port)
		return false;

	return Host == ip_end_point.Host;
}
bool            FIO::IPEndPoint::operator != (const IPEndPoint& ip_end_point) const
{
	return !operator==(ip_end_point);
}
