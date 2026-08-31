#include "Endian.hpp"
#include "Socket.hpp"
#include "Thread.hpp"
#include "ThreadPool.hpp"

#include <limits>

#if defined(FIO_LINUX)
	#define GetLastError()    errno
	#define WSAGetLastError() errno

	#define SOCKET_ERROR      -1
	#define INVALID_SOCKET    -1

	#define WSAEINTR          EINTR
	#define WSAENOBUFS        ENOBUFS
	#define WSAEALREADY       EALREADY
	#define WSAETIMEDOUT      ETIMEDOUT
	#define WSAECONNRESET     ECONNRESET
	#define WSAEINPROGRESS    EINPROGRESS
	#define WSAEWOULDBLOCK    EWOULDBLOCK

	#include <fcntl.h>
#elif defined(FIO_WIN32)
	#include <Ws2Tcpip.h>
	#include <Ws2Ipdef.h>
	#include <Mswsock.h>
#endif

#define INVALID_SOCKET_HANDLE INVALID_SOCKET

FIO::Socket::Socket(int type, int protocol)
	: is_open(false),
	is_bound(false),
	is_closing(false),
	is_blocking(true),
	is_connected(false),
	is_listening(false),
	is_associated(false),
	is_monitoring(false),
	type(type),
#if defined(FIO_LINUX)
	error(0),
	handle(INVALID_SOCKET_HANDLE),
#elif defined(FIO_WIN32)
	error(0),
	handle(INVALID_SOCKET_HANDLE),
#endif
	protocol(protocol),
	address_family(0),
	ip_end_point_local{},
	ip_end_point_remote{},
	thread_pool(nullptr)
{
}

FIO::Socket::~Socket()
{
	if (IsOpen())
		Close();
}

bool FIO::Socket::SetBlocking(bool value)
{
	if (!IsOpen() || is_closing)
		return false;

#if defined(FIO_LINUX)
	int flags;

	if ((flags = fcntl(GetHandle(), F_GETFL, 0)) == -1)
	{
		error = errno;

		return false;
	}

	if (value)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	if (fcntl(GetHandle(), F_SETFL, flags) == -1)
	{
		error = errno;

		return false;
	}
#elif defined(FIO_WIN32)
	u_long arg = value ? 0 : 1;

	if (ioctlsocket(GetHandle(), FIONBIO, &arg) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return false;
	}
#endif

	error       = 0;
	is_blocking = value;

	return true;
}

