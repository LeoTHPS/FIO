#pragma once
#include <list>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>

#if defined(FIO_LINUX)
	#include <liburing.h>

	#include "MPSCQueue.hpp"
#elif defined(FIO_WIN32)
	#include <Windows.h>
#endif

#include "Thread.hpp"
#include "SpinLock.hpp"
#include "TimeSpan.hpp"

namespace FIO
{
	class ThreadPool
	{
	public:
		struct IOContext;

		typedef std::function<void(ThreadPool& pool)>                                                         Function;
		typedef std::function<void(ThreadPool& pool, IOContext& context, size_t number_of_bytes_transferred)> IOCallback;

		struct IOContext
		{
#if defined(FIO_LINUX)
			io_uring_cqe* CQE;
			io_uring*     Ring;
#elif defined(FIO_WIN32)
			OVERLAPPED    O;
#endif
			IOCallback    Callback;
		};

		class  IOManager
		{
			std::list<IOContext*> list;
			SpinLock              list_lock;
			SpinLock              list_empty;

			IOManager(IOManager&&) = delete;
			IOManager(const IOManager&) = delete;

		public:
			typedef IOContext  Context;
			typedef IOCallback Callback;

			IOManager();

			virtual ~IOManager();

			void Add(Context& value);
			void Remove(Context& value);

			void Wait() const;
		};

	private:
#if defined(FIO_LINUX)
		enum IOFUNCTION_TYPE
		{
			IOFUNCTION_TYPE_IO,
			IOFUNCTION_TYPE_FUNCTION,
			IOFUNCTION_TYPE_SHUTDOWN
		};

		struct IOFunction
		{
			int                  Type;
			ThreadPool::Function Function;
		};
#endif

		struct ThreadContext
		{
#if defined(FIO_LINUX)
			SpinLock            Lock;
			io_uring            Ring;
			std::atomic<size_t> Queue;
#endif
			FIO::Thread         Thread;
		};

		std::atomic<bool>           is_running;
		std::atomic<bool>           is_stopping;

#if defined(FIO_LINUX)
		int                         error;
#elif defined(FIO_WIN32)
		DWORD                       error;
		HANDLE                      handle;
#endif

		std::vector<ThreadContext*> threads;
#if defined(FIO_LINUX)
		Thread                      threads_io;
		MPSCQueue<IOFunction>       threads_io_queue;
		const size_t                threads_io_queue_capacity; // applies to each queue per thread
#endif
		std::atomic<size_t>         threads_active_count; // excludes threads_io

		ThreadPool(ThreadPool&&) = delete;
		ThreadPool(const ThreadPool&) = delete;

	public:
		// @param io_queue_size has no effect on windows
		ThreadPool(size_t size, size_t io_queue_size);

		virtual ~ThreadPool();

		inline    bool IsRunning() const
		{
			return is_running.load(std::memory_order_relaxed);
		}

		inline    bool IsStopping() const
		{
			return is_stopping.load(std::memory_order_relaxed);
		}

		constexpr auto GetSize() const
		{
			return threads.size();
		}

		constexpr auto GetHandle() const
		{
#if defined(FIO_LINUX)

#elif defined(FIO_WIN32)
			return handle;
#endif
		}

		constexpr auto GetLastError() const
		{
			return error;
		}

		bool Join();
		// @return 0 on error
		// @return -1 on timeout
		int  Join(TimeSpan timeout);

		bool Post(Function&& function);
		bool Post(const Function& function);

		bool Resize(size_t value);

		bool Start();
		void Shutdown();

#if defined(FIO_WIN32)
		bool Associate(HANDLE handle);
#endif

	private:
		void Thread_Main(Thread& thread, ThreadContext& context);
#if defined(FIO_LINUX)
		void Thread_MainIO(Thread& thread);
#endif
		bool Thread_HandleIO(Thread& thread, IOContext* context, size_t number_of_bytes_transferred);
	};
}
