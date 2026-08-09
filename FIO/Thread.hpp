#pragma once
#include <functional>

#if defined(FIO_LINUX)
	#include <pthread.h>
#elif defined(FIO_WIN32)
	#include <Windows.h>
#endif

namespace FIO
{
	class Thread;
	class ThreadPool;

	typedef std::function<void()> ThreadMain;

	class Thread
	{
		bool       is_running;

		ThreadMain main;
#if defined(FIO_LINUX)
		int        error;
		pthread_t  handle;
#elif defined(FIO_WIN32)
		DWORD      error;
		HANDLE     handle;
#endif

		Thread(Thread&&) = delete;
		Thread(const Thread&) = delete;

	public:
		Thread();

		virtual ~Thread();

		constexpr bool IsRunning() const
		{
			return is_running;
		}

		constexpr auto GetHandle() const
		{
			return handle;
		}

		constexpr auto GetLastError() const
		{
			return error;
		}

		bool Join();

		bool Start(ThreadMain&& main);
		bool Terminate();

	private:
#if defined(FIO_LINUX)
		static void*        Detour(void* param);
#elif defined(FIO_WIN32)
		static DWORD WINAPI Detour(LPVOID param);
#endif
	};
}
