#pragma once
#include <list>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>

#if defined(FIO_LINUX)
	#include <linux/io_uring.h>
#elif defined(FIO_WIN32)
	#include <Windows.h>
#endif

namespace FIO
{
	class Thread;

	class ThreadPool
	{
	public:
		struct IOContext;

		typedef std::function<void()>                                                       Function;
		typedef std::function<void(IOContext& context, size_t number_of_bytes_transferred)> IOCallback;

		struct IOContext
		{
#if defined(FIO_WIN32)
			OVERLAPPED O;
#endif
			IOCallback Callback;
		};

		class  IOManager
		{
			std::list<IOContext*> list;
			std::atomic_flag      list_busy;
			std::atomic_flag      list_empty;

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

		private:
			inline void Lock()
			{
				while (list_busy.test_and_set(std::memory_order_acquire))
					while (list_busy.test(std::memory_order_relaxed))
						;
			}
			inline void Unlock()
			{
				list_busy.clear(std::memory_order_release);
			}
		};

	private:
#if defined(FIO_WIN32)
		enum IOCP_REQUEST_KEY : UINT_PTR
		{
			IOCP_REQUEST_KEY_IO,
			IOCP_REQUEST_KEY_FUNC,
			IOCP_REQUEST_KEY_FUNC_PTR,
			IOCP_REQUEST_KEY_SHUTDOWN
		};
#endif

		std::atomic<bool>    is_running;
		std::atomic<bool>    is_stopping;

#if defined(FIO_WIN32)
		HANDLE               handle;
#endif

		std::vector<Thread*> threads;
		std::atomic<size_t>  threads_count;

		ThreadPool(ThreadPool&&) = delete;
		ThreadPool(const ThreadPool&) = delete;

	public:
		explicit ThreadPool(size_t size);

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

		bool Join();

		bool Post(Function&& function);
		bool Post(const Function& function);

		bool Resize(size_t value);

		bool Start();
		void Shutdown();

#if defined(FIO_WIN32)
		bool Associate(HANDLE handle);
#endif

	private:
		void Thread_Main();
#if defined(FIO_WIN32)
		bool Thread_HandleIOCP(const OVERLAPPED_ENTRY& entry);
#endif
	};
}
