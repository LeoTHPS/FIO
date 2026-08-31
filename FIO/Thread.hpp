#pragma once
#include <functional>

#if defined(FIO_LINUX)
	#include <pthread.h>
#elif defined(FIO_WIN32)
	#include <Windows.h>
#endif

namespace FIO
{
	class Thread
	{
	public:
		typedef std::function<void(Thread& thread)> Main;

	private:
		bool       is_open;
		bool       is_running;

		Main       main;
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
		static void OpenCurrent(Thread& thread);

		Thread();

		virtual ~Thread();

		constexpr bool IsOpen() const
		{
			return is_open;
		}

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

		void Close();

		bool Start(Main&& main);
		bool Terminate();

	private:
#if defined(FIO_LINUX)
		static void*        Detour(void* param);
#elif defined(FIO_WIN32)
		static DWORD WINAPI Detour(LPVOID param);
#endif
	};
}
