#include "Thread.hpp"

#if defined(FIO_LINUX)
	#define INVALID_THREAD_HANDLE 0

	#include <cerrno>
#elif defined(FIO_WIN32)
	#define INVALID_THREAD_HANDLE NULL
#endif

void FIO::Thread::OpenCurrent(Thread& thread)
{
	if (thread.IsOpen())
		thread.Close();

	thread.is_open    = true;
	thread.is_running = true;
#if defined(FIO_LINUX)
	thread.handle     = pthread_self();
#elif defined(FIO_WIN32)
	thread.handle     = GetCurrentThread();
#endif
}

FIO::Thread::Thread()
	: is_open(false),
	is_running(false),
	error(0),
	handle(INVALID_THREAD_HANDLE)
{
}

FIO::Thread::~Thread()
{
	if (IsRunning())
		Join();
}

bool FIO::Thread::Join()
{
	if (IsRunning())
	{
#if defined(FIO_LINUX)
		if ((error = pthread_join(GetHandle(), nullptr)) && (error != ESRCH))
			return false;
#elif defined(FIO_WIN32)
		if (WaitForSingleObject(GetHandle(), INFINITE) != WAIT_OBJECT_0)
		{
			error = GetLastError();

			return false;
		}
#endif

		error = 0;
	}

	return true;
}

void FIO::Thread::Close()
{
	if (IsOpen())
	{
#if defined(FIO_LINUX)

#elif defined(FIO_WIN32)
		CloseHandle(GetHandle());
#endif

		error      = 0;
		handle     = INVALID_THREAD_HANDLE;

		is_open    = false;
		is_running = false;
	}
}

bool FIO::Thread::Start(Main&& main)
{
	if (IsRunning())
		return false;

	this->main       = std::move(main);
	this->error      = 0;
	this->is_running = true;

#if defined(FIO_LINUX)
	if (this->error = pthread_create(&this->handle, nullptr, &Detour, this))
	{
		this->is_running = false;

		return false;
	}
#elif defined(FIO_WIN32)
	if ((this->handle = CreateThread(NULL, 0, &Detour, this, 0, NULL)) == INVALID_THREAD_HANDLE)
	{
		this->error      = GetLastError();
		this->is_running = false;

		return false;
	}
#endif

	return true;
}
bool FIO::Thread::Terminate()
{
	if (IsRunning())
	{
#if defined(FIO_LINUX)
		if (error = pthread_cancel(GetHandle()))
			return false;
#elif defined(FIO_WIN32)
		if (!TerminateThread(GetHandle(), 0))
		{
			error = GetLastError();

			return false;
		}
#endif

		error      = 0;
		is_running = false;
	}

	return true;
}

#if defined(FIO_LINUX)
void*        FIO::Thread::Detour(void* param)
{
	auto thread = (Thread*)param;

	thread->main(*thread);
	thread->Close();

	return nullptr;
}
#elif defined(FIO_WIN32)
DWORD WINAPI FIO::Thread::Detour(LPVOID param)
{
	auto thread = (Thread*)param;

	thread->main(*thread);
	thread->Close();

	return 0;
}
#endif
