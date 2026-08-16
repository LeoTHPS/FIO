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
	enum FILE_MODE
	{
		FILE_MODE_READ     = 0x1,
		FILE_MODE_WRITE    = 0x2,
		FILE_MODE_APPEND   = 0x4,
		FILE_MODE_CREATE   = 0x8, // If the file exists then open it. Otherwise create it
		FILE_MODE_TRUNCATE = 0x10 // If the file exists then truncate it. Otherwise create it
	};

	class File;

	typedef std::function<void(File& file, void* buffer, size_t size, size_t number_of_bytes_read)>          FileReadCallback;
	typedef std::function<void(File& file, const void* buffer, size_t size, size_t number_of_bytes_written)> FileWriteCallback;

	class File
	{
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

			ThreadPoolIOContext IO;
			BufferContext       Buffer;
			FileReadCallback    Callback;
		};
		struct IOContext_Write
		{
			struct BufferContext
			{
				size_t      Size;
				const void* Buffer;
			};

			ThreadPoolIOContext IO;
			BufferContext       Buffer;
			FileWriteCallback   Callback;
		};

		bool                  is_open;
		bool                  is_closing;
		bool                  is_associated;

		const int             mode;
		std::string           path;
		std::atomic<uint64_t> size;
#if defined(FIO_LINUX)
		std::atomic<int>      error;
		int                   handle;
#elif defined(FIO_WIN32)
		std::atomic<DWORD>    error;
		HANDLE                handle;
#endif
		std::atomic<uint64_t> position[POSITION_TYPE_COUNT];
		std::atomic<int>      position_type;

		ThreadPool*           thread_pool;
		ThreadPoolIOManager   thread_pool_io;

		File(File&&) = delete;
		File(const File&) = delete;

	public:
		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Copy(std::string_view source, std::string_view destination);

		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Move(std::string_view source, std::string_view destination);

		// @return 0 on error
		// @return -1 on not found
		// @return -2 on already exists
		static int  Create(std::string_view path);

		// @return 0 on error
		// @return -1 on not found
		static int  Delete(std::string_view path);

		static bool Exists(std::string_view path);

		File(std::string_view path, int mode);

		virtual ~File();

		constexpr bool  IsOpen() const
		{
			return is_open;
		}

		constexpr bool  IsReadOnly() const
		{
			return (mode & FILE_MODE_READ) && !(mode & FILE_MODE_WRITE);
		}

		constexpr bool  IsWriteOnly() const
		{
			return (mode & FILE_MODE_WRITE) && !(mode & FILE_MODE_READ);
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
		bool Read(void* buffer, size_t size, FileReadCallback&& callback);

		bool Write(const void* buffer, size_t size, size_t& number_of_bytes_written);
		bool Write(const void* buffer, size_t size, FileWriteCallback&& callback);

	private:
		uint64_t Position_Get(int type) const;
		void     Position_Set(int type, uint64_t value);
		void     Position_Reset();
		bool     Position_Select(int type);
		bool     Position_SelectAsync(int type);
		void     Position_Increment(int type, size_t value);
		void     Position_IncrementAsync(int type, size_t value);

	private:
		void OnRead(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
		void OnWrite(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
	};
}
