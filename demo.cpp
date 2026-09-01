#include <format>
#include <string>
#include <iostream>
#include <iterator>

#include <FIO/DNS.hpp>
#include <FIO/Path.hpp>
#include <FIO/File.hpp>
#include <FIO/Timer.hpp>
#include <FIO/Socket.hpp>
#include <FIO/SpinLock.hpp>
#include <FIO/MPSCQueue.hpp>
#include <FIO/Directory.hpp>
#include <FIO/ByteBuffer.hpp>
#include <FIO/ThreadPool.hpp>

#ifdef FIO_WIN32
	#include <FIO/WinSock2.hpp>
#endif

FIO::SpinLock    print_lock;
const FIO::Timer print_timer;

template<typename ... T>
void print(std::string_view format, T ... args)
{
	std::ostream_iterator<char> it(std::cout);

	FIO::SpinLockGuard lock(print_lock);

	std::format_to(it, "[{:.6f}] ", print_timer.GetElapsed().ToMicroseconds() / 1000000.0f);
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
}
template<typename ... T>
void print(std::wstring_view format, T ... args)
{
	std::ostreambuf_iterator<wchar_t> it(std::wcout);

	FIO::SpinLockGuard lock(print_lock);

	std::format_to(it, L"[{:.6f}] ", print_timer.GetElapsed().ToMicroseconds() / 1000000.0f);
#if defined(FIO_LINUX)
	std::wcout << L'[' << gettid() << L"] ";
#elif defined(FIO_WIN32)
	std::wcout << L'[' << GetCurrentThreadId() << L"] ";
#endif
	if constexpr (sizeof...(T) == 0)
		std::wcout << format;
	else
		std::vformat_to(it, format, std::make_wformat_args(args ...));

	std::wcout << std::endl;
}

class demo_dns
{
	std::string hostname;

public:
	demo_dns(std::string_view hostname)
		: hostname(hostname)
	{
	}

	void run()
	{
		print("FIO::DNS::Enumerate() -> {}", FIO::DNS::Enumerate(hostname, std::bind(&demo_dns::on_enum, this, std::placeholders::_1)));
	}

private:
	bool on_enum(const FIO::IPAddress& ip_address)
	{
		print(ip_address.ToString());

		return true;
	}
};

class demo_ws2
{
#ifdef FIO_WIN32
	FIO::WinSock2 ws2;
#endif

public:
	demo_ws2()
	{
	}

