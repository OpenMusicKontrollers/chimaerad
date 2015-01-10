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

# libzip
pkg_search_module(ZIP REQUIRED libzip>=0.10)
include_directories(${ZIP_INCLUDE_DIRS})
set(LIBS ${LIBS} ${ZIP_LDFLAGS})

# libuv
pkg_search_module(UV REQUIRED libuv>=1.0.0)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# JACK
option(USE_JACK "use JACK" OFF)

# RtMidi
option(USE_SYSTEM_RTMIDI "use System RtMidi" OFF)
if(USE_SYSTEM_RTMIDI)
	pkg_search_module(RTMIDI REQUIRED librtmidi>=2.1)
	include_directories(${RTMIDI_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${RTMIDI_LDFLAGS})
else() # use embedded rtmidi
	add_library(rtmidi STATIC rtmidi/RtMidi.cpp)
	include_directories(${PROJECT_SOURCE_DIR}/rtmidi)

	if(WIN32)
		target_compile_definitions(rtmidi PUBLIC -D__WINDOWS_MM__)
	endif()

	if(${USE_JACK} AND UNIX) # for Apple and Linux
		pkg_search_module(JACK OPTIONAL jack)
		if(${JACK_FOUND})
			include_directories(${JACK_INCLUDE_DIRS})
			set(LIBS ${LIBS} ${JACK_LDFLAGS})
			target_compile_definitions(rtmidi PUBLIC -D__UNIX_JACK__)
		endif()
	endif()

	if(APPLE)
		find_library(CORE_MIDI CoreMIDI)
		find_library(CORE_FOUNDATION CoreFoundation)
		find_library(CORE_AUDIO CoreAudio)
		set(LIBS ${LIBS} ${CORE_MIDI} ${CORE_FOUNDATION} ${CORE_AUDIO})
		target_compile_definitions(rtmidi PUBLIC -D__MACOSX_CORE__)
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
option(USE_LUAJIT "use LuaJIT" OFF)
if(USE_LUAJIT)
	pkg_search_module(LUAJIT REQUIRED luajit>=2.0)
	include_directories(${LUAJIT_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUAJIT_LDFLAGS})
else()
	pkg_search_module(LUA REQUIRED lua>=5.1)
	include_directories(${LUA_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUA_LDFLAGS})
endif()

if(WIN32)
	set(BUILD_SHARED_LIBRARIES OFF)
	set(CMAKE_C_FLAGS "-std=gnu99")
	set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
	set(LIBS ${LIBS} -lwinmm -lws2_32 -lpsapi -liphlpapi -lavrt)
endif()

# chimaerad
add_executable(chimaerad
# osc
	libosc/osc.c
# osc_stream
	libosc_stream/osc_stream.c
	libosc_stream/osc_stream_pipe.c
	libosc_stream/osc_stream_slip.c
	libosc_stream/osc_stream_tcp.c
	libosc_stream/osc_stream_udp.c
	mod_osc.c
# cJSON
	cJSON/cJSON.c
	mod_json.c
# chimaerad
	chimaerad.c
	#chimaerad_host.c
	#chimaerad_discover.c
	#chimaerad_client.c
	#chimaerad_source.c
	#rtmidi_c.cpp
# rtmidi
	mod_rtmidi.cpp
# http
	http-parser/http_parser.c
	mod_http.c
# zip
	mod_zip.c
# tlsf
	tlsf-3.0/tlsf.c)
if(NOT USE_SYSTEM_RTMIDI)
	target_link_libraries(chimaerad rtmidi)
endif()
target_link_libraries(chimaerad ${LIBS})
install(TARGETS chimaerad DESTINATION bin)

find_program(ZIP_BIN NAMES zip)
if(ZIP_BIN_NOTFOUND)
  message(SEND_ERROR "zip not found")
else()
  message(STATUS "zip found: " ${ZIP_BIN})
endif(ZIP_BIN_NOTFOUND)

set(ZIP_SOURCES
	${PROJECT_SOURCE_DIR}/COPYING
	${PROJECT_SOURCE_DIR}/Berenika-Bold.ttf
	${PROJECT_SOURCE_DIR}/Berenika-Bold.woff
	${PROJECT_SOURCE_DIR}/Abel.woff
	${PROJECT_SOURCE_DIR}/Abel.woff2
	${PROJECT_SOURCE_DIR}/chimaerad.js
	${PROJECT_SOURCE_DIR}/content.html
	${PROJECT_SOURCE_DIR}/dummy.lua
	${PROJECT_SOURCE_DIR}/favicon.png
	${PROJECT_SOURCE_DIR}/index.html
	${PROJECT_SOURCE_DIR}/jquery-1.11.2.min.js
	${PROJECT_SOURCE_DIR}/logo.png
	${PROJECT_SOURCE_DIR}/style.css
	${PROJECT_SOURCE_DIR}/ace/ace.js
	${PROJECT_SOURCE_DIR}/ace/mode-lua.js
	${PROJECT_SOURCE_DIR}/ace/theme-clouds_midnight.js
	${PROJECT_SOURCE_DIR}/ace/worker-lua.js)

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/app.zip
	COMMAND ${ZIP_BIN} ARGS -j ${PROJECT_BINARY_DIR}/app.zip ${ZIP_SOURCES}
	MAIN_DEPENDENCY
		${ZIP_SOURCES})
add_custom_target(CONTAINER ALL DEPENDS ${PROJECT_BINARY_DIR}/app.zip)
#install(FILES ${PROJECT_BINARY_DIR}/app.zip DESTINATION ${DEST})
