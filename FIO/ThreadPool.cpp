#include "Thread.hpp"
#include "ThreadPool.hpp"

#if defined(FIO_LINUX)
	#define INVALID_THREAD_POOL_HANDLE -1
#elif defined(FIO_WIN32)
	#define INVALID_THREAD_POOL_HANDLE INVALID_HANDLE_VALUE
#endif

FIO::ThreadPoolIOManager::ThreadPoolIOManager()
	: list_busy(false),
	list_empty(true)
{
}
FIO::ThreadPoolIOManager::~ThreadPoolIOManager()
{
	Wait();
}
void FIO::ThreadPoolIOManager::Add(Context& value)
{
	Lock();

	list.push_front(&value);

	Unlock();
}
void FIO::ThreadPoolIOManager::Remove(Context& value)
{
	Lock();

	for (auto it = list.begin(); it != list.end(); ++it)
		if (*it == &value)
		{
			list.erase(it);

			if (list.empty())
			{
				list_empty.test_and_set();
				list_empty.notify_all();
			}

			break;
		}

	Unlock();
}
void FIO::ThreadPoolIOManager::Wait() const
{
	list_empty.wait(false);
}

FIO::ThreadPool::ThreadPool(size_t size)
	: is_running(false),
	is_stopping(false),
#if defined(FIO_LINUX)

#elif defined(FIO_WIN32)
	handle(INVALID_THREAD_POOL_HANDLE),
#endif
	threads(size, nullptr)
{
	for (auto& thread : threads)
		thread = new Thread();
}
FIO::ThreadPool::~ThreadPool()
{
	if (IsRunning())
	{
		Shutdown();
		Join();
	}

	for (auto thread : threads)
		delete thread;
}
bool FIO::ThreadPool::Join()
{
	if (IsRunning())
		for (auto thread : threads)
			if (!thread->Join())
				return false;

	return true;
}
bool FIO::ThreadPool::Post(Function&& function)
{
	if (!IsRunning() || IsStopping())
		return false;

	auto func = new Function(std::move(function));

#if defined(FIO_LINUX)
	// TODO: implement linux
	return false;
#elif defined(FIO_WIN32)
	if (!PostQueuedCompletionStatus(GetHandle(), 0, IOCP_REQUEST_KEY_FUNC, (LPOVERLAPPED)func))
	{
		function = std::move(*func);

		delete func;

		return false;
	}
#endif

	return true;
}
bool FIO::ThreadPool::Post(const Function& function)
{
	if (!IsRunning() || IsStopping())
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
	return false;
#elif defined(FIO_WIN32)
	if (!PostQueuedCompletionStatus(GetHandle(), 0, IOCP_REQUEST_KEY_FUNC_PTR, (LPOVERLAPPED)&function))
		return false;
#endif

	return true;
}
bool FIO::ThreadPool::Resize(size_t value)
{
	if (IsRunning())
		return false;

	for (auto thread : threads)
		delete thread;

	threads.resize(value);

	for (auto& thread : threads)
		thread = new Thread();

	return true;
}
bool FIO::ThreadPool::Start()
{
	if (IsRunning())
		return false;

#if defined(FIO_WIN32)
	if (!(handle = CreateIoCompletionPort(INVALID_THREAD_POOL_HANDLE, nullptr, 0, (DWORD)GetSize())))
		return false;
#endif

	is_running    = true;
	is_stopping   = false;
	threads_count = GetSize();

	for (auto it = threads.begin(); it != threads.end(); ++it)
		if (!(*it)->Start(std::bind(&ThreadPool::Thread_Main, this)))
		{
			Shutdown();

			return false;
		}

	return true;
}
void FIO::ThreadPool::Shutdown()
{
	if (IsRunning() && !IsStopping())
	{
		is_stopping = true;

#if defined(FIO_LINUX)
		// TODO: implement linux
#elif defined(FIO_WIN32)
		for (auto thread : threads)
			PostQueuedCompletionStatus(GetHandle(), 0, IOCP_REQUEST_KEY_SHUTDOWN, nullptr);
#endif
	}
}
#if defined(FIO_WIN32)
bool FIO::ThreadPool::Associate(HANDLE handle)
{
	if (!IsRunning() || IsStopping())
		return false;

	if (!CreateIoCompletionPort(handle, GetHandle(), IOCP_REQUEST_KEY_IO, 0))
		return false;

	return true;
}
#endif
void FIO::ThreadPool::Thread_Main()
{
#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	OVERLAPPED_ENTRY entry;
	ULONG            entry_count;

	do
	{
		if (!GetQueuedCompletionStatusEx(GetHandle(), &entry, 1, &entry_count, INFINITE, TRUE))
		{
			switch (GetLastError())
			{
				case WAIT_TIMEOUT:
				case WAIT_IO_COMPLETION:
					continue;
			}

			break;
		}
	} while (Thread_HandleIOCP(entry));
#endif

	if (threads_count.fetch_sub(1) == 1)
	{
#if defined(FIO_LINUX)
		// TODO: implement linux
#elif defined(FIO_WIN32)
		CloseHandle(GetHandle());
		handle = INVALID_THREAD_POOL_HANDLE;
#endif

		is_running  = false;
		is_stopping = false;
	}
}
#if defined(FIO_WIN32)
bool FIO::ThreadPool::Thread_HandleIOCP(const OVERLAPPED_ENTRY& entry)
{
	switch (entry.lpCompletionKey)
	{
		case IOCP_REQUEST_KEY_IO:
			if (auto io = (IOContext*)entry.lpOverlapped)
				io->Callback(*io, entry.dwNumberOfBytesTransferred);
			break;

		case IOCP_REQUEST_KEY_FUNC:
			if (auto function = (ThreadPoolFunction*)entry.lpOverlapped)
			{
				(*function)();

				delete function;
			}
			break;

		case IOCP_REQUEST_KEY_FUNC_PTR:
			if (auto function = (const ThreadPoolFunction*)entry.lpOverlapped)
				(*function)();
			break;

		case IOCP_REQUEST_KEY_SHUTDOWN:
			return false;
	}

	return true;
}
#endif
