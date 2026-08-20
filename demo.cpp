#include <atomic>
#include <format>
#include <iostream>
#include <iterator>

#include <FIO/DNS.hpp>
#include <FIO/Path.hpp>
#include <FIO/File.hpp>
#include <FIO/Timer.hpp>
#include <FIO/Socket.hpp>
#include <FIO/MPSCQueue.hpp>
#include <FIO/Directory.hpp>
#include <FIO/ByteBuffer.hpp>
#include <FIO/ThreadPool.hpp>

#ifdef FIO_WIN32
	#include <FIO/WinSock2.hpp>
#endif

template<typename ... T>
void print(std::string_view format, T ... args)
{
	std::ostream_iterator<char> it(std::cout);
	static std::atomic_flag     busy;
	static FIO::Timer           timer;

	while (busy.test_and_set(std::memory_order_acquire))
		while (busy.test(std::memory_order_relaxed))
			;

	std::format_to(it, "[{:.6f}] ", timer.GetElapsed().ToMicroseconds() / 1000000.0f);
#if defined(FIO_LINUX)
	std::cout << '[' << gettid() << "] ";
#elif defined(FIO_WIN32)
	std::cout << '[' << GetCurrentThreadId() << "] ";
#endif
	if constexpr (sizeof...(T) == 0)
		std::cout << format;
	else
		std::vformat_to(it, format, std::make_format_args(args ...));

	std::cout << std::endl;

	busy.clear(std::memory_order_release);
}

class demo_dns
{
#ifdef FIO_WIN32
	FIO::WinSock2 ws2;
#endif
	int           family;
	std::string   hostname;

public:
	demo_dns(std::string_view hostname, int family)
		: family(family),
		hostname(hostname)
	{
	}

	void run()
	{
		print("FIO::DNS::Enumerate() -> {}", FIO::DNS::Enumerate(hostname, family, std::bind(&demo_dns::on_enum, this, std::placeholders::_1)));
	}

private:
	bool on_enum(const FIO::IPAddress& ip_address)
	{
		print(ip_address.ToString());

		return true;
	}
};

class demo_file_in
{
	FIO::File       file;
	uint8_t         buffer[0xFF];
	FIO::ThreadPool threads;

public:
	demo_file_in(std::string_view path, size_t thread_count)
		: file(path, FIO::File::MODE_READ),
		threads(thread_count)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("file.Open() -> {}", file.Open());
		print("file.Associate() -> {}", file.Associate(threads));

