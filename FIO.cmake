cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD   17)
set(CMAKE_CXX_STANDARD 20)

project(FIO)
add_library(FIO STATIC
	${CMAKE_CURRENT_LIST_DIR}/FIO/Path.cpp
	${CMAKE_CURRENT_LIST_DIR}/FIO/File.cpp
	${CMAKE_CURRENT_LIST_DIR}/FIO/Directory.cpp

	${CMAKE_CURRENT_LIST_DIR}/FIO/Timer.cpp

	${CMAKE_CURRENT_LIST_DIR}/FIO/IP.cpp
	${CMAKE_CURRENT_LIST_DIR}/FIO/DNS.cpp
	${CMAKE_CURRENT_LIST_DIR}/FIO/Socket.cpp

	${CMAKE_CURRENT_LIST_DIR}/FIO/Thread.cpp
	${CMAKE_CURRENT_LIST_DIR}/FIO/ThreadPool.cpp
)
target_include_directories(FIO PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(WIN32)
	target_sources(FIO PUBLIC ${CMAKE_CURRENT_LIST_DIR}/FIO/WinSock2.cpp)
	target_link_libraries(FIO PRIVATE Ws2_32 Mswsock Iphlpapi Shlwapi)
	target_compile_definitions(FIO PUBLIC -DFIO_WIN32=1 -DWIN32_LEAN_AND_MEAN=1)
elseif(LINUX)
	target_link_libraries(FIO PRIVATE uring)
	target_compile_definitions(FIO PUBLIC -DFIO_LINUX=1)
	target_compile_definitions(FIO PRIVATE -D_LARGEFILE64_SOURCE=1)
endif()
