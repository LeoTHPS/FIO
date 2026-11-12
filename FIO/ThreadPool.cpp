#include "Timer.hpp"
#include "Thread.hpp"
#include "ThreadPool.hpp"

#if defined(FIO_LINUX)

#elif defined(FIO_WIN32)
	#define INVALID_THREAD_POOL_HANDLE INVALID_HANDLE_VALUE
#endif

FIO::ThreadPool::IOManager::IOManager()
{
	list_empty.Lock();
}
FIO::ThreadPool::IOManager::~IOManager()
{
	Wait();
}
void FIO::ThreadPool::IOManager::Add(Context& value)
{
	SpinLockGuard lock(list_lock);

	list.push_front(&value);
	list_empty.Unlock();
}
void FIO::ThreadPool::IOManager::Remove(Context& value)
{
	SpinLockGuard lock(list_lock);

	for (auto it = list.begin(); it != list.end(); ++it)
		if (*it == &value)
		{
			list.erase(it);

			if (list.empty())
			{
				list_empty.Lock();
				list_empty.NotifyAll();
			}

			break;
		}
}
void FIO::ThreadPool::IOManager::Wait() const
{
	list_empty.Wait(false);
}

FIO::ThreadPool::ThreadPool(size_t size, size_t io_queue_size)
	: is_running(false),
	is_stopping(false),
	error(0),
#if defined(FIO_LINUX)
	threads_io_queue_capacity(io_queue_size),
#elif defined(FIO_WIN32)
	handle(INVALID_THREAD_POOL_HANDLE),