		print("file.Read() -> {}", file.Read(buffer, sizeof(buffer), std::bind(&demo_file_in::on_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> {}", (threads.Shutdown(), true));
		print("threads.Join() -> {}", threads.Join());
	}

private:
	void on_read(FIO::File& file, void* buffer, size_t size, size_t number_of_bytes_read)
	{
		print("read {}/{} bytes [error: {}]", file.GetReadPosition(), file.GetSize(), file.GetLastError());

		if (number_of_bytes_read)
			print("file.Read() -> {}", file.Read(buffer, size, std::bind(&demo_file_in::on_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
	}
};

class demo_file_out
{
	FIO::File       file;
	uint8_t         buffer[0xFF];
	FIO::ThreadPool threads;

public:
	demo_file_out(std::string_view path, size_t thread_count)
		: file(path, FIO::File::MODE_WRITE | FIO::File::MODE_TRUNCATE),
		threads(thread_count)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("file.Open() -> {}", file.Open());
		print("file.Associate() -> {}", file.Associate(threads));

		print("file.Write() -> {}", file.Write(buffer, sizeof(buffer), std::bind(&demo_file_out::on_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> {}", (threads.Shutdown(), true));
		print("threads.Join() -> {}", threads.Join());
	}

private:
	void on_write(FIO::File& file, const void* buffer, size_t size, size_t number_of_bytes_written)
	{
		print("wrote {} bytes [error: {}]", number_of_bytes_written, file.GetLastError());

		print("file.Write() -> {}", file.Write(buffer, size, std::bind(&demo_file_out::on_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
	}
};

class demo_socket_tcp
{
#ifdef FIO_WIN32
	FIO::WinSock2   ws2;
#endif
	FIO::ThreadPool threads;
	FIO::IPEndPoint end_point;

	FIO::Socket     socket1;
	uint8_t         socket1_buffer[0xFF];
	FIO::Socket     socket1_client;

	FIO::Socket     socket2;
	uint8_t         socket2_buffer[sizeof(socket1_buffer)];

public:
	demo_socket_tcp(FIO::IPEndPoint&& local_end_point, size_t thread_count)
		: threads(thread_count),
		end_point(std::move(local_end_point)),
		socket1(FIO::Socket::TYPE_TCP, end_point.Host.Family),
		socket1_client(FIO::Socket::TYPE_TCP, end_point.Host.Family),
		socket2(FIO::Socket::TYPE_TCP, end_point.Host.Family)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("socket1.Open() -> {}", socket1.Open());
		print("socket1.Associate() -> {}", socket1.Associate(threads));
		print("socket1.Bind() -> {}", socket1.Bind(end_point));
		print("socket1.Listen() -> {}", socket1.Listen());
		print("socket1.Accept() -> {}", socket1.Accept(socket1_client, std::bind(&demo_socket_tcp::on_accept, this, std::placeholders::_1, std::placeholders::_2)));

		print("socket2.Open() -> {}", socket2.Open());
		print("socket2.Associate() -> {}", socket2.Associate(threads));
		print("socket2.Connect() -> {}", socket2.Connect(end_point));
		print("socket2.Send() -> {}", socket2.Send(socket2_buffer, sizeof(socket2_buffer), std::bind(&demo_socket_tcp::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> {}", (threads.Shutdown(), true));
		print("threads.Join() -> {}", threads.Join());
	}

private:
	void on_accept(FIO::Socket& socket, FIO::Socket& client)
	{
		print("accepted connection from {} [error: {}]", client.GetRemoteEndPoint().ToString(), socket.GetLastError());

		print("socket1_client.Associate() -> {}", client.Associate(threads));
		print("socket1_client.Receive() -> {}", client.Receive(socket1_buffer, sizeof(socket1_buffer), std::bind(&demo_socket_tcp::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
	}

	void on_send(FIO::Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent)
	{
		print("sent {} bytes [error: {}]", number_of_bytes_sent, socket.GetLastError());

		socket.Send(buffer, size, std::bind(&demo_socket_tcp::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}

	void on_receive(FIO::Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received)
	{
		print("received {} bytes [error: {}]", number_of_bytes_received, socket.GetLastError());

		socket.Receive(buffer, size, std::bind(&demo_socket_tcp::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}
};

class demo_socket_udp
{
#ifdef FIO_WIN32
	FIO::WinSock2   ws2;
#endif
	FIO::ThreadPool threads;
	FIO::IPEndPoint end_point;

	FIO::Socket     socket1;
	uint8_t         socket1_buffer[0xFF];

	FIO::Socket     socket2;
	uint8_t         socket2_buffer[sizeof(socket1_buffer)];

public:
	demo_socket_udp(FIO::IPEndPoint&& local_end_point, size_t thread_count)
		: threads(thread_count),
		end_point(std::move(local_end_point)),
		socket1(FIO::Socket::TYPE_UDP, end_point.Host.Family),
		socket2(FIO::Socket::TYPE_UDP, end_point.Host.Family)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("socket1.Open() -> {}", socket1.Open());
		print("socket1.Associate() -> {}", socket1.Associate(threads));
		print("socket1.Bind() -> {}", socket1.Bind(end_point));
		print("socket1.Receive() -> {}", socket1.Receive(socket1_buffer, sizeof(socket1_buffer), std::bind(&demo_socket_udp::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		print("socket2.Open() -> {}", socket2.Open());
		print("socket2.Associate() -> {}", socket2.Associate(threads));
		print("socket2.Connect() -> {}", socket2.Connect(end_point));
		print("socket2.Send() -> {}", socket2.Send(socket2_buffer, sizeof(socket2_buffer), std::bind(&demo_socket_udp::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> {}", (threads.Shutdown(), true));
		print("threads.Join() -> {}", threads.Join());
	}

private:
	void on_send(FIO::Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent)
	{
		print("sent {} bytes [error: {}]", number_of_bytes_sent, socket.GetLastError());

		socket.Send(buffer, size, std::bind(&demo_socket_udp::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}

	void on_receive(FIO::Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received)
	{
		print("received {} bytes [error: {}]", number_of_bytes_received, socket.GetLastError());

		socket.Receive(buffer, size, std::bind(&demo_socket_udp::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}
};

class demo_directory
{
	std::string path;
	bool        path_is_valid;

public:
	demo_directory()
		: path_is_valid(FIO::Directory::GetCurrentPath(path))
	{
		print("FIO::Directory::GetCurrentPath() -> {}", path_is_valid);
	}

	void run()
	{
		if (path_is_valid)
			print("FIO::Directory::Enumerate() -> {}", FIO::Directory::Enumerate(path, std::bind(&demo_directory::on_enum, this, std::placeholders::_1, std::placeholders::_2)));
	}

private:
	bool on_enum(std::string_view path, int type)
	{
		print("{}, {}", type, path);

		return true;
	}
};

class demo_byte_buffer
{
	FIO::ByteBuffer buffer;
	uint8_t         memory[0xFF];

public:
	demo_byte_buffer()
		: buffer(FIO::ByteBuffer::Open(memory, sizeof(memory), FIO::Endian::MACHINE))
	{
		memset(memory, 0, sizeof(memory));
	}

	void run()
	{
		std::string str("Hello world");
		print("buffer.Write() -> {}", buffer.Write(str));
		print("buffer.Read() -> {}", buffer.Read(str));

		std::wstring wstr(L"Hello world");
		print("buffer.Write() -> {}", buffer.Write(wstr));
		print("buffer.Read() -> {}", buffer.Read(wstr));

		uint32_t uint = 0x12345678;
		print("buffer.Write() -> {}", buffer.Write(uint));
		print("buffer.Read() -> {}", buffer.Read(uint));
	}
};

class demo_mpsc_queue
{
	FIO::MPSCQueue<int> queue;

public:
	demo_mpsc_queue()
	{
	}

	void run()
	{
		for (int i = 0; i < 100; ++i)
		{
			print("queue.Push() -> {}", (queue.Push(std::move(i)), true));
			print("queue.Pop() -> {}", queue.Pop(i));
		}
	}
};

#define THREAD_COUNT 4

int main(int argc, char* argv[])
{
	// demo_dns("www.google.com", FIO::ADDRESS_FAMILY_IP_V4).run();
	// demo_dns("www.google.com", FIO::ADDRESS_FAMILY_IP_V6).run();

	// demo_file_in("./demo.bin", THREAD_COUNT).run();
	// demo_file_out("./demo.bin", THREAD_COUNT).run();

	// demo_socket_tcp(FIO::IPEndPoint::Loopback(9001), THREAD_COUNT).run();
	// demo_socket_udp(FIO::IPEndPoint::Loopback(9001), THREAD_COUNT).run();

	// demo_directory().run();

	// demo_byte_buffer().run();

	// demo_mpsc_queue().run();

	return 0;
}