	void run()
	{
#ifdef FIO_WIN32
		print("ws2.IsLoaded() -> {}", ws2.IsLoaded());

		if (auto data = ws2.GetData())
		{
			print("data->wVersion -> {}.{}", (data->wVersion >> 8), (data->wVersion & 0xFF));
			print("data->wHighVersion -> {}.{}", (data->wHighVersion >> 8), (data->wHighVersion & 0xFF));
			print("data->iMaxSockets -> {}", data->iMaxSockets);
			print("data->iMaxUdpDg -> {}", data->iMaxUdpDg);
			print("data->lpVendorInfo -> {}", data->lpVendorInfo ? data->lpVendorInfo : "");
			print("data->szDescription -> {}", data->szDescription);
			print("data->szSystemStatus -> {}", data->szSystemStatus);
		}
#endif
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
		socket1(FIO::Socket::TYPE_STREAM, FIO::Socket::PROTOCOL_TCP),
		socket1_client(FIO::Socket::TYPE_STREAM, FIO::Socket::PROTOCOL_TCP),
		socket2(FIO::Socket::TYPE_STREAM, FIO::Socket::PROTOCOL_TCP)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("socket1.Open() -> {}", socket1.Open(end_point.Host.Family));
		print("socket1.Associate() -> {}", socket1.Associate(threads));
		print("socket1.Bind() -> {}", socket1.Bind(end_point));
		print("socket1.Listen() -> {}", socket1.Listen());
		print("socket1.Accept() -> {}", socket1.Accept(socket1_client, std::bind(&demo_socket_tcp::on_accept, this, std::placeholders::_1, std::placeholders::_2)));

		print("socket2.Open() -> {}", socket2.Open(end_point.Host.Family));
		print("socket2.Associate() -> {}", socket2.Associate(threads));
		print("socket2.Bind() -> {}", socket2.Bind({ .Host = FIO::IPAddress { .Family = end_point.Host.Family }, .Port = 0 }));
		print("socket2.Connect() -> {}", socket2.Connect(end_point, std::bind(&demo_socket_tcp::on_connect, this, std::placeholders::_1)));

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

	void on_connect(FIO::Socket& socket)
	{
		print("connected to {} [error: {}]", socket.GetRemoteEndPoint().ToString(), socket.GetLastError());

		print("socket2.Send() -> {}", socket.Send(socket2_buffer, sizeof(socket2_buffer), std::bind(&demo_socket_tcp::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
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
		socket1(FIO::Socket::TYPE_DGRAM, FIO::Socket::PROTOCOL_UDP),
		socket2(FIO::Socket::TYPE_DGRAM, FIO::Socket::PROTOCOL_UDP)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("socket1.Open() -> {}", socket1.Open(end_point.Host.Family));
		print("socket1.Associate() -> {}", socket1.Associate(threads));
		print("socket1.Bind() -> {}", socket1.Bind(end_point));
		print("socket1.Receive() -> {}", socket1.Receive(socket1_buffer, sizeof(socket1_buffer), std::bind(&demo_socket_udp::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		print("socket2.Open() -> {}", socket2.Open(end_point.Host.Family));
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

class demo_socket_raw_sniff_v4
{
	uint8_t         buffer[0xFFF];
	FIO::Socket     socket;
	FIO::ThreadPool threads;
	FIO::IPEndPoint end_point;

public:
	demo_socket_raw_sniff_v4(const FIO::IPAddress& address, size_t thread_count)
		: socket(FIO::Socket::TYPE_RAW, FIO::Socket::PROTOCOL_IPV4),
		threads(thread_count),
		end_point{
			.Host = address,
			.Port = 0
		}
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		print("socket.Open() -> {}", socket.Open(end_point.Host.Family));
		print("socket.Bind() -> {}", socket.Bind(end_point));
		print("socket.Associate() -> {}", socket.Associate(threads));
		print("socket.Monitor() -> {}", socket.Monitor());
		print("socket.Receive() -> {}", socket.Receive(buffer, sizeof(buffer), std::bind(&demo_socket_raw_sniff_v4::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> {}", (threads.Shutdown(), true));
		print("threads.Join() -> {}", threads.Join());
	}

private:
	void on_receive(FIO::Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received)
	{
		std::string hex(number_of_bytes_received * 2, '\0');
		auto        src  = (const uint8_t*)buffer;
		auto        dest = hex.data();

		static constexpr char HEX[] = "0123456789ABCDEF";

		for (size_t i = 0; i < number_of_bytes_received; ++i, ++src)
		{
			*dest++ = HEX[(*src >> 4)];
			*dest++ = HEX[(*src & 0xF)];
		}

		print("received {} bytes [error: {}]: {}", number_of_bytes_received, socket.GetLastError(), hex);

		socket.Receive(buffer, size, std::bind(&demo_socket_raw_sniff_v4::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
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
		: buffer(FIO::ByteBuffer::Open(memory, sizeof(memory), FIO::Endian::BIG))
	{
		memset(memory, 0, sizeof(memory));
	}

	void run()
	{
		std::string str("Hello world");
		print("buffer.Write(\"{}\") -> {}", str, buffer.Write(str));
		print("buffer.Read(\"{}\") -> {}", str, buffer.Read(str));

		std::wstring wstr(L"Hello world");
		print(L"buffer.Write(\"{}\") -> {}", wstr, buffer.Write(wstr));
		print(L"buffer.Read(\"{}\") -> {}", wstr, buffer.Read(wstr));

		uint32_t uint32 = 0x12345678;
		print("buffer.Write({}) -> {}", uint32, buffer.Write(uint32));
		print("buffer.Read({}) -> {}", uint32, buffer.Read(uint32));

		uint64_t uint64 = 0x1234567812345678;
		print("buffer.Write({}) -> {}", uint64, buffer.Write(uint64));
		print("buffer.Read({}) -> {}", uint64, buffer.Read(uint64));
	}
};

class demo_mpsc_queue
{
	FIO::MPSCQueue<int> queue;
	FIO::ThreadPool     threads;

public:
	demo_mpsc_queue(size_t thread_count)
		: threads(thread_count)
	{
	}

	void run()
	{
		print("threads.Start() -> {}", threads.Start());

		for (int i = 0; i < 100; ++i)
			threads.Post([this, i](FIO::ThreadPool& pool) {
				queue.Push((int&&)i);

				print("queue.Push({}) -> {}", i, true);
			});

		for (int i = 0, j; i < 100; )
		{
			if (!queue.Pop(j))
				continue;

			print("queue.Pop({}) -> {}", j, true);

			++i;
		}

		print("threads.Shutdown() -> {}", (threads.Shutdown(), true));
		print("threads.Join() -> {}", threads.Join());
	}
};

#define THREAD_COUNT 4

int main(int argc, char* argv[])
{
	// demo_dns("www.google.com").run();

	// demo_ws2().run();

	// demo_file_in("./demo.bin", THREAD_COUNT).run();
	// demo_file_out("./demo.bin", THREAD_COUNT).run();

	// demo_socket_tcp(FIO::IPEndPoint::Loopback(9001), THREAD_COUNT).run();
	// demo_socket_udp(FIO::IPEndPoint::Loopback(9001), THREAD_COUNT).run();
	// demo_socket_raw_sniff_v4(FIO::IPAddress::Loopback, THREAD_COUNT).run();

	// demo_directory().run();

	// demo_byte_buffer().run();

	// demo_mpsc_queue(THREAD_COUNT).run();

	return 0;
}
