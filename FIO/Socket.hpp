#pragma once
#if defined(FIO_LINUX)
	#include <sys/socket.h>

	#define SD_BOTH    SHUT_RDWR
	#define SD_SEND    SHUT_WR
	#define SD_RECEIVE SHUT_RD
#elif defined(FIO_WIN32)
	#include <WinSock2.h>
	#include <Windows.h>

	typedef int socklen_t;
#endif

#include <list>
#include <functional>

#include "IP.hpp"
#include "ThreadPool.hpp"

namespace FIO
{
	enum SOCKET_TYPE
	{
		SOCKET_TYPE_RAW     = (SOCK_RAW    << 8) | IPPROTO_RAW,
		SOCKET_TYPE_TCP     = (SOCK_STREAM << 8) | IPPROTO_TCP,
		SOCKET_TYPE_UDP     = (SOCK_DGRAM  << 8) | IPPROTO_UDP,
		SOCKET_TYPE_ICMP    = (SOCK_RAW    << 8) | IPPROTO_ICMP,
		SOCKET_TYPE_ICMP_V6 = (SOCK_RAW    << 8) | IPPROTO_ICMPV6
	};

	enum SOCKET_SHUTDOWN
	{
		SOCKET_SHUTDOWN_SEND = SD_SEND,
		SOCKET_SHUTDOWN_RECV = SD_RECEIVE,
		SOCKET_SHUTDOWN_BOTH = SD_BOTH
	};

	class Socket;

	typedef std::function<void(Socket& socket, Socket& client)>                                                                                   SocketAcceptCallback;
	typedef std::function<void(Socket& socket)>                                                                                                   SocketConnectCallback;

	typedef std::function<void(Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent)>                                     SocketSendCallback;
	typedef std::function<void(Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent, const IPEndPoint& remote_end_point)> SocketSendToCallback;

	typedef std::function<void(Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received)>                                       SocketReceiveCallback;
	typedef std::function<void(Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received, const IPEndPoint& remote_end_point)>   SocketReceiveFromCallback;

	class Socket
	{
		struct IOContext_Accept
		{
			ThreadPoolIOContext  IO;
			uint8_t              Buffer[(sizeof(sockaddr_storage) + 16) * 2];
			Socket*              Client;
			bool                 ClientIsOpen;
			SocketAcceptCallback Callback;
#if defined(FIO_WIN32)
			DWORD                NumBytesReceived;
#endif
		};
		struct IOContext_Connect
		{
			ThreadPoolIOContext   IO;
			SocketConnectCallback Callback;
#if defined(FIO_WIN32)
			DWORD                 NumBytesSent;
#endif
		};
		struct IOContext_Send
		{
			ThreadPoolIOContext IO;
#if defined(FIO_WIN32)
			WSABUF              Buffer;
#endif
			SocketSendCallback  Callback;
#if defined(FIO_WIN32)
			DWORD               NumBytesSent;
#endif
		};
		struct IOContext_SendTo
		{
			ThreadPoolIOContext  IO;
#if defined(FIO_WIN32)
			WSABUF               Buffer;
#endif
			SocketSendToCallback Callback;
#if defined(FIO_WIN32)
			DWORD                NumBytesSent;
#endif
			IPEndPoint           RemoteEndPoint;
		};
		struct IOContext_Receive
		{
			ThreadPoolIOContext   IO;
#if defined(FIO_WIN32)
			DWORD                 Flags;
			WSABUF                Buffer;
#endif
			SocketReceiveCallback Callback;
#if defined(FIO_WIN32)
			DWORD                 NumBytesReceived;
#endif
		};
		struct IOContext_ReceiveFrom
		{
			struct EndPoint
			{
				socklen_t        Size;
				sockaddr_storage Address;
			};

			ThreadPoolIOContext       IO;
#if defined(FIO_WIN32)
			DWORD                     Flags;
			WSABUF                    Buffer;
#endif
			SocketReceiveFromCallback Callback;
			EndPoint                  RemoteEndPoint;
#if defined(FIO_WIN32)
			DWORD                     NumBytesReceived;
#endif
		};

		bool                is_open;
		bool                is_bound;
		bool                is_closing;
		bool                is_blocking;
		bool                is_connected;
		bool                is_listening;
		bool                is_associated;

		const int           type;
#if defined(FIO_LINUX)
		std::atomic<int>    error;
		int                 handle;
#elif defined(FIO_WIN32)
		std::atomic<DWORD>  error;
		SOCKET              handle;
#endif
		const int           address_family;
		IPEndPoint          ip_end_point_local;
		IPEndPoint          ip_end_point_remote;

		ThreadPool*         thread_pool;
		ThreadPoolIOManager thread_pool_io;

		Socket(Socket&&) = delete;
		Socket(const Socket&) = delete;

	public:
		Socket(int type, int address_family);

		virtual ~Socket();

		constexpr bool  IsOpen() const
		{
			return is_open;
		}

		constexpr bool  IsBound() const
		{
			return is_bound;
		}

		constexpr bool  IsBlocking() const
		{
			return is_blocking;
		}

		constexpr bool  IsConnected() const
		{
			return is_connected;
		}

		constexpr bool  IsListening() const
		{
			return is_listening;
		}

		constexpr bool  IsAssociated() const
		{
			return is_associated;
		}

		constexpr auto  GetType() const
		{
			return type;
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

		constexpr auto  GetAddressFamily() const
		{
			return address_family;
		}

		constexpr auto& GetLocalEndPoint() const
		{
			return ip_end_point_local;
		}

		constexpr auto& GetRemoteEndPoint() const
		{
			return ip_end_point_remote;
		}

		bool SetBlocking(bool value = true);

		bool Open();
		void Close(bool wait_for_io = false);

		bool Bind(const IPEndPoint& local_ip_end_point);

		// @return 0 on error
		// @return -1 on would block
		int  Accept(Socket& socket);
		// @return 0 on error
		// @return -1 on would block
		int  Accept(Socket& socket, SocketAcceptCallback&& callback);

		bool Listen();
		bool Listen(uint32_t backlog);

		bool Connect(const IPEndPoint& remote_ip_end_point);
		// @return 0 on error
		// @return -1 on would block
		int  Connect(const IPEndPoint& remote_ip_end_point, SocketConnectCallback&& callback);

		bool Shutdown(int type);

		bool Associate(ThreadPool& pool);

		bool Send(const void* buffer, size_t size, size_t& number_of_bytes_sent);
		// @return 0 on error
		// @return -1 on would block
		int  Send(const void* buffer, size_t size, SocketSendCallback&& callback);

		bool SendTo(const void* buffer, size_t size, const IPEndPoint& remote_ip_end_point, size_t& number_of_bytes_sent);
		// @return 0 on error
		// @return -1 on would block
		int  SendTo(const void* buffer, size_t size, const IPEndPoint& remote_ip_end_point, SocketSendToCallback&& callback);

		bool Receive(void* buffer, size_t size, size_t& number_of_bytes_received);
		// @return 0 on error
		// @return -1 on would block
		int  Receive(void* buffer, size_t size, SocketReceiveCallback&& callback);

		bool ReceiveFrom(void* buffer, size_t size, IPEndPoint& remote_ip_end_point, size_t& number_of_bytes_received);
		// @return 0 on error
		// @return -1 on would block
		int  ReceiveFrom(void* buffer, size_t size, SocketReceiveFromCallback&& callback);

	private:
		void OnAccept(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
		void OnConnect(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
		void OnSend(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
		void OnSendTo(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
		void OnReceive(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
		void OnReceiveFrom(ThreadPoolIOContext& io, size_t number_of_bytes_transferred);
	};
}
