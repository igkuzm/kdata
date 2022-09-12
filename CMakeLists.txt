cmake_minimum_required(VERSION 3.15)

set(TARGET kdata)

project(
	${TARGET} VERSION 1.0 
	DESCRIPTION "C library for SQLite database with cloud sync"
	HOMEPAGE_URL ""
	LANGUAGES C 
)

set (CMAKE_C_STANDARD 11)

set(CYANDEXDISK_SOURCE_DIR ${CMAKE_SOURCE_DIR}/cYandexDisk)
add_subdirectory(${CYANDEXDISK_SOURCE_DIR})

add_library(${TARGET} STATIC
	kdata.c
	yd_daemon.c
	yd_update_from_cloud.c
	yd_upload.c
)
target_link_libraries(${TARGET} curl)

if(${BUILD_TEST})
	add_executable(cYandexDisk_test test.c)
	target_link_libraries(cYandexDisk_test ${TARGET})
endif()