bool FIO::Socket::Open(int address_family)
{
	if (IsOpen())
		return false;

#if defined(FIO_LINUX)
	if ((handle = socket(address_family, GetType(), GetProtocol())) == INVALID_SOCKET)
#elif defined(FIO_WIN32)
	if ((handle = WSASocket(address_family, GetType(), GetProtocol(), nullptr, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
#endif
	{
		error = WSAGetLastError();

		return false;
	}

	this->error          = 0;
	this->address_family = address_family;
	this->is_open        = true;

	return true;
}
void FIO::Socket::Close(bool wait_for_io)
{
	if (IsOpen())
	{
		is_closing = true;

		if (wait_for_io && IsAssociated())
			thread_pool_io.Wait();

#if defined(FIO_LINUX)
		close(GetHandle());
#elif defined(FIO_WIN32)
		closesocket(GetHandle());
#endif

		if (IsAssociated())
			thread_pool_io.Wait();

		error               = 0;
		handle              = INVALID_SOCKET_HANDLE;
		thread_pool         = nullptr;
		address_family      = 0;
		ip_end_point_local  = {};
		ip_end_point_remote = {};

		is_open             = false;
		is_bound            = false;
		is_closing          = false;
		is_blocking         = true;
		is_connected        = false;
		is_associated       = false;
		is_monitoring       = false;
	}
}

bool FIO::Socket::Bind(const IPEndPoint& local_ip_end_point)
{
	if (!IsOpen() || is_closing)
		return false;

	sockaddr_storage address;
	socklen_t        address_size;
	local_ip_end_point.ToStorage(address, address_size);

	if (bind(GetHandle(), (const sockaddr*)&address, address_size) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return false;
	}

	error              = 0;
	ip_end_point_local = local_ip_end_point;
	is_bound           = true;

	return true;
}

int  FIO::Socket::Accept(Socket& socket)
{
	if (!IsListening() || is_closing || (GetType() != socket.GetType()) || (GetProtocol() != socket.GetProtocol()))
		return 0;

	decltype(Socket::handle) handle;
	sockaddr_storage         address[2]   = {};
	socklen_t                address_size = sizeof(sockaddr_storage);

	if ((handle = accept(GetHandle(), (sockaddr*)&address[1], &address_size)) == INVALID_SOCKET_HANDLE)
	{
		switch (error = WSAGetLastError())
		{
			case WSAEINTR:
			case WSAECONNRESET:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				error = 0;
				return -1;
		}

		return 0;
	}

	if (getsockname(GetHandle(), (sockaddr*)&address[0], &address_size) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

#if defined(FIO_LINUX)
		close(handle);
#elif defined(FIO_WIN32)
		closesocket(handle);
#endif

		return 0;
	}

	if (socket.IsOpen())
		socket.Close();

	error                 = 0;

	socket.handle         = handle;
	socket.address_family = GetAddressFamily();
	socket.is_open        = true;
	socket.is_connected   = true;

	IPEndPoint::FromAddress(socket.ip_end_point_local,  (const sockaddr&)address[0], address_size);
	IPEndPoint::FromAddress(socket.ip_end_point_remote, (const sockaddr&)address[1], address_size);

	return 1;
}
int  FIO::Socket::Accept(Socket& socket, AcceptCallback&& callback)
{
	if (!IsListening() || is_closing || !IsAssociated() || (GetType() != socket.GetType()) || (GetProtocol() != socket.GetProtocol()))
		return 0;

#if defined(FIO_LINUX)
	// TODO: implement linux
	return 0;
#elif defined(FIO_WIN32)
	bool socket_is_open = socket.IsOpen();

	if ((!socket_is_open && !socket.Open(GetAddressFamily())) || socket.IsBound() || socket.IsConnected() || socket.IsListening())
		return 0;

	auto context = new IOContext_Accept
	{
		.IO           = { .Callback = std::bind(&Socket::OnAccept, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Client       = &socket,
		.ClientIsOpen = socket_is_open,
		.Callback     = std::move(callback)
	};

	thread_pool_io.Add(context->IO);

	if (!AcceptEx(GetHandle(), socket.GetHandle(), context->Buffer, 0, sizeof(sockaddr_storage) + 16, sizeof(sockaddr_storage) + 16, &context->NumBytesReceived, &context->IO.O))
	{
		switch (error = WSAGetLastError())
		{
			case WSA_IO_PENDING:
				return 1;

			case WSAENOBUFS:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				if (!socket_is_open)
					socket.Close();
				thread_pool_io.Remove(context->IO);
				delete context;
				return -1;
		}

		if (!socket_is_open)
			socket.Close();

		thread_pool_io.Remove(context->IO);
		delete context;

		return 0;
	}
#endif

	return 1;
}

bool FIO::Socket::Listen()
{
	return Listen(SOMAXCONN);
}
bool FIO::Socket::Listen(uint32_t backlog)
{
	if (!IsOpen() || IsConnected() || IsListening() || is_closing)
		return false;

	if (listen(GetHandle(), (int)(backlog & INT_MAX)) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return false;
	}

	error        = 0;
	is_listening = true;

	return true;
}

bool FIO::Socket::Connect(const IPEndPoint& remote_ip_end_point)
{
	if (!IsOpen() || IsConnected() || IsListening() || is_closing)
		return false;

	sockaddr_storage address;
	socklen_t        address_size;
	remote_ip_end_point.ToStorage(address, address_size);

	if (connect(GetHandle(), (const sockaddr*)&address, address_size) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return false;
	}

	if (getsockname(GetHandle(), (sockaddr*)&address, &address_size) == SOCKET_ERROR)
		; // TODO: this should cause an error but it's not THAT important or remotely likely to happen so bad practices will live on

	IPEndPoint::FromAddress(ip_end_point_local, (const sockaddr&)address, address_size);

	error               = 0;
	ip_end_point_remote = remote_ip_end_point;
	is_connected        = true;

	return true;
}
int  FIO::Socket::Connect(const IPEndPoint& remote_ip_end_point, ConnectCallback&& callback)
{
	if (!IsOpen() || IsConnected() || IsListening() || !IsAssociated() || is_closing)
		return 0;

	sockaddr_storage address;
	socklen_t        address_size;
	remote_ip_end_point.ToStorage(address, address_size);

#if defined(FIO_LINUX)
	// TODO: implement linux
	return 0;
#elif defined(FIO_WIN32)
	LPFN_CONNECTEX connect_ex;
	GUID           connect_ex_guid = WSAID_CONNECTEX;
	DWORD          connect_ex_bytes;

	if (WSAIoctl(GetHandle(), SIO_GET_EXTENSION_FUNCTION_POINTER, &connect_ex_guid, sizeof(GUID), &connect_ex, sizeof(LPFN_CONNECTEX), &connect_ex_bytes, nullptr, nullptr) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return 0;
	}

	auto context = new IOContext_Connect
	{
		.IO       = { .Callback = std::bind(&Socket::OnConnect, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Callback = std::move(callback)
	};

	thread_pool_io.Add(context->IO);

	if (!connect_ex(GetHandle(), (const sockaddr*)&address, address_size, nullptr, 0, &context->NumBytesSent, &context->IO.O))
	{
		switch (error = WSAGetLastError())
		{
			case WSA_IO_PENDING:
				return 1;

			case WSAEALREADY:
			case WSAETIMEDOUT:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				thread_pool_io.Remove(context->IO);
				delete context;
				return -1;
		}

		thread_pool_io.Remove(context->IO);
		delete context;

		return 0;
	}

	OnConnect(*thread_pool, context->IO, context->NumBytesSent);
#endif

	return 1;
}

bool FIO::Socket::Monitor(bool set)
{
	if (!IsOpen())
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	DWORD buffer_in      = set ? RCVALL_ON : RCVALL_OFF;
	DWORD bytes_returned = 0;

	if (WSAIoctl(GetHandle(), SIO_RCVALL, &buffer_in, sizeof(buffer_in), nullptr, 0, &bytes_returned, nullptr, nullptr) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return false;
	}
#endif

	is_monitoring = set;

	return true;
}

bool FIO::Socket::Shutdown(int type)
{
	if (!IsOpen() || is_closing)
		return false;

	if (shutdown(GetHandle(), type) == SOCKET_ERROR)
	{
		error = WSAGetLastError();

		return false;
	}

	return true;
}

bool FIO::Socket::Associate(ThreadPool& pool)
{
	if (!IsOpen() || is_closing)
		return false;

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	if (!pool.Associate((HANDLE)GetHandle()))
		return false;
#endif

	thread_pool   = &pool;
	is_associated = true;

	return true;
}

bool FIO::Socket::Send(const void* buffer, size_t size, size_t& number_of_bytes_sent)
{
	if (!IsOpen() || is_closing)
		return false;

	int num_bytes_sent;

	if ((num_bytes_sent = send(GetHandle(), (const char*)buffer, (int)(size & INT_MAX), 0)) == SOCKET_ERROR)
	{
		switch (error = WSAGetLastError())
		{
			case WSAEINTR:
			case WSAENOBUFS:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				number_of_bytes_sent = 0;
				return true;
		}

		return false;
	}

	error                = 0;
	number_of_bytes_sent = num_bytes_sent;

	return true;
}
int  FIO::Socket::Send(const void* buffer, size_t size, SendCallback&& callback)
{
	if (!IsOpen() || !IsAssociated() || is_closing)
		return 0;

#if defined(FIO_LINUX)
	// TODO: implement linux
	return 0;
#elif defined(FIO_WIN32)
	auto context = new IOContext_Send
	{
		.IO       = { .Callback = std::bind(&Socket::OnSend, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Buffer   = { .len = (u_long)size, .buf = (char*)buffer },
		.Callback = std::move(callback)
	};

	thread_pool_io.Add(context->IO);

	if (WSASend(GetHandle(), &context->Buffer, 1, &context->NumBytesSent, 0, &context->IO.O, nullptr))
	{
		switch (error = WSAGetLastError())
		{
			case WSA_IO_PENDING:
				return 1;

			case WSAENOBUFS:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				thread_pool_io.Remove(context->IO);
				delete context;
				return -1;
		}

		thread_pool_io.Remove(context->IO);
		delete context;

		return 0;
	}
#endif

	return 1;
}

bool FIO::Socket::SendTo(const void* buffer, size_t size, const IPEndPoint& remote_ip_end_point, size_t& number_of_bytes_sent)
{
	if (!IsOpen() || is_closing)
		return false;

	sockaddr_storage address;
	socklen_t        address_size;
	remote_ip_end_point.ToStorage(address, address_size);

	int num_bytes_sent;

	if ((num_bytes_sent = sendto(GetHandle(), (const char*)buffer, (int)(size & INT_MAX), 0, (const sockaddr*)&address, address_size)) == SOCKET_ERROR)
	{
		switch (error = WSAGetLastError())
		{
			case WSAEINTR:
			case WSAENOBUFS:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				number_of_bytes_sent = 0;
				return true;
		}

		return false;
	}

	error                = 0;
	number_of_bytes_sent = num_bytes_sent;

	return true;
}
int  FIO::Socket::SendTo(const void* buffer, size_t size, const IPEndPoint& remote_ip_end_point, SendToCallback&& callback)
{
	if (!IsOpen() || !IsAssociated() || is_closing)
		return 0;

	sockaddr_storage address;
	socklen_t        address_size;
	remote_ip_end_point.ToStorage(address, address_size);

#if defined(FIO_LINUX)
	// TODO: implement linux
	return 0;
#elif defined(FIO_WIN32)
	auto context = new IOContext_SendTo
	{
		.IO             = { .Callback = std::bind(&Socket::OnSendTo, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Buffer         = { .len = (u_long)size, .buf = (char*)buffer },
		.Callback       = std::move(callback),
		.RemoteEndPoint = remote_ip_end_point
	};

	thread_pool_io.Add(context->IO);

	if (WSASendTo(GetHandle(), &context->Buffer, 1, &context->NumBytesSent, 0, (const sockaddr*)&address, address_size, &context->IO.O, nullptr))
	{
		switch (error = WSAGetLastError())
		{
			case WSA_IO_PENDING:
				return 1;

			case WSAENOBUFS:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				thread_pool_io.Remove(context->IO);
				delete context;
				return -1;
		}

		thread_pool_io.Remove(context->IO);
		delete context;

		return 0;
	}
#endif

	return 1;
}

bool FIO::Socket::Receive(void* buffer, size_t size, size_t& number_of_bytes_received)
{
	if (!IsOpen() || is_closing)
		return false;

	int num_bytes_received;

	if ((num_bytes_received = recv(GetHandle(), (char*)buffer, (int)(size & INT_MAX), 0)) == SOCKET_ERROR)
	{
		switch (error = WSAGetLastError())
		{
			case WSAEINTR:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				number_of_bytes_received = 0;
				return true;
		}

		return false;
	}

	error                    = 0;
	number_of_bytes_received = num_bytes_received;

	return true;
}
int  FIO::Socket::Receive(void* buffer, size_t size, ReceiveCallback&& callback)
{
	if (!IsOpen() || !IsAssociated() || is_closing)
		return 0;

#if defined(FIO_LINUX)
	// TODO: implement linux
	return 0;
#elif defined(FIO_WIN32)
	auto context = new IOContext_Receive
	{
		.IO       = { .Callback = std::bind(&Socket::OnReceive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Flags    = 0,
		.Buffer   = { .len = (u_long)size, .buf = (char*)buffer },
		.Callback = std::move(callback)
	};

	thread_pool_io.Add(context->IO);

	if (WSARecv(GetHandle(), &context->Buffer, 1, &context->NumBytesReceived, &context->Flags, &context->IO.O, nullptr))
	{
		switch (error = WSAGetLastError())
		{
			case WSA_IO_PENDING:
				return 1;

			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				thread_pool_io.Remove(context->IO);
				delete context;
				return -1;
		}

		thread_pool_io.Remove(context->IO);
		delete context;

		return 0;
	}
#endif

	return 1;
}

bool FIO::Socket::ReceiveFrom(void* buffer, size_t size, IPEndPoint& remote_ip_end_point, size_t& number_of_bytes_received)
{
	if (!IsOpen() || is_closing)
		return false;

	sockaddr_storage address;
	socklen_t        address_size = sizeof(sockaddr_storage);

	int num_bytes_received;

	if ((num_bytes_received = recvfrom(GetHandle(), (char*)buffer, (int)(size & INT_MAX), 0, (sockaddr*)&address, &address_size)) == SOCKET_ERROR)
	{
		switch (error = WSAGetLastError())
		{
			case WSAEINTR:
			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				number_of_bytes_received = 0;
				return true;
		}

		return false;
	}

	IPEndPoint::FromAddress(remote_ip_end_point, (const sockaddr&)address, address_size);

	error                    = 0;
	number_of_bytes_received = num_bytes_received;

	return true;
}
int  FIO::Socket::ReceiveFrom(void* buffer, size_t size, ReceiveFromCallback&& callback)
{
	if (!IsOpen() || !IsAssociated() || is_closing)
		return 0;

#if defined(FIO_LINUX)
	// TODO: implement linux
	return 0;
#elif defined(FIO_WIN32)
	auto context = new IOContext_ReceiveFrom
	{
		.IO             = { .Callback = std::bind(&Socket::OnReceiveFrom, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
		.Flags          = 0,
		.Buffer         = { .len = (u_long)size, .buf = (char*)buffer },
		.Callback       = std::move(callback),
		.RemoteEndPoint = { .Size = sizeof(sockaddr_storage) }
	};

	thread_pool_io.Add(context->IO);

	if (WSARecvFrom(GetHandle(), &context->Buffer, 1, &context->NumBytesReceived, &context->Flags, (sockaddr*)&context->RemoteEndPoint.Address, &context->RemoteEndPoint.Size, &context->IO.O, nullptr))
	{
		switch (error = WSAGetLastError())
		{
			case WSA_IO_PENDING:
				return 1;

			case WSAEINPROGRESS:
			case WSAEWOULDBLOCK:
				thread_pool_io.Remove(context->IO);
				delete context;
				return -1;
		}

		thread_pool_io.Remove(context->IO);
		delete context;

		return 0;
	}
#endif

	return 1;
}

#define fio_socket_get_io_context(type, context) (type*)&context
#define fio_socket_remove_io_context(context)    thread_pool_io.Remove(context->IO); delete context
void FIO::Socket::OnAccept(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	auto accept = fio_socket_get_io_context(IOContext_Accept, io);

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	if (this->error = accept->IO.O.Internal)
	{
		if (!accept->ClientIsOpen)
			accept->Client->Close();

		accept->Callback(*this, *accept->Client);
	}
	else if (setsockopt(accept->Client->GetHandle(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&this->handle, sizeof(SOCKET)) == SOCKET_ERROR)
	{
		auto error = WSAGetLastError();

		if (!accept->ClientIsOpen)
			accept->Client->Close();

		accept->Client->error = error;

		accept->Callback(*this, *accept->Client);
	}
	else
	{
		sockaddr_storage address[2]      = {};
		socklen_t        address_size[2] = { sizeof(sockaddr_storage), sizeof(sockaddr_storage) };

		if ((getsockname(accept->Client->GetHandle(), (sockaddr*)&address[0], &address_size[0]) == SOCKET_ERROR) ||
			(getpeername(accept->Client->GetHandle(), (sockaddr*)&address[1], &address_size[1]) == SOCKET_ERROR))
		{
			auto error = WSAGetLastError();

			if (!accept->ClientIsOpen)
				accept->Client->Close();

			accept->Client->error = error;

			accept->Callback(*this, *accept->Client);
		}
		else
		{
			IPEndPoint::FromAddress(accept->Client->ip_end_point_local,  (const sockaddr&)address[0], address_size[0]);
			IPEndPoint::FromAddress(accept->Client->ip_end_point_remote, (const sockaddr&)address[1], address_size[1]);

			accept->Client->is_open      = true;
			accept->Client->is_connected = true;

			accept->Callback(*this, *accept->Client);
		}
	}
#endif

	fio_socket_remove_io_context(accept);
}
void FIO::Socket::OnConnect(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	auto connect = fio_socket_get_io_context(IOContext_Connect, io);

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	if (this->error = connect->IO.O.Internal)
		;
	else if (setsockopt(GetHandle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR)
		this->error = WSAGetLastError();
	else
	{
		sockaddr_storage address[2]      = {};
		socklen_t        address_size[2] = { sizeof(sockaddr_storage), sizeof(sockaddr_storage) };

		if ((getsockname(GetHandle(), (sockaddr*)&address[0], &address_size[0]) == SOCKET_ERROR) ||
			(getpeername(GetHandle(), (sockaddr*)&address[1], &address_size[1]) == SOCKET_ERROR))
		{
			this->error = WSAGetLastError();
		}
		else
		{
			IPEndPoint::FromAddress(this->ip_end_point_local,  (const sockaddr&)address[0], address_size[0]);
			IPEndPoint::FromAddress(this->ip_end_point_remote, (const sockaddr&)address[1], address_size[1]);

			this->is_connected = true;
		}
	}

	connect->Callback(*this);
#endif

	fio_socket_remove_io_context(connect);
}
void FIO::Socket::OnSend(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	auto send = fio_socket_get_io_context(IOContext_Send, io);

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	this->error = send->IO.O.Internal;

	send->Callback(*this, send->Buffer.buf, send->Buffer.len, number_of_bytes_transferred);
#endif

	fio_socket_remove_io_context(send);
}
void FIO::Socket::OnSendTo(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	auto send_to = fio_socket_get_io_context(IOContext_SendTo, io);

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	this->error = send_to->IO.O.Internal;

	send_to->Callback(*this, send_to->Buffer.buf, send_to->Buffer.len, number_of_bytes_transferred, send_to->RemoteEndPoint);
#endif

	fio_socket_remove_io_context(send_to);
}
void FIO::Socket::OnReceive(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	auto receive = fio_socket_get_io_context(IOContext_Receive, io);

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	this->error = receive->IO.O.Internal;

	receive->Callback(*this, receive->Buffer.buf, receive->Buffer.len, number_of_bytes_transferred);
#endif

	fio_socket_remove_io_context(receive);
}
void FIO::Socket::OnReceiveFrom(ThreadPool& pool, ThreadPool::IOContext& io, size_t number_of_bytes_transferred)
{
	auto receive_from = fio_socket_get_io_context(IOContext_ReceiveFrom, io);

	IPEndPoint remote_ip_end_point;
	IPEndPoint::FromAddress(remote_ip_end_point, (const sockaddr&)receive_from->RemoteEndPoint.Address, receive_from->RemoteEndPoint.Size);

#if defined(FIO_LINUX)
	// TODO: implement linux
#elif defined(FIO_WIN32)
	this->error = receive_from->IO.O.Internal;

	receive_from->Callback(*this, receive_from->Buffer.buf, receive_from->Buffer.len, number_of_bytes_transferred, remote_ip_end_point);
#endif

	fio_socket_remove_io_context(receive_from);
}
