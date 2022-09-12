cmake_minimum_required(VERSION 3.15)

set(TARGET kdata)

project(
	${TARGET} VERSION 1.0 
	DESCRIPTION "C API for Yandex Disk"
	HOMEPAGE_URL ""
	LANGUAGES C 
)


set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 17)

# Determine the location of the libraries to use based on the platform.
if(ANDROID)
	ADD_LIBRARY(curl STATIC IMPORTED)
	SET_TARGET_PROPERTIES(curl PROPERTIES IMPORTED_LOCATION ${CYANDEXDISK_SOURCE_DIR}/curl/android/${ANDROID_ABI}/libcurl.a)
	INCLUDE_DIRECTORIES(${CYANDEXDISK_SOURCE_DIR}/curl/android/include)
elseif(APPLE)
  if(IOS)
	ADD_LIBRARY(curl STATIC IMPORTED)
	SET_TARGET_PROPERTIES(curl PROPERTIES IMPORTED_LOCATION ${CYANDEXDISK_SOURCE_DIR}/curl/iOS/libcurl.a)
	INCLUDE_DIRECTORIES(${CYANDEXDISK_SOURCE_DIR}/curl/iOS/include)
  endif()
elseif(WIN32)
	ADD_LIBRARY(curl STATIC IMPORTED)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8) #64 bit	
		SET_TARGET_PROPERTIES(curl PROPERTIES IMPORTED_LOCATION ${CYANDEXDISK_SOURCE_DIR}/curl/windows/win64/lib/libcurl.a)
		INCLUDE_DIRECTORIES(${CYANDEXDISK_SOURCE_DIR}/windows/win64/include)
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 4) #32 bit
		SET_TARGET_PROPERTIES(curl PROPERTIES IMPORTED_LOCATION ${CYANDEXDISK_SOURCE_DIR}/curl/windows/win32/lib/libcurl.a)	
		INCLUDE_DIRECTORIES(${CYANDEXDISK_SOURCE_DIR}/windows/win32/include)
	endif()
endif()

add_library(${TARGET} STATIC cYandexDisk.c cJSON.c uuid4/uuid4.c)
target_link_libraries(${TARGET} curl)

if(${BUILD_TEST})
	add_executable(cYandexDisk_test test.c)
	target_link_libraries(cYandexDisk_test ${TARGET})
endif()
