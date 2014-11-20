cmake_minimum_required(VERSION 2.8)

project(chimaerad)

# pkg-config
find_package(PkgConfig REQUIRED)
include(CheckIncludeFiles)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/tlsf-3.0)
include_directories(${PROJECT_SOURCE_DIR}/http-parser)
include_directories(${PROJECT_SOURCE_DIR}/libosc)
include_directories(${PROJECT_SOURCE_DIR}/libosc_stream)

# libuv
pkg_search_module(UV REQUIRED libuv>=1.0.0)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# RtMidi
option(USE_RTMIDI "use RtMidi" ON)
if(USE_RTMIDI)
	pkg_search_module(RTMIDI REQUIRED librtmidi>=2.1.0)
	include_directories(${RTMIDI_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${RTMIDI_LDFLAGS})
endif()

# lua
option(USE_LUAJIT "use LuaJIT" ON)
if(USE_LUAJIT)
	pkg_search_module(LUAJIT REQUIRED luajit)
	include_directories(${LUAJIT_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUAJIT_LDFLAGS})
else()
	pkg_search_module(LUA REQUIRED lua)
	include_directories(${LUA_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUA_LDFLAGS})
endif()

# eina
pkg_search_module(EINA REQUIRED eina>=1.11.2)
include_directories(${EINA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EINA_LDFLAGS})

# osc
add_library(osc libosc/osc.c)

# osc_stream
add_library(osc_stream
	libosc_stream/osc_stream.c
	libosc_stream/osc_stream_pipe.c
	libosc_stream/osc_stream_slip.c
	libosc_stream/osc_stream_tcp.c
	libosc_stream/osc_stream_udp.c)

# chimaerad
add_executable(chimaerad
	chimaerad.c
	chimaerad_http.c
	http-parser/http_parser.c
	tlsf-3.0/tlsf.c)
target_link_libraries(chimaerad osc osc_stream ${LIBS})
install(TARGETS chimaerad DESTINATION bin)
