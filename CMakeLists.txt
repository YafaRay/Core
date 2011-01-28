project (yafaray)
cmake_minimum_required (VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_VERBOSE_MAKEFILE OFF)
   
####### Options ########
option(WITH_Freetype "Build font rendering with FreeType" ON)
option(WITH_OpenEXR "Build OpenEXR image I/O plugin" ON)
option(WITH_JPEG "Build JPEG image I/O plugin" ON)
option(WITH_PNG "Build PNG image I/O plugin" ON)
option(WITH_TIFF "Build TIFF image I/O plugin" ON)
option(WITH_XML_LOADER "Build XML Loader" ON)
option(WITH_QT "Enable Qt Gui build" ON)
option(WITH_YAF_PY_BINDINGS "Enable the YafaRay Python bindings" ON)
option(WITH_YAF_RUBY_BINDINGS "Enable the YafaRay Ruby bindings" OFF)
option(BUILDRELEASE "Enable release mode building of YafaRay" OFF)
option(DEBUG_BUILD "Enable debug build mode" OFF)
option(EMBED_FONT_QT "Embed font for QT GUI (usefull for some buggy QT installations)" OFF)

###### Packages and Definitions #########

# Load user configurations if available
include(CMakeConfig/UserConfig.txt OPTIONAL)
if(NOT YAF_USER_CONFIG)
	include(${CMAKE_BINARY_DIR}/UserConfig.txt OPTIONAL)
endif(NOT YAF_USER_CONFIG)
if(NOT YAF_USER_CONFIG)
	include(${CMAKE_SOURCE_DIR}/UserConfig.txt OPTIONAL)
endif(NOT YAF_USER_CONFIG)

if(YAF_USER_INCLUDE_DIRS)
	set(CMAKE_INCLUDE_PATH ${YAF_USER_INCLUDE_DIRS} CACHE STRING "Include search directories" FORCE)
endif(YAF_USER_INCLUDE_DIRS)

if(YAF_USER_LIBRARY_DIRS)
	set(CMAKE_LIBRARY_PATH ${YAF_USER_LIBRARY_DIRS} CACHE STRING "Library search directories" FORCE)
endif(YAF_USER_LIBRARY_DIRS)

if(YAF_USER_QT_BIN_DIR AND YAF_USER_SWIG_BIN_DIR)
	set(CMAKE_PROGRAM_PATH ${YAF_USER_QT_BIN_DIR} ${YAF_USER_SWIG_BIN_DIR} CACHE STRING "Executables search directories" FORCE)
endif(YAF_USER_QT_BIN_DIR AND YAF_USER_SWIG_BIN_DIR)

if(YAF_USER_EXTRA_LIBS)
	set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} ${YAF_USER_EXTRA_LIBS}" CACHE STRING "Standard C++ libraries" FORCE)
endif(YAF_USER_EXTRA_LIBS)

# find the required libs
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

if(LIBXML2_FOUND)
	add_definitions(-DHAVE_XML)
endif(LIBXML2_FOUND)

find_package(Threads REQUIRED)

# Find optional libs
if(CMAKE_HAVE_PTHREAD_H)
	add_definitions(-DHAVE_PTHREAD)
	message("Using Posix Threads: yes")
else(CMAKE_HAVE_PTHREAD_H)
	message("Using Posix Threads: no")
endif(CMAKE_HAVE_PTHREAD_H)

if(WITH_Freetype)
	find_package(Freetype REQUIRED)
	message("Using FreeType: yes")
	if(FREETYPE_FOUND)
		add_definitions(-DHAVE_FREETYPE)
	endif(FREETYPE_FOUND)
else(WITH_Freetype)
	message("Using FreeType: no")
endif(WITH_Freetype)

if(WITH_OpenEXR)
	find_package(OpenEXR REQUIRED)
	message("Using OpenEXR: yes")
else(WITH_OpenEXR)
	message("Using OpenEXR: no")
endif(WITH_OpenEXR)

if(WITH_JPEG)
	find_package(JPEG REQUIRED)
	message("Using JPEG: yes")
else(WITH_JPEG)
	message("Using JPEG: no")
endif(WITH_JPEG)

if(WITH_PNG)
	find_package(PNG REQUIRED)
	message("Using PNG: yes")
else(WITH_PNG)
	message("Using PNG: no")
endif(WITH_PNG)

if(WITH_TIFF)
	find_package(TIFF REQUIRED)
	message("Using TIFF: yes")
else(WITH_TIFF)
	message("Using TIFF: no")
