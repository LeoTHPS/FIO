#include "WinSock2.hpp"
#include "SpinLock.hpp"

#include <atomic>

static WSAData          ws2_data;
static FIO::SpinLock    ws2_lock;
static size_t           ws2_ref_count;

static bool ws2_load()
{
	FIO::SpinLockGuard lock(ws2_lock);

	if (!ws2_ref_count && WSAStartup(MAKEWORD(2, 2), &ws2_data))
		return false;

	++ws2_ref_count;

	return true;
}
static void ws2_unload()
{
	FIO::SpinLockGuard lock(ws2_lock);

	if (ws2_ref_count && !--ws2_ref_count)
		WSACleanup();
}

FIO::WinSock2::WinSock2()
	: is_loaded(ws2_load())
{
}
FIO::WinSock2::WinSock2(WinSock2&& ws2)
	: is_loaded(ws2.is_loaded)
{
	ws2.is_loaded = false;
}
FIO::WinSock2::WinSock2(const WinSock2& ws2)
	: is_loaded(ws2.is_loaded && ws2_load())
{
}

FIO::WinSock2::~WinSock2()
{
	Unload();
}

const WSAData* FIO::WinSock2::GetData() const
{
	// don't worry about locking here
	// if IsLoaded() returns true then ws2_data is populated

	return IsLoaded() ? &ws2_data : nullptr;
}

size_t         FIO::WinSock2::GetReferenceCount() const
{
	return IsLoaded() ? ws2_ref_count : 0;
}

bool FIO::WinSock2::Load()
{
	if (IsLoaded())
		return true;

	if (!ws2_load())
		return false;

	is_loaded = true;

	return true;
}
void FIO::WinSock2::Unload()
{
	if (IsLoaded())
	{
		ws2_unload();

		is_loaded = false;
	}
}

FIO::WinSock2& FIO::WinSock2::operator = (WinSock2&& ws2)
{
	Unload();

	is_loaded = ws2.is_loaded;
	ws2.is_loaded = false;

	return *this;
}
FIO::WinSock2& FIO::WinSock2::operator = (const WinSock2& ws2)
{
	Unload();

	is_loaded = ws2.IsLoaded() && ws2_load();

	return *this;
}