#endif
	threads_active_count(0)
{
	Resize(size);
}
FIO::ThreadPool::~ThreadPool()
{
	if (IsRunning())
	{
		Shutdown();
		Join();
	}

	for (auto context : threads)
		delete context;
}
bool FIO::ThreadPool::Join()
{
	if (IsRunning())
		for (auto context : threads)
			if (!context->Thread.Join())
				return false;

	return true;
}
int  FIO::ThreadPool::Join(TimeSpan timeout)
{
	if (IsRunning())
	{
		Timer    timer;
		TimeSpan elapsed;

		for (auto context : threads)
		{
			if ((elapsed = timer.GetElapsed()) >= timeout)
				return -1;

			switch (context->Thread.Join(timeout - elapsed))
			{
				case 0:  return 0;
				case -1: return -1;
			}
		}
	}

	return 1;
}
bool FIO::ThreadPool::Post(Function&& function)
{
	if (!IsRunning() || IsStopping())
		return false;

#if defined(FIO_LINUX)
	threads_io_queue.Push({
		.Type     = IOFUNCTION_TYPE_FUNCTION,
		.Function = std::move(function)
	});
#elif defined(FIO_WIN32)
	auto context = new IOContext
	{
		.Callback = [func = std::move(function)](ThreadPool& pool, IOContext& context, size_t number_of_bytes_transferred)
		{
			func(pool);

			return true;
		}
	};

	if (!PostQueuedCompletionStatus(GetHandle(), 0, 0, (LPOVERLAPPED)context))
	{
		error = ::GetLastError();

		delete context;

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
	threads_io_queue.Push({
		.Type     = IOFUNCTION_TYPE_FUNCTION,
		.Function = function
	});
#elif defined(FIO_WIN32)
	auto context = new IOContext
	{
		.Callback = [&function](ThreadPool& pool, IOContext& context, size_t number_of_bytes_transferred)
		{
			function(pool);

			return true;
		}
	};

	if (!PostQueuedCompletionStatus(GetHandle(), 0, 0, (LPOVERLAPPED)context))
	{
		error = ::GetLastError();

		delete context;

		return false;
	}
#endif

	return true;
}
bool FIO::ThreadPool::Resize(size_t value)
{
	if (IsRunning())
		return false;

	for (auto context : threads)
		if (context) delete context;

	threads.resize(value);

	for (auto& context : threads)
		context = new ThreadContext
		{
#if defined(FIO_LINUX)
			.Queue = 0
#endif
		};

	return true;
}
bool FIO::ThreadPool::Start()
{
	if (IsRunning())
		return false;

#if defined(FIO_LINUX)
	if (!threads_io.Start(std::bind(&ThreadPool::Thread_MainIO, this, std::placeholders::_1)))
	{
		error = threads_io.GetLastError();

		return false;
	}
#elif defined(FIO_WIN32)
	if (!(handle = CreateIoCompletionPort(INVALID_THREAD_POOL_HANDLE, nullptr, 0, (DWORD)GetSize())))
	{
		error = ::GetLastError();

		return false;
	}
#endif

	is_running  = true;
	is_stopping = false;

	for (auto it = threads.begin(); it != threads.end(); ++it)
	{
#if defined(FIO_LINUX)
		if (auto error = io_uring_queue_init(threads_io_queue_capacity, &(*it)->Ring, 0))
		// if (auto error = io_uring_queue_init(threads_io_queue_capacity, &(*it)->Ring, IORING_SETUP_SINGLE_ISSUER))
		{
			this->error = -error;

			Shutdown();
			Join();

			return false;
		}
#endif

		if (!(*it)->Thread.Start(std::bind(&ThreadPool::Thread_Main, this, std::placeholders::_1, std::ref(**it))))
		{
			error = (*it)->Thread.GetLastError();

			Shutdown();
			Join();

			return false;
		}
	}

	return true;
}
void FIO::ThreadPool::Shutdown()
{
	if (IsRunning() && !IsStopping())
	{
		is_stopping = true;

		for (auto context : threads)
#if defined(FIO_LINUX)
			threads_io_queue.Push({
				.Type = IOFUNCTION_TYPE_SHUTDOWN
			});
#elif defined(FIO_WIN32)
			if (!PostQueuedCompletionStatus(GetHandle(), 0, 0, nullptr))
				error = ::GetLastError();
#endif
	}
}
#if defined(FIO_WIN32)
bool FIO::ThreadPool::Associate(HANDLE handle)
{
	if (!IsRunning() || IsStopping())
		return false;

	if (!CreateIoCompletionPort(handle, GetHandle(), 0, 0))
	{
		error = ::GetLastError();

		return false;
	}

	return true;
}
#endif
void FIO::ThreadPool::Thread_Main(Thread& thread, ThreadContext& context)
{
	++threads_active_count;

#if defined(FIO_LINUX)
	io_uring_cqe* cqe;
	int           cqe_error;
	int           cqe_result;
	IOContext*    cqe_context;

	do
	{
	wait_cqe:
		if ((cqe_error = io_uring_wait_cqe(&context.Ring, &cqe)) < 0)
		{
			if (cqe_error == -EINTR)
				goto wait_cqe;

			error = -cqe_error;

			break;
		}

		cqe_result  = cqe->res;
		cqe_context = (IOContext*)io_uring_cqe_get_data(cqe);

		cqe_context->CQE = cqe;

		io_uring_cqe_seen(&context.Ring, cqe);
	} while (Thread_HandleIO(context.Thread, cqe_context, cqe_result));
#elif defined(FIO_WIN32)
	OVERLAPPED_ENTRY entry;
	ULONG            entry_count;

	do
	{
		if (!GetQueuedCompletionStatusEx(GetHandle(), &entry, 1, &entry_count, INFINITE, TRUE))
		{
			auto error = ::GetLastError();

			switch (error)
			{
				case WAIT_TIMEOUT:
				case WAIT_IO_COMPLETION:
					continue;
			}

			this->error = error;

			break;
		}
	} while (Thread_HandleIO(context.Thread, (IOContext*)entry.lpOverlapped, entry.dwNumberOfBytesTransferred));
#endif

#if defined(FIO_LINUX)
	io_uring_queue_exit(&context.Ring);
#endif

	if (threads_active_count.fetch_sub(1) == 1)
	{
#if defined(FIO_WIN32)
		CloseHandle(GetHandle());
		handle = INVALID_THREAD_POOL_HANDLE;
#endif

		is_running  = false;
		is_stopping = false;
	}
}
#if defined(FIO_LINUX)
void FIO::ThreadPool::Thread_MainIO(Thread& thread)
{
	io_uring_sqe* sqe;
	int           error;
	IOCallback    callback;
	IOFunction    function;

	while ((function = threads_io_queue.Pop()).Type != IOFUNCTION_TYPE_SHUTDOWN)
	{
		ThreadContext* selected_thread = nullptr;

		for (auto ctx : this->threads)
			if (!selected_thread || (ctx->Queue < selected_thread->Queue))
				selected_thread = ctx;

		selected_thread->Queue.fetch_add(1);

		if (!(sqe = io_uring_get_sqe(&selected_thread->Ring)))
			; // TODO: error

		switch (function.Type)
		{
			case IOFUNCTION_TYPE_IO:
				// TODO: implement
				break;

			case IOFUNCTION_TYPE_FUNCTION:
				io_uring_prep_nop(sqe);
				callback = [this, function = std::move(function.Function)](ThreadPool& pool, IOContext& context, size_t number_of_bytes_transferred)
				{
					function(*this);
				};
				break;
		}

		auto context = new IOContext
		{
			.Ring     = &selected_thread->Ring,
			.Callback = std::move(callback)
		};

		io_uring_sqe_set_data(sqe, context);

		if ((error = io_uring_submit(&selected_thread->Ring)) < 0)
		{
			this->error = -error;

			delete context;

			selected_thread->Queue.fetch_sub(1);

			// TODO: error
		}
	}
}
#endif
bool FIO::ThreadPool::Thread_HandleIO(Thread& thread, IOContext* context, size_t number_of_bytes_transferred)
{
	if (context == nullptr)
		return false;

	context->Callback(*this, *context, number_of_bytes_transferred);

	return true;
}
