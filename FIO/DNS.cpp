#include "DNS.hpp"

#if defined(FIO_LINUX)
	#include <netdb.h>
#elif defined(FIO_WIN32)
	#include "WinSock2.hpp"

	#include <Ws2Tcpip.h>
#endif

// @return 0 on error
// @return -1 on not found
int  fio_dns_begin(std::string_view host, int family, addrinfo*& result)
{
	addrinfo hint = { .ai_family = family };

	if (auto error = getaddrinfo(host.data(), "", &hint, &result))
	{
#if defined(FIO_LINUX)
		switch (error)
		{
			case EAI_NODATA:
			case EAI_NONAME:
			case EAI_SERVICE:
				return -1;
		}
#elif defined(FIO_WIN32)
		switch (WSAGetLastError())
		{
			case WSANO_DATA:
			case WSAHOST_NOT_FOUND:
			case WSATYPE_NOT_FOUND:
				return -1;
		}
#endif

		return 0;
	}

	return 1;
}
void fio_dns_cleanup(addrinfo* result)
{
	freeaddrinfo(result);
}

int  FIO::DNS::Resolve(IPAddress& ip_address, std::string_view host, int family)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif
	addrinfo*     result;

	switch (fio_dns_begin(host, family, result))
	{
		case 0:  return 0;
		case -1: return -1;
	}

	do
	{
		if (result->ai_family == family)
		{
			bool success = IPAddress::FromAddress(ip_address, *result->ai_addr, result->ai_addrlen);

			fio_dns_cleanup(result);

			return success ? 1 : 0;
		}
	} while (result = result->ai_next);

	fio_dns_cleanup(result);

	return -1;
}
bool FIO::DNS::Enumerate(std::string_view host, int family, const EnumCallback& callback)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif
	addrinfo*     result;

	switch (fio_dns_begin(host, family, result))
	{
		case 0:  return false;
		case -1: return true;
	}

	IPAddress address;

	do
	{
		if (result->ai_family == family)
		{
			if (!IPAddress::FromAddress(address, *result->ai_addr, result->ai_addrlen))
			{
				fio_dns_cleanup(result);

				return false;
			}

			if (!callback(address))
				break;
		}
	} while (result = result->ai_next);

	fio_dns_cleanup(result);

	return true;
}
