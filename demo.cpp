#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <cinttypes>

#include <FIO/DNS.hpp>
#include <FIO/File.hpp>
#include <FIO/Timer.hpp>
#include <FIO/Socket.hpp>
#include <FIO/ThreadPool.hpp>

#ifdef FIO_WIN32
	#include <FIO/WinSock2.hpp>
#endif

void print(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	{
		static std::atomic_flag busy;
		static FIO::Timer       timer;

		while (busy.test_and_set(std::memory_order_acquire))
			while (busy.test(std::memory_order_relaxed))
				;

		printf("[%" PRIu64 "] ", timer.GetElapsed().ToMilliseconds());
#if defined(FIO_LINUX)
		printf("[Thread: %" PRIi64 "] ", (int64_t)gettid());
#elif defined(FIO_WIN32)
		printf("[Thread: %" PRIu32 "] ", (uint32_t)GetCurrentThreadId());
#endif
		vprintf(format, args);
		putc('\n', stdout);

		busy.clear(std::memory_order_release);
	}

	va_end(args);
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
		print("FIO::DNS::Enumerate() -> %i", FIO::DNS::Enumerate(hostname, family, std::bind(&demo_dns::on_enum, this, std::placeholders::_1)));
	}

private:
	bool on_enum(const FIO::IPAddress& ip_address)
	{
		print("%s", ip_address.ToString().c_str());

		return true;
	}
};

class demo_file
{
	FIO::File       file;
	uint8_t         buffer[0xFF];
	FIO::ThreadPool threads;

public:
	demo_file(std::string_view path, int mode, size_t thread_count)
		: file(path, mode),
		threads(thread_count)
	{
	}

	void run()
	{
		print("threads.Start() -> %i", threads.Start());

		print("file.Open() -> %i", file.Open());
		print("file.Associate() -> %i", file.Associate(threads));

		if (file.IsReadOnly())
			print("file.Read() -> %i", file.Read(buffer, sizeof(buffer), std::bind(&demo_file::on_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
		else
			print("file.Write() -> %i", file.Write(buffer, sizeof(buffer), std::bind(&demo_file::on_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> %i", (threads.Shutdown(), true));
		print("threads.Join() -> %i", threads.Join());
	}

private:
	void on_read(FIO::File& file, void* buffer, size_t size, size_t number_of_bytes_read)
	{
		print("read %" PRIu64 "/%" PRIu64 " bytes (error: 0x%08" PRIX32 ")", file.GetReadPosition(), file.GetSize(), file.GetLastError());

		if (number_of_bytes_read)
			print("file.Read() -> %i", file.Read(buffer, size, std::bind(&demo_file::on_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
	}
	void on_write(FIO::File& file, const void* buffer, size_t size, size_t number_of_bytes_written)
	{
		print("wrote %zu bytes (error: 0x%08" PRIX32 ")", number_of_bytes_written, file.GetLastError());

		print("file.Write() -> %i", file.Write(buffer, size, std::bind(&demo_file::on_write, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
	}
};

class demo_socket
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
	demo_socket(int socket_type, FIO::IPEndPoint&& local_end_point, size_t thread_count)
		: threads(thread_count),
		end_point(std::move(local_end_point)),
		socket1(socket_type, end_point.Host.Family),
		socket1_client(socket_type, end_point.Host.Family),
		socket2(socket_type, end_point.Host.Family)
	{
	}

	void run()
	{
		print("threads.Start() -> %i", threads.Start());

		print("socket1.Open() -> %i", socket1.Open());
		print("socket1.Bind() -> %i", socket1.Bind(end_point));
		print("socket1.Associate() -> %i", socket1.Associate(threads));

		print("socket2.Open() -> %i", socket2.Open());
		print("socket2.Associate() -> %i", socket2.Associate(threads));

		switch (socket1.GetType())
		{
			case FIO::SOCKET_TYPE_TCP:
				print("socket1.Listen() -> %i", socket1.Listen());
				print("socket1.Accept() -> %i", socket1.Accept(socket1_client, std::bind(&demo_socket::on_accept, this, std::placeholders::_1, std::placeholders::_2)));
				break;

			case FIO::SOCKET_TYPE_UDP:
				print("socket1.Receive() -> %i", socket1.Receive(socket1_buffer, sizeof(socket1_buffer), std::bind(&demo_socket::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
				break;
		}

		print("socket2.Connect() -> %i", socket2.Connect(end_point));
		print("socket2.Send() -> %i", socket2.Send(socket2_buffer, sizeof(socket2_buffer), std::bind(&demo_socket::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));

		// print("threads.Shutdown() -> %i", (threads.Shutdown(), true));
		print("threads.Join() -> %i", threads.Join());
	}

private:
	void on_accept(FIO::Socket& socket, FIO::Socket& client)
	{
		print("accepted connection from %s (error: 0x%08" PRIX32 ")", client.GetRemoteEndPoint().ToString().c_str(), socket.GetLastError());

		print("socket1_client.Associate() -> %i", client.Associate(threads));
		print("socket1_client.Receive() -> %i", client.Receive(socket1_buffer, sizeof(socket1_buffer), std::bind(&demo_socket::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)));
	}

	void on_send(FIO::Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent)
	{
		print("sent %zu bytes (error: 0x%08" PRIX32 ")", number_of_bytes_sent, socket.GetLastError());

		socket.Send(buffer, size, std::bind(&demo_socket::on_send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}
	void on_send_to(FIO::Socket& socket, const void* buffer, size_t size, size_t number_of_bytes_sent, const FIO::IPEndPoint& remote_end_point)
	{
		print("sent %zu bytes to %s (error: 0x%08" PRIX32 ")", number_of_bytes_sent, remote_end_point.ToString().c_str(), socket.GetLastError());

		socket.SendTo(buffer, size, remote_end_point, std::bind(&demo_socket::on_send_to, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
	}

	void on_receive(FIO::Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received)
	{
		print("received %zu bytes (error: 0x%08" PRIX32 ")", number_of_bytes_received, socket.GetLastError());

		socket.Receive(buffer, size, std::bind(&demo_socket::on_receive, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	}
	void on_receive_from(FIO::Socket& socket, void* buffer, size_t size, size_t number_of_bytes_received, const FIO::IPEndPoint& remote_end_point)
	{
		print("received %zu bytes from %s (error: 0x%08" PRIX32 ")", number_of_bytes_received, remote_end_point.ToString().c_str(), socket.GetLastError());

		socket.ReceiveFrom(buffer, size, std::bind(&demo_socket::on_receive_from, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
	}
};

int main(int argc, char* argv[])
{
	// demo_dns("www.google.com", FIO::ADDRESS_FAMILY_IP_V4).run();
	// demo_dns("www.google.com", FIO::ADDRESS_FAMILY_IP_V6).run();

	// demo_file("./demo.bin", FIO::FILE_MODE_READ, 4).run();
	// demo_file("./demo.bin", FIO::FILE_MODE_WRITE | FIO::FILE_MODE_TRUNCATE, 4).run();

	// demo_socket(FIO::SOCKET_TYPE_TCP, FIO::IPEndPoint::Loopback(9001), 4).run();
	// demo_socket(FIO::SOCKET_TYPE_UDP, FIO::IPEndPoint::Loopback(9001), 4).run();

	return 0;
}
