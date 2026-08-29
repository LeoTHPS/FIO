#include "DNS.hpp"

#if defined(FIO_LINUX)
	#include <netdb.h>
#elif defined(FIO_WIN32)
	#include "WinSock2.hpp"

	#include <Ws2Tcpip.h>
#endif

template<typename T>
struct fio_dns;
template<>
struct fio_dns<char>
{
	typedef addrinfo info;

	static constexpr const char SERVICE[] = "";

	static constexpr auto       get  = &getaddrinfo;
	static constexpr auto       free = &freeaddrinfo;
};
#if defined(FIO_WIN32)
template<>
struct fio_dns<wchar_t>
{
	typedef addrinfoW info;

	static constexpr const wchar_t SERVICE[] = L"";

	static constexpr auto          get  = &GetAddrInfoW;
	static constexpr auto          free = &FreeAddrInfoW;
};
#endif

// @return 0 on error
// @return -1 on not found
template<typename T_CHAR>
int  fio_dns_begin(typename fio_dns<T_CHAR>::info*& result, std::basic_string_view<T_CHAR> host, int family)
{
	typename fio_dns<T_CHAR>::info hint = { .ai_family = family };

	if (auto error = fio_dns<T_CHAR>::get(host.data(), fio_dns<T_CHAR>::SERVICE, &hint, &result))
	{
		switch (error)
		{
#if defined(FIO_LINUX)
			case EAI_NODATA:
			case EAI_NONAME:
			case EAI_SERVICE:
				return -1;
#elif defined(FIO_WIN32)
			case WSANO_DATA:
			case WSAHOST_NOT_FOUND:
			case WSATYPE_NOT_FOUND:
				return -1;
#endif
		}

		return 0;
	}

	return 1;
}
template<typename T_CHAR>
void fio_dns_cleanup(typename fio_dns<T_CHAR>::info* result)
{
	fio_dns<T_CHAR>::free(result);
}

int  FIO::DNS::Resolve(IPAddress& ip_address, std::string_view host)
{
	return Resolve(ip_address, host, AF_UNSPEC);
}
#if defined(FIO_WIN32)
int  FIO::DNS::Resolve(IPAddress& ip_address, std::wstring_view host)
{
	return Resolve(ip_address, host, AF_UNSPEC);
}
#endif
int  FIO::DNS::Resolve(IPAddress& ip_address, std::string_view host, int family)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif
	addrinfo*     result;

	switch (fio_dns_begin(result, host, family))
	{
		case 0:  return 0;
		case -1: return -1;
	}

	do
		if (IPAddress::FromAddress(ip_address, *result->ai_addr, result->ai_addrlen))
		{
			fio_dns_cleanup<char>(result);

			return 1;
		}
	while (result = result->ai_next);

	fio_dns_cleanup<char>(result);

	return -1;
}
#if defined(FIO_WIN32)
int  FIO::DNS::Resolve(IPAddress& ip_address, std::wstring_view host, int family)
{
	FIO::WinSock2 ws2;
	addrinfoW*    result;

	switch (fio_dns_begin(result, host, family))
	{
		case 0:  return 0;
		case -1: return -1;
	}

	do
		if (IPAddress::FromAddress(ip_address, *result->ai_addr, result->ai_addrlen))
		{
			fio_dns_cleanup<wchar_t>(result);

			return 1;
		}
	while (result = result->ai_next);

	fio_dns_cleanup<wchar_t>(result);

	return -1;
}
#endif

bool FIO::DNS::Enumerate(std::string_view host, const EnumCallback& callback)
{
	return Enumerate(host, AF_UNSPEC, callback);
}
#if defined(FIO_WIN32)
bool FIO::DNS::Enumerate(std::wstring_view host, const EnumCallback& callback)
{
	return Enumerate(host, AF_UNSPEC, callback);
}
#endif
bool FIO::DNS::Enumerate(std::string_view host, int family, const EnumCallback& callback)
{
#if defined(FIO_WIN32)
	FIO::WinSock2 ws2;
#endif
	addrinfo*     result;
	IPAddress     address;

	switch (fio_dns_begin(result, host, family))
	{
		case 0:  return false;
		case -1: return true;
	}

	do
		if (IPAddress::FromAddress(address, *result->ai_addr, result->ai_addrlen))
			if (!callback(address))
				break;
	while (result = result->ai_next);

	fio_dns_cleanup<char>(result);

	return true;
}
#if defined(FIO_WIN32)
bool FIO::DNS::Enumerate(std::wstring_view host, int family, const EnumCallback& callback)
{
	FIO::WinSock2 ws2;
	addrinfoW*    result;
	IPAddress     address;

	switch (fio_dns_begin(result, host, family))
	{
		case 0:  return false;
		case -1: return true;
	}

	do
		if (IPAddress::FromAddress(address, *result->ai_addr, result->ai_addrlen))
			if (!callback(address))
				break;
	while (result = result->ai_next);

	fio_dns_cleanup<wchar_t>(result);

	return true;
}
#endif
