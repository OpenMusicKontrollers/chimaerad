cmake_minimum_required(VERSION 2.8)

project(chimaerad)

# pkg-config
find_package(PkgConfig REQUIRED)
include(CheckIncludeFiles)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/http-parser)
include_directories(${PROJECT_SOURCE_DIR}/libosc)
include_directories(${PROJECT_SOURCE_DIR}/libosc_stream)
include_directories(${PROJECT_SOURCE_DIR}/cJSON)
include_directories(${PROJECT_SOURCE_DIR}/inlist)
include_directories(${PROJECT_SOURCE_DIR}/varchunk)
include_directories(${PROJECT_SOURCE_DIR}/lua-5.3.1)

set(APP_DIR share/chimaerad)
set(LIBS ${LIBS} m)

set(CMAKE_C_FLAGS "-std=gnu11 -Wextra -Wno-unused-parameter -ffast-math -fvisibility=hidden ${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "-Wshadow -Wimplicit-function-declaration -Wmissing-prototypes -Wstrict-prototypes ${CMAKE_C_FLAGS}")

set(CMAKE_CXX_FLAGS "-std=gnu++11 -Wextra -ffast-math -fvisibility=hidden ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wshadow -Wredundant-decls -Weffc++ ${CMAKE_CXX_FLAGS}")

if(WIN32)
	set(BUILD_SHARED_LIBRARIES OFF)
	set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
	set(LIBS ${LIBS} "-lwinmm -lws2_32 -lpsapi -liphlpapi -lavrt -Wl,-Bstatic -lstdc++ -lwinpthread -luserenv")
endif()

# zlib
pkg_search_module(Z OPTIONAL zlib>=1.2.0) # optional for APPLE
include_directories(${Z_INCLUDE_DIRS})
if(DEFINED STATIC_Z)
	set(LIBS ${STATIC_Z} ${LIBS})
else()
	set(LIBS ${LIBS} ${Z_LDFLAGS})
endif()

# libzip
pkg_search_module(ZIP REQUIRED libzip>=1.1.0)
include_directories(${ZIP_INCLUDE_DIRS})
if(DEFINED STATIC_ZIP)
	set(LIBS ${STATIC_ZIP} ${LIBS})
else()
	set(LIBS ${LIBS} ${ZIP_LDFLAGS})
endif()

# libuv
pkg_search_module(UV REQUIRED libuv>=1.3.0)
include_directories(${UV_INCLUDE_DIRS})
if(DEFINED STATIC_UV)
	set(LIBS ${STATIC_UV} ${LIBS})
else()
	set(LIBS ${LIBS} ${UV_LDFLAGS})
endif()

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

add_library(lua OBJECT
	lua-5.3.1/lapi.c
	lua-5.3.1/lcode.c
	lua-5.3.1/lctype.c
	lua-5.3.1/ldebug.c
	lua-5.3.1/ldo.c
	lua-5.3.1/ldump.c
	lua-5.3.1/lfunc.c
	lua-5.3.1/lgc.c
	lua-5.3.1/llex.c
	lua-5.3.1/lmem.c
	lua-5.3.1/lobject.c
	lua-5.3.1/lopcodes.c
	lua-5.3.1/lparser.c
	lua-5.3.1/lstate.c
	lua-5.3.1/lstring.c
	lua-5.3.1/ltable.c
	lua-5.3.1/ltm.c
	lua-5.3.1/lundump.c
	lua-5.3.1/lvm.c
	lua-5.3.1/lzio.c

	lua-5.3.1/lbitlib.c
	lua-5.3.1/ldblib.c
	lua-5.3.1/liolib.c
	lua-5.3.1/loslib.c
	lua-5.3.1/loadlib.c
	lua-5.3.1/linit.c
	lua-5.3.1/lbaselib.c
	lua-5.3.1/lauxlib.c
	lua-5.3.1/lcorolib.c
	lua-5.3.1/lmathlib.c
	lua-5.3.1/lstrlib.c
	lua-5.3.1/ltablib.c
	lua-5.3.1/lutf8lib.c)
set_target_properties(lua PROPERTIES POSITION_INDEPENDENT_CODE true) # -fPIC
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_compile_definitions(lua PUBLIC -DLUA_USE_LINUX)
elseif(WIN32)
	target_compile_definitions(lua PUBLIC)
elseif(APPLE)
	target_compile_definitions(lua PUBLIC -DLUA_USE_MACOSX)
endif()

add_library(cjson OBJECT
	cJSON/cJSON.c)

add_library(http_parser OBJECT
	http-parser/http_parser.c)

# chimaerad
add_executable(chimaerad
# OSC
	mod_osc_common.c
	mod_osc_stream.c
# cJSON
	mod_json.c
# chimaerad
	chimaerad.c
# http
	mod_http.c
# zip
	mod_zip.c
# iface
	mod_iface.c
# dns_sd
	mod_dns_sd.c
# http-parser
	$<TARGET_OBJECTS:http_parser>
# cJSON
	$<TARGET_OBJECTS:cjson>
# Lua
	$<TARGET_OBJECTS:lua>)

target_link_libraries(chimaerad ${LIBS})
target_compile_definitions(chimaerad PUBLIC -DAPP_DIR=${APP_DIR})
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_compile_definitions(chimaerad PUBLIC -DLUA_USE_LINUX)
elseif(WIN32)
	target_compile_definitions(chimaerad lua PUBLIC)
elseif(APPLE)
	target_compile_definitions(chimaerad PUBLIC -DLUA_USE_MACOSX)
endif()
install(TARGETS chimaerad DESTINATION bin)

find_program(ZIP_BIN NAMES zip)
if(ZIP_BIN_NOTFOUND)
  message(SEND_ERROR "zip not found")
else()
  message(STATUS "zip found: " ${ZIP_BIN})
endif(ZIP_BIN_NOTFOUND)

set(ZIP_SOURCES
	index.html
	interfaces.html
	device.html
	calibration.html

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

install(PROGRAMS ${PROJECT_SOURCE_DIR}/chimaerad.app DESTINATION bin)
