#pragma once
#include "IP.hpp"

#include <functional>
#include <string_view>

namespace FIO
{
	// @return false to stop enumeration
	typedef std::function<bool(const IPAddress& ip_address)> DNSEnumCallback;

	class DNS
	{
		DNS() = delete;

	public:
		// @return 0 on error
		// @return -1 on not found
		static int  Resolve(IPAddress& ip_address, std::string_view host, int family);

		static bool Enumerate(std::string_view host, int family, const DNSEnumCallback& callback);
	};
}
