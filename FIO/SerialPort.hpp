#pragma once
#include <string>
#include <cstdint>
#include <functional>

#if defined(FIO_LINUX)

#elif defined(FIO_WIN32)
	#include <Windows.h>
#endif

#include "ThreadPool.hpp"

namespace FIO
{
	class SerialPort
	{
	public:
		enum FLAG
		{
			FLAG_PARITY_ODD            = 0x1,
			FLAG_PARITY_EVEN           = 0x2,
			FLAG_PARITY_MARK           = 0x4,
			FLAG_PARITY_SPACE          = 0x8,
			FLAG_PARITY_DISABLED       = 0x10,

			FLAG_TWO_STOP_BITS         = 0x100,

			FLAG_CTS_CONTROL_ENABLE    = 0x1000,
			FLAG_CTS_CONTROL_DISABLE   = 0x2000,

			FLAG_DTR_CONTROL_ENABLE    = 0x10000,
			FLAG_DTR_CONTROL_DISABLE   = 0x20000,
			FLAG_DTR_CONTROL_HANDSHAKE = 0x40000,

			FLAG_RTS_CONTROL_TOGGLE    = 0x100000,
			FLAG_RTS_CONTROL_ENABLE    = 0x200000,
			FLAG_RTS_CONTROL_DISABLE   = 0x400000,
			FLAG_RTS_CONTROL_HANDSHAKE = 0x800000
		};

		typedef std::function<void(SerialPort& port, void* buffer, size_t size, size_t number_of_bytes_read)>          ReadCallback;
		typedef std::function<void(SerialPort& port, const void* buffer, size_t size, size_t number_of_bytes_written)> WriteCallback;

	private:
		struct IOContext_Read
		{
			struct BufferContext
			{
				size_t Size;
				void*  Buffer;
			};

			ThreadPool::IOContext IO;
			BufferContext         Buffer;
			ReadCallback          Callback;
		};
		struct IOContext_Write
		{
			struct BufferContext
			{
				size_t      Size;
				const void* Buffer;
			};

			ThreadPool::IOContext IO;
			BufferContext         Buffer;
			WriteCallback         Callback;
		};

		bool                  is_open;
		bool                  is_closing;
		bool                  is_associated;

		const uint32_t        baud;
		const int             flags;
#if defined(FIO_LINUX)
		std::string           path;
		std::atomic<int>      error;
		int                   handle;
#elif defined(FIO_WIN32)
		std::wstring          path;
		std::atomic<DWORD>    error;
		HANDLE                handle;
#endif

		ThreadPool*           thread_pool;
		ThreadPool::IOManager thread_pool_io;

		SerialPort(SerialPort&&) = delete;
		SerialPort(const SerialPort&) = delete;

	public:
		SerialPort(std::string_view path, uint32_t baud, int flags);
#if defined(FIO_WIN32)
		SerialPort(std::wstring_view path, uint32_t baud, int flags);
#endif

		virtual ~SerialPort();

		constexpr bool  IsOpen() const
		{
			return is_open;
		}

		constexpr bool  IsAssociated() const
		{
			return is_associated;
		}

		constexpr auto  GetBaud() const
		{
			return baud;
		}

		constexpr auto& GetPath() const
		{
			return path;
		}

		constexpr auto  GetFlags() const
		{
			return flags;
		}

		constexpr auto  GetHandle() const
		{
			return handle;
		}

		inline    auto  GetLastError() const
		{
			return error.load();
		}

		constexpr auto  GetThreadPool() const
		{
			return thread_pool;
		}

		bool Open();
		void Close();

		bool Associate(ThreadPool& pool);

		bool Read(void* buffer, size_t size, size_t& number_of_bytes_read);
		bool Read(void* buffer, size_t size, ReadCallback&& callback);

		bool Write(const void* buffer, size_t size, size_t& number_of_bytes_written);
		bool Write(const void* buffer, size_t size, WriteCallback&& callback);

	private:
		void OnRead(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnWrite(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
	};
}
