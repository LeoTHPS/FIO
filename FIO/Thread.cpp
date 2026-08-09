#include "Thread.hpp"

#if defined(FIO_LINUX)
	#define INVALID_THREAD_HANDLE 0

	#include <cerrno>
#elif defined(FIO_WIN32)
	#define INVALID_THREAD_HANDLE NULL
#endif

FIO::Thread::Thread()
	: is_running(false),
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

bool FIO::Thread::Start(ThreadMain&& main)
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

	thread->main();

	thread->error      = 0;
	thread->handle     = INVALID_THREAD_HANDLE;
	thread->is_running = false;

	return nullptr;
}
#elif defined(FIO_WIN32)
DWORD WINAPI FIO::Thread::Detour(LPVOID param)
{
	auto thread = (Thread*)param;

	thread->main();

	CloseHandle(thread->GetHandle());

	thread->error      = 0;
	thread->handle     = INVALID_THREAD_HANDLE;
	thread->is_running = false;

	return 0;
}
#endif
