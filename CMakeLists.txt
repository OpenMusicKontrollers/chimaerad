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

set(LIBS -lm)

# libuv
pkg_search_module(UV REQUIRED libuv>=1.0.0)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# RtMidi
option(USE_RTMIDI "use RtMidi" ON)
if(USE_RTMIDI)
	pkg_search_module(RTMIDI REQUIRED librtmidi>=2.1)
	include_directories(${RTMIDI_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${RTMIDI_LDFLAGS})
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

# eina
pkg_search_module(EINA REQUIRED eina>=1.2)
include_directories(${EINA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EINA_LDFLAGS})

# eet
pkg_search_module(EET REQUIRED eet>=1.2)
include_directories(${EET_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EET_LDFLAGS})

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
install(TARGETS chimaerad DESTINATION bin)

# container
find_program(EET_BIN NAMES eet)
if(EET_BIN_NOTFOUND)
  message(SEND_ERROR "eet not found")
else()
  message(STATUS "eet found: " ${EET_BIN})
endif()

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/chimaerad.eet
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /											${PROJECT_SOURCE_DIR}/index.html						0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /content.html					${PROJECT_SOURCE_DIR}/content.html					0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /jquery-1.11.1.min.js	${PROJECT_SOURCE_DIR}/jquery-1.11.1.min.js	0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /chimaerad.js					${PROJECT_SOURCE_DIR}/chimaerad.js					0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /style.css							${PROJECT_SOURCE_DIR}/style.css							0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /favicon.png						${PROJECT_SOURCE_DIR}/favicon.png						0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /logo.png							${PROJECT_SOURCE_DIR}/logo.png							0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /dummy.lua							${PROJECT_SOURCE_DIR}/dummy.lua				    	0
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /Berenika-Bold.ttf			${PROJECT_SOURCE_DIR}/Berenika-Bold.ttf			1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /GenBasR.ttf						${PROJECT_SOURCE_DIR}/GenBasR.ttf			    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /GenBasI.ttf						${PROJECT_SOURCE_DIR}/GenBasI.ttf			    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /GenBasB.ttf						${PROJECT_SOURCE_DIR}/GenBasB.ttf			    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /GenBasBI.ttf					${PROJECT_SOURCE_DIR}/GenBasBI.ttf		    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /ace/ace.js						${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/ace.js			    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /ace/theme-clouds_midnight.js						${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/theme-clouds_midnight.js			    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /ace/worker-lua.js						${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/worker-lua.js			    	1
	COMMAND ${EET_BIN} ARGS -i ${PROJECT_BINARY_DIR}/chimaerad.eet /ace/mode-lua.js						${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/mode-lua.js			    	1
	DEPENDS
		${PROJECT_SOURCE_DIR}/index.html
		${PROJECT_SOURCE_DIR}/content.html
		${PROJECT_SOURCE_DIR}/jquery-1.11.1.min.js
		${PROJECT_SOURCE_DIR}/chimaerad.js
		${PROJECT_SOURCE_DIR}/style.css
		${PROJECT_SOURCE_DIR}/favicon.png
		${PROJECT_SOURCE_DIR}/logo.png
		${PROJECT_SOURCE_DIR}/dummy.lua
		${PROJECT_SOURCE_DIR}/Berenika-Bold.ttf
		${PROJECT_SOURCE_DIR}/GenBasR.ttf
		${PROJECT_SOURCE_DIR}/GenBasI.ttf
		${PROJECT_SOURCE_DIR}/GenBasB.ttf
		${PROJECT_SOURCE_DIR}/GenBasBI.ttf
		${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/ace.js
		${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/theme-clouds_midnight.js
		${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/worker-lua.js
		${PROJECT_SOURCE_DIR}/ace-builds/src-min-noconflict/mode-lua.js)
add_custom_target(container ALL DEPENDS ${PROJECT_BINARY_DIR}/chimaerad.eet)
install(FILES ${PROJECT_BINARY_DIR}/chimaerad.eet DESTINATION share/chimaerad)
