#pragma once
#if defined(FIO_LINUX)
	#include <sys/socket.h>

	#define SD_BOTH    SHUT_RDWR
	#define SD_SEND    SHUT_WR
	#define SD_RECEIVE SHUT_RD
#elif defined(FIO_WIN32)
	#include "WinSock2.hpp"

	#include <Windows.h>
#endif

#include <list>
#include <functional>

#include "IPAddress.hpp"
#include "ThreadPool.hpp"

namespace FIO
{
	class Socket
	{
	public:
		enum TYPE
		{
			TYPE_RAW       = SOCK_RAW,
			TYPE_RDM       = SOCK_RDM,
			TYPE_DGRAM     = SOCK_DGRAM,
			TYPE_STREAM    = SOCK_STREAM,
			TYPE_SEQPACKET = SOCK_SEQPACKET
		};

		enum PROTOCOL
		{
			PROTOCOL_TCP      = IPPROTO_TCP,
			PROTOCOL_UDP      = IPPROTO_UDP,

			PROTOCOL_IPV4     = IPPROTO_IP,
			PROTOCOL_IPV6     = IPPROTO_IPV6,

			PROTOCOL_ICMPV4   = IPPROTO_ICMP,
			PROTOCOL_ICMPV6   = IPPROTO_ICMPV6
		};

		enum SHUTDOWN
		{
			SHUTDOWN_SEND = SD_SEND,
			SHUTDOWN_RECV = SD_RECEIVE,
			SHUTDOWN_BOTH = SD_BOTH
		};

		typedef std::function<void(Socket& socket, Socket& client)>                                                                                   AcceptCallback;
		typedef std::function<void(Socket& socket)>                                                                                                   ConnectCallback;

		typedef std::function<void(Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent)>                                     SendCallback;
		typedef std::function<void(Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent, const IPEndPoint& remote_end_point)> SendToCallback;

		typedef std::function<void(Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received)>                                       ReceiveCallback;
		typedef std::function<void(Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received, const IPEndPoint& remote_end_point)>   ReceiveFromCallback;

	private:
		struct IOContext_Accept
		{
			ThreadPool::IOContext IO;
			uint8_t               Buffer[(sizeof(sockaddr_storage) + 16) * 2];
			Socket*               Client;
			bool                  ClientIsOpen;
			AcceptCallback        Callback;
#if defined(FIO_WIN32)
			DWORD                 NumBytesReceived;
#endif
		};
		struct IOContext_Connect
		{
			ThreadPool::IOContext IO;
			ConnectCallback       Callback;
#if defined(FIO_WIN32)
			DWORD                 NumBytesSent;
#endif
		};
		struct IOContext_Send
		{
			ThreadPool::IOContext IO;
#if defined(FIO_WIN32)
			WSABUF                Buffer;
#endif
			SendCallback          Callback;
#if defined(FIO_WIN32)
			DWORD                 NumBytesSent;
#endif
		};
		struct IOContext_SendTo
		{
			ThreadPool::IOContext IO;
#if defined(FIO_WIN32)
			WSABUF                Buffer;
#endif
			SendToCallback        Callback;
#if defined(FIO_WIN32)
			DWORD                 NumBytesSent;
#endif
			IPEndPoint            RemoteEndPoint;
		};
		struct IOContext_Receive
		{
			ThreadPool::IOContext IO;
#if defined(FIO_WIN32)
			DWORD                 Flags;
			WSABUF                Buffer;
#endif
			ReceiveCallback       Callback;
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

			ThreadPool::IOContext IO;
#if defined(FIO_WIN32)
			DWORD                 Flags;
			WSABUF                Buffer;
#endif
			ReceiveFromCallback   Callback;
			EndPoint              RemoteEndPoint;
#if defined(FIO_WIN32)
			DWORD                 NumBytesReceived;
#endif
		};

		bool                  is_open;
		bool                  is_bound;
		bool                  is_closing;
		bool                  is_blocking;
		bool                  is_connected;
		bool                  is_listening;
		bool                  is_associated;
		bool                  is_monitoring;

		const int             type;
		const int             protocol;

#if defined(FIO_LINUX)
		std::atomic<int>      error;
		int                   handle;
#elif defined(FIO_WIN32)
		WinSock2              ws2;
		std::atomic<DWORD>    error;
		SOCKET                handle;
#endif
		int                   address_family;
		IPEndPoint            ip_end_point_local;
		IPEndPoint            ip_end_point_remote;

		ThreadPool*           thread_pool;
		ThreadPool::IOManager thread_pool_io;

		Socket(Socket&&) = delete;
		Socket(const Socket&) = delete;

	public:
		Socket(int type, int protocol);

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

		constexpr bool  IsMonitoring() const
		{
			return is_monitoring;
		}

		constexpr auto  GetType() const
		{
			return type;
		}

		constexpr auto  GetHandle() const
		{
			return handle;
		}

		constexpr auto  GetProtocol() const
		{
			return protocol;
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

		bool Open(int address_family);
		void Close(bool wait_for_io = false);

		bool Bind(const IPEndPoint& local_ip_end_point);

		// @return 0 on error
		// @return -1 on would block
		int  Accept(Socket& socket);
		// @return 0 on error
		// @return -1 on would block
		int  Accept(Socket& socket, AcceptCallback&& callback);

		bool Listen();
		bool Listen(uint32_t backlog);

		bool Connect(const IPEndPoint& remote_ip_end_point);
		// @return 0 on error
		// @return -1 on would block
		int  Connect(const IPEndPoint& remote_ip_end_point, ConnectCallback&& callback);

		bool Monitor(bool set = true);

		bool Shutdown(int type);

		bool Associate(ThreadPool& pool);

		bool Send(const void* buffer, size_t size, size_t& number_of_bytes_sent);
		// @return 0 on error
		// @return -1 on would block
		int  Send(const void* buffer, size_t size, SendCallback&& callback);

		bool SendTo(const void* buffer, size_t size, const IPEndPoint& remote_ip_end_point, size_t& number_of_bytes_sent);
		// @return 0 on error
		// @return -1 on would block
		int  SendTo(const void* buffer, size_t size, const IPEndPoint& remote_ip_end_point, SendToCallback&& callback);

		bool Receive(void* buffer, size_t size, size_t& number_of_bytes_received);
		// @return 0 on error
		// @return -1 on would block
		int  Receive(void* buffer, size_t size, ReceiveCallback&& callback);

		bool ReceiveFrom(void* buffer, size_t size, IPEndPoint& remote_ip_end_point, size_t& number_of_bytes_received);
		// @return 0 on error
		// @return -1 on would block
		int  ReceiveFrom(void* buffer, size_t size, ReceiveFromCallback&& callback);

	private:
		void OnAccept(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnConnect(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnSend(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnSendTo(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnReceive(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
		void OnReceiveFrom(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred);
	};
}
