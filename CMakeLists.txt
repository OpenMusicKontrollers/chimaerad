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
include_directories(${PROJECT_SOURCE_DIR}/cJSON)
include_directories(${PROJECT_SOURCE_DIR}/inlist)

set(LIBS -lm)

# libuv
pkg_search_module(UV REQUIRED libuv>=1.0.0)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# RtMidi
option(USE_SYSTEM_RTMIDI "use System RtMidi" ON)
if(USE_SYSTEM_RTMIDI)
	pkg_search_module(RTMIDI REQUIRED librtmidi>=2.1)
	include_directories(${RTMIDI_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${RTMIDI_LDFLAGS})
else() # use embedded rtmidi
	add_library(rtmidi rtmidi/RtMidi.cpp)
	include_directories(${PROJECT_SOURCE_DIR}/rtmidi)

	if(WIN32)
		# what?
	endif()

	if(APPLE)
		target_compile_definitions(rtmidi PUBLIC -D__MACOSX_CORE__ -framework CoreMIDI -framework CoreAudio -framework CoreFoundation)
	endif()

	if(UNIX) # for Apple and Linux
		pkg_search_module(JACK OPTIONAL jack)
		if(${JACK_FOUND})
			include_directories(${JACK_INCLUDE_DIRS})
			set(LIBS ${LIBS} ${JACK_LDFLAGS})
			target_compile_definitions(rtmidi PUBLIC -D__UNIX_JACK__)
		endif()
	endif()

	if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		pkg_search_module(ALSA OPTIONAL alsa)
		if(${ALSA_FOUND})
			include_directories(${ALSA_INCLUDE_DIRS})
			set(LIBS ${LIBS} ${ALSA_LDFLAGS})
			target_compile_definitions(rtmidi PUBLIC -D__LINUX_ALSA__)
		endif()
	endif()
endif()

# lua
option(USE_LUAJIT "use LuaJIT" ON)
if(USE_LUAJIT)
	pkg_search_module(LUAJIT REQUIRED luajit>=2.0)
	include_directories(${LUAJIT_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUAJIT_LDFLAGS})
else()
	pkg_search_module(LUA REQUIRED lua>=5.1)
	include_directories(${LUA_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUA_LDFLAGS})
endif()

# osc_stream
add_library(osc_stream
	libosc/osc.c
	libosc_stream/osc_stream.c
	libosc_stream/osc_stream_pipe.c
	libosc_stream/osc_stream_slip.c
	libosc_stream/osc_stream_tcp.c
	libosc_stream/osc_stream_udp.c)

# cJSON
add_library(cjson
	cJSON/cJSON.c)

# chimaerad
add_executable(chimaerad
	chimaerad.c
	chimaerad_host.c
	chimaerad_discover.c
	chimaerad_client.c
	chimaerad_source.c
	rtmidi_c.cpp
	http-parser/http_parser.c
	tlsf-3.0/tlsf.c)
target_link_libraries(chimaerad osc_stream cjson ${LIBS})
if(NOT USE_SYSTEM_RTMIDI)
	target_link_libraries(chimaerad rtmidi)
endif()
install(TARGETS chimaerad DESTINATION bin)
