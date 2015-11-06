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

set(APP_DIR share/chimaerad)
set(LIBS -lm)

set(CMAKE_C_FLAGS "-std=gnu11 -Wextra -Wno-unused-parameter -ffast-math -fvisibility=hidden ${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "-Wshadow -Wimplicit-function-declaration -Wmissing-prototypes -Wstrict-prototypes ${CMAKE_C_FLAGS}")

set(CMAKE_CXX_FLAGS "-std=gnu++11 -Wextra -ffast-math -fvisibility=hidden ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wshadow -Wredundant-decls -Weffc++ ${CMAKE_CXX_FLAGS}")
	
if(WIN32)
	set(BUILD_SHARED_LIBRARIES OFF)
	set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
	set(LIBS ${LIBS} "-lwinmm -lws2_32 -lpsapi -liphlpapi -lavrt -Wl,-Bstatic -lstdc++ -lwinpthread")
elseif(APPLE)
	if(CMAKE_SIZEOF_VOID_P MATCHES "8")
		set(CMAKE_EXE_LINKER_FLAGS "-pagezero_size 10000 -image_base 100000000")
	endif()
endif()

# libzip
pkg_search_module(ZIP REQUIRED libzip>=0.10)
include_directories(${ZIP_INCLUDE_DIRS})
set(LIBS ${LIBS} ${ZIP_LDFLAGS})

# libuv
pkg_search_module(UV REQUIRED libuv>=1.0.0)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# lua
pkg_search_module(LUA REQUIRED lua>=5.1 lua5.3 lua5.2 lua5.1)
include_directories(${LUA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LUA_LDFLAGS})

if(WIN32)
	find_library(DNS_SD dns_sd)
	set(LIBS ${LIBS} ${DNS_SD})
elseif(APPLE)
	message(STATUS "using dns_sd")
else()
	pkg_search_module(DNS_SD REQUIRED avahi-compat-libdns_sd>=0.6)
	include_directories(${DNS_SD_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${DNS_SD_LDFLAGS})
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
	mod_osc_common.c
	mod_osc_stream.c
# cJSON
	cJSON/cJSON.c
	mod_json.c
# chimaerad
	chimaerad.c
# http
	http-parser/http_parser.c
	mod_http.c
# zip
	mod_zip.c
# iface
	mod_iface.c
# dns_sd
	mod_dns_sd.c
# tlsf
	tlsf-3.0/tlsf.c)
target_link_libraries(chimaerad ${LIBS})
target_compile_definitions(chimaerad PUBLIC -DAPP_DIR=${APP_DIR})
install(TARGETS chimaerad DESTINATION bin)

find_program(ZIP_BIN NAMES zip)
if(ZIP_BIN_NOTFOUND)
  message(SEND_ERROR "zip not found")
else()
  message(STATUS "zip found: " ${ZIP_BIN})
endif(ZIP_BIN_NOTFOUND)

set(ZIP_SOURCES
	index.html
	main.html
	device.html
	style.css

	chimaerad.js
	angular.js
	angular-route.js

	main.lua
	class.lua
	osc_responder.lua
	rest_responder.lua
	httpd.lua
	dns_sd.lua
	
	font/Berenika-Bold.ttf
	font/Berenika-Bold.woff
	font/Abel.woff
	font/Abel.woff2

	img/favicon.png
	img/logo.png)

set(ZIP_DEPENDS "")
foreach(f ${ZIP_SOURCES})
	list(APPEND ZIP_DEPENDS app/${f})
endforeach()

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/app.zip
	COMMAND ${ZIP_BIN} ARGS -0 -p ${PROJECT_BINARY_DIR}/app.zip ${ZIP_SOURCES}
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/app
	DEPENDS ${ZIP_DEPENDS})
add_custom_target(CONTAINER ALL DEPENDS ${PROJECT_BINARY_DIR}/app.zip)
install(FILES ${PROJECT_BINARY_DIR}/app.zip DESTINATION ${APP_DIR})
