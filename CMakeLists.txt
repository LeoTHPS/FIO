cmake_minimum_required(VERSION 3.25)

set(FAKE_LINUX         0)

set(CMAKE_C_STANDARD   17)
set(CMAKE_CXX_STANDARD 20)

project(FIO)
add_library(FIO STATIC
	FIO/File.cpp

	FIO/Timer.cpp

	FIO/IP.cpp
	FIO/DNS.cpp
	FIO/Socket.cpp

	FIO/Thread.cpp
	FIO/ThreadPool.cpp
)
target_include_directories(FIO PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(FAKE_LINUX)
	target_link_libraries(FIO PRIVATE uring)
	target_compile_definitions(FIO PUBLIC -DFIO_LINUX=1)
	target_compile_definitions(FIO PRIVATE -D_LARGEFILE64_SOURCE=1)
	target_include_directories(FIO PUBLIC $ENV{RASPBERRY_PI_INCLUDE} $ENV{RASPBERRY_PI_INCLUDE_ARM})
elseif(WIN32)
	target_sources(FIO PRIVATE FIO/WinSock2.cpp)
	target_link_libraries(FIO PRIVATE Ws2_32 Mswsock Iphlpapi Shlwapi)
	target_compile_definitions(FIO PUBLIC -DFIO_WIN32=1 -DWIN32_LEAN_AND_MEAN=1)
elseif(LINUX)
	target_link_libraries(FIO PRIVATE uring)
	target_compile_definitions(FIO PUBLIC -DFIO_LINUX=1)
	target_compile_definitions(FIO PRIVATE -D_LARGEFILE64_SOURCE=1)
endif()

# add_executable(demo demo.cpp)
# target_link_libraries(demo FIO)
