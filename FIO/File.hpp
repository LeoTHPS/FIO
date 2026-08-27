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
	class File
	{
	public:
		enum MODE
		{
			MODE_READ     = 0x1,
			MODE_WRITE    = 0x2,
			MODE_APPEND   = 0x4,
			MODE_CREATE   = 0x8, // If the file exists then open it. Otherwise create it
			MODE_TRUNCATE = 0x10 // If the file exists then truncate it. Otherwise create it
		};

		typedef std::function<void(File& file, void* buffer, size_t size, size_t number_of_bytes_read)>          ReadCallback;
		typedef std::function<void(File& file, const void* buffer, size_t size, size_t number_of_bytes_written)> WriteCallback;

	private:
		enum POSITION_TYPE
		{
			POSITION_TYPE_NONE = -1,

			POSITION_TYPE_READ,
			POSITION_TYPE_WRITE,

			POSITION_TYPE_COUNT
		};

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

		const int             mode;
		std::atomic<uint64_t> size;
#if defined(FIO_LINUX)
		std::string           path;
		std::atomic<int>      error;
		int                   handle;
#elif defined(FIO_WIN32)
		std::wstring          path;
		std::atomic<DWORD>    error;
		HANDLE                handle;
#endif
		std::atomic<uint64_t> position[POSITION_TYPE_COUNT];
		std::atomic<int>      position_type;

		ThreadPool*           thread_pool;
		ThreadPool::IOManager thread_pool_io;

		File(File&&) = delete;
		File(const File&) = delete;

	public:
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Copy(std::string_view source, std::string_view destination);
#if defined(FIO_WIN32)
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Copy(std::wstring_view source, std::wstring_view destination);
#endif

		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Move(std::string_view source, std::string_view destination);
#if defined(FIO_WIN32)
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Move(std::wstring_view source, std::wstring_view destination);
#endif

		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Create(std::string_view path);
#if defined(FIO_WIN32)
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Create(std::wstring_view path);
#endif

		// @return 0 on error
		// @return -1 on not found
		static int  Delete(std::string_view path);
#if defined(FIO_WIN32)
		// @return 0 on error
		// @return -1 on not found
		static int  Delete(std::wstring_view path);
#endif

		static bool Exists(std::string_view path);
#if defined(FIO_WIN32)
		static bool Exists(std::wstring_view path);
#endif

		File(std::string_view path, int mode);
#if defined(FIO_WIN32)
		File(std::wstring_view path, int mode);
#endif

		virtual ~File();

		constexpr bool  IsOpen() const
		{
			return is_open;
		}

		constexpr bool  IsReadOnly() const
		{
			return (mode & MODE_READ) && !(mode & MODE_WRITE);
		}

		constexpr bool  IsWriteOnly() const
		{
			return (mode & MODE_WRITE) && !(mode & MODE_READ);
		}

		constexpr bool  IsAssociated() const
		{
			return is_associated;
		}

		constexpr auto  GetMode() const
		{
			return mode;
		}

		constexpr auto& GetPath() const
		{
			return path;
		}

		inline    auto  GetSize() const
		{
			return size.load();
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

		inline    auto  GetReadPosition() const
		{
			return position[POSITION_TYPE_READ].load();
		}

		inline    auto  GetWritePosition() const
		{
			return position[POSITION_TYPE_WRITE].load();
		}

		void SetReadPosition(uint64_t value);

		void SetWritePosition(uint64_t value);

		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		int  Open();
		void Close(bool wait_for_io = false);

		bool Associate(ThreadPool& pool);

		bool Read(void* buffer, size_t size, size_t& number_of_bytes_read);
		bool Read(void* buffer, size_t size, ReadCallback&& callback);

		bool Write(const void* buffer, size_t size, size_t& number_of_bytes_written);
		bool Write(const void* buffer, size_t size, WriteCallback&& callback);

	private:
		uint64_t Position_Get(int type) const;
		void     Position_Set(int type, uint64_t value);
		void     Position_Reset();
		bool     Position_Select(int type);
		bool     Position_SelectAsync(int type);
		void     Position_Increment(int type, size_t value);
		void     Position_IncrementAsync(int type, size_t value);

	private:
		void OnRead(ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnWrite(ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
	};
}