endif(WITH_TIFF)

if(WITH_QT)
	find_package(Qt4 4.4.0 COMPONENTS QtCore QtGui REQUIRED)
	# Qt enabling header
	include(${QT_USE_FILE})
	message("Using Qt: yes")
else(WITH_QT)
	message("Using Qt: no")
endif(WITH_QT)

if(DEBUG_BUILD)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build mode" FORCE)
else(DEBUG_BUILD)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build mode" FORCE)
endif(DEBUG_BUILD)

set(YAF_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR})

if (USER_INSTALL_PREFIX)
	set(CMAKE_INSTALL_PREFIX ${USER_INSTALL_PREFIX})
endif (USER_INSTALL_PREFIX)

if(NOT WIN32)
	# installation directories

	if(NOT YAF_BIN_DIR)
		set(YAF_BIN_DIR bin)
	endif(NOT YAF_BIN_DIR)

	if(NOT YAF_LIB_DIR)
		set(YAF_LIB_DIR lib)
	endif(NOT YAF_LIB_DIR)

	# build flags
	set(CMAKE_CXX_FLAGS "-Wall" CACHE STRING "Default compiler flags" FORCE)
	set(YAF_CXX_FLAGS_RELEASE "-O3 -ffast-math")
	set(YAF_CXX_FLAGS_DEBUG "-ggdb")
	set(YAF_TARGET_TYPE LIBRARY)
else(NOT WIN32)
	# installation directories

	if(NOT YAF_BIN_DIR)
		set(YAF_BIN_DIR Win32Pak)
	endif(NOT YAF_BIN_DIR)

	set(YAF_LIB_DIR ${YAF_BIN_DIR})

	# build flags
	if(MINGW)
		set(CMAKE_CXX_FLAGS "-Wall" CACHE STRING "Default compiler flags" FORCE)
		set(YAF_CXX_FLAGS_RELEASE "-O3 -ffast-math")
		set(YAF_CXX_FLAGS_DEBUG "-ggdb")
	else(MINGW)
		set(CMAKE_CXX_FLAGS "/DWIN32 /D_WIN32 /D_USE_MATH_DEFINES /EHsc /MD /nologo" CACHE STRING "Default compiler flags" FORCE)
		set(YAF_CXX_FLAGS_RELEASE "/Ot /Ox")
		set(YAF_CXX_FLAGS_DEBUG "/Zi /GS /RTC1")
	endif(MINGW)
	set(YAF_TARGET_TYPE ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/linklibs RUNTIME)
endif(NOT WIN32)

if(YAF_USER_EXTRA_DEFS)
	add_definitions(${YAF_USER_EXTRA_DEFS})
endif(YAF_USER_EXTRA_DEFS)

if (BUILDRELEASE)
	add_definitions(-DRELEASE)
endif (BUILDRELEASE)

# Paths and flags configuration

if(NOT WIN32)
	if(NOT YAF_PLUGIN_DIR)
		set(YAF_PLUGIN_DIR ${YAF_LIB_DIR}/yafaray)
	endif(NOT YAF_PLUGIN_DIR)
else(NOT WIN32)
	if(NOT YAF_PLUGIN_DIR)
		set(YAF_PLUGIN_DIR ${YAF_LIB_DIR}/plugins)
	endif(NOT YAF_PLUGIN_DIR)
endif(NOT WIN32)

if (USER_RELFLAGS)
	set(CMAKE_CXX_FLAGS_RELEASE "${YAF_CXX_FLAGS_RELEASE} ${USER_RELFLAGS}" CACHE STRING "Release compiler flags" FORCE)
else (USER_RELFLAGS)
	set(CMAKE_CXX_FLAGS_RELEASE "${YAF_CXX_FLAGS_RELEASE}" CACHE STRING "Release compiler flags" FORCE)
endif (USER_RELFLAGS)

if (USER_DBGFLAGS)
	set(CMAKE_CXX_FLAGS_DEBUG "${YAF_CXX_FLAGS_DEBUG} ${USER_DEBUG}" CACHE STRING "Debug compiler flags" FORCE)
else (USER_DBGFLAGS)
	set(CMAKE_CXX_FLAGS_DEBUG "${YAF_CXX_FLAGS_DEBUG}" CACHE STRING "Debug compiler flags" FORCE)
endif (USER_DBGFLAGS)

# Adding subdirectories

add_subdirectory(src)

# Generating config headers

include(CMakeConfig/GenConfHeaders.txt)

