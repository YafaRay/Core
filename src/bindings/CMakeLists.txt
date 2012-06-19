#check for swig if needed

IF (WITH_YAF_PY_BINDINGS OR WITH_YAF_RUBY_BINDINGS)
	
	FIND_PACKAGE(YafSWIG REQUIRED)
	
	IF(YAF_PY_VERSION GREATER 3.1)
		SET(YAF_SWIG_MIN_VERSION 2.0.4)
	ELSE(YAF_PY_VERSION GREATER 3.1)
		SET(YAF_SWIG_MIN_VERSION 1.3.36)
	ENDIF(YAF_PY_VERSION GREATER 3.1)
	
	IF(NOT SWIG_FOUND)
		MESSAGE("Swig not found, please install it to build script bindings")
		SET(WITH_YAF_PY_BINDINGS OFF)
		SET(WITH_YAF_RUBY_BINDINGS OFF)
	ELSE(NOT SWIG_FOUND)
		IF(SWIG_VERSION VERSION_LESS ${YAF_SWIG_MIN_VERSION})
			IF(YAF_PY_VERSION GREATER 3.1)
				message("Swig version < ${YAF_SWIG_MIN_VERSION}, to build with Python 3.2 you need an adecuately patched version of swig. Please set the path to your patched swig version in your UserConfig.txt file on the YAF_USER_SWIG_BIN_DIR variable. The patches required can be found on the SupportFiles folder on yafaray's source path.'")
			ELSE(YAF_PY_VERSION GREATER 3.1)
				message("Swig version < ${YAF_SWIG_MIN_VERSION}, please update or bindings can't be built")
			ENDIF(YAF_PY_VERSION GREATER 3.1)
			set(WITH_YAF_PY_BINDINGS OFF)
			set(WITH_YAF_RUBY_BINDINGS OFF)
		ENDIF(SWIG_VERSION VERSION_LESS ${YAF_SWIG_MIN_VERSION})
	ENDIF(NOT SWIG_FOUND)
	
	INCLUDE(${SWIG_USE_FILE})
ENDIF (WITH_YAF_PY_BINDINGS OR WITH_YAF_RUBY_BINDINGS)

# Python bindings 

IF (WITH_YAF_PY_BINDINGS)
	
	IF(YAF_PY_VERSION)
		set(REQUIRED_PYTHON_VERSION ${YAF_PY_VERSION})
	ELSE(YAF_PY_VERSION)
		set(REQUIRED_PYTHON_VERSION 2.6)
	endif(YAF_PY_VERSION)
	
	MESSAGE("Searching for Python ${REQUIRED_PYTHON_VERSION}")
	find_package(YafPythonLibs REQUIRED)
	
	if (NOT PYTHON_LIBRARIES)
		message("Python wasn't found, please install it to build python bindings")
		set(WITH_YAF_PY_BINDINGS OFF)
	else (NOT PYTHON_LIBRARIES)
		message("Python ${REQUIRED_PYTHON_VERSION} found at: ${PYTHON_LIBRARIES}")
	endif (NOT PYTHON_LIBRARIES)
	
endif (WITH_YAF_PY_BINDINGS)

# Ruby bindings

if (WITH_YAF_RUBY_BINDINGS)
	
	find_package(Ruby 1.8 REQUIRED)
	
	message("Ruby version found: ${RUBY_VERSION}")
	message("Ruby headers at: ${RUBY_INCLUDE_DIR}")
	message("Ruby library at: ${RUBY_LIBRARY}")

	if (NOT SWIG_FOUND OR NOT RUBY_FOUND)
		set(WITH_YAF_RUBY_BINDINGS OFF)
	endif (NOT SWIG_FOUND OR NOT RUBY_FOUND)
	
endif (WITH_YAF_RUBY_BINDINGS)


if (WITH_YAF_PY_BINDINGS)
	
	if(NOT WIN32)

		if(NOT YAF_BINDINGS_PY_DIR)
			set(YAF_BINDINGS_PY_DIR ${CMAKE_SOURCE_DIR}/bindings/python)
		endif(NOT YAF_BINDINGS_PY_DIR)
		
	else(NOT WIN32)

		if(NOT YAF_BINDINGS_PY_DIR)
			set(YAF_BINDINGS_PY_DIR ${YAF_BIN_DIR}/python)
		endif(NOT YAF_BINDINGS_PY_DIR)

	endif(NOT WIN32)
	
	message("Python header path: ${PYTHON_INCLUDE_DIR}")
	include_directories(${YAF_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIR})
	include(${SWIG_USE_FILE})

	if(YAF_PY_VERSION GREATER 3)
		set(CMAKE_SWIG_FLAGS -c++ -py3)
	else(YAF_PY_VERSION GREATER 3)
		set(CMAKE_SWIG_FLAGS -c++)
	endif(YAF_PY_VERSION GREATER 3)

	set_source_files_properties(yafrayinterface.i PROPERTIES CPLUSPLUS ON)

	if(WITH_QT)
		set_source_files_properties(yafqt.i PROPERTIES CPLUSPLUS ON)
	endif(WITH_QT)

	SWIG_ADD_MODULE(yafrayinterface PYTHON yafrayinterface.i)

	if(APPLE) # use dynamic lookup for helping dlopen to find symbols properly - Jens
		SET_TARGET_PROPERTIES(${SWIG_MODULE_yafrayinterface_REAL_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
	endif(APPLE)
	if(WITH_QT)
		SWIG_ADD_MODULE(yafqt PYTHON yafqt.i)
	if(APPLE) # use dynamic lookup for helping dlopen to find symbols properly - Jens
		SET_TARGET_PROPERTIES(${SWIG_MODULE_yafqt_REAL_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
	endif(APPLE)

	endif(WITH_QT)

	SWIG_LINK_LIBRARIES(yafrayinterface yafaraycore yafarayplugin ${PYTHON_LIBRARIES})

	if(WITH_QT)
		SWIG_LINK_LIBRARIES(yafqt yafarayqt ${PYTHON_LIBRARIES})
	endif(WITH_QT)

if(APPLE) # set rpath - Jens
	add_custom_command(TARGET ${SWIG_MODULE_yafrayinterface_REAL_NAME} POST_BUILD COMMAND install_name_tool -add_rpath @loader_path/ _yafrayinterface.so)
endif(APPLE)

	install(TARGETS ${SWIG_MODULE_yafrayinterface_REAL_NAME} DESTINATION ${YAF_BINDINGS_PY_DIR})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/yafrayinterface.py DESTINATION ${YAF_BINDINGS_PY_DIR})

	if(WITH_QT)
		install(TARGETS ${SWIG_MODULE_yafqt_REAL_NAME} DESTINATION ${YAF_BINDINGS_PY_DIR})
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/yafqt.py DESTINATION ${YAF_BINDINGS_PY_DIR})
	endif(WITH_QT)

endif (WITH_YAF_PY_BINDINGS)

if (WITH_YAF_RUBY_BINDINGS)
	
	if(NOT WIN32)

		if(NOT YAF_BINDINGS_RUBY_DIR)
			set(YAF_BINDINGS_RUBY_DIR ${CMAKE_SOURCE_DIR}/bindings/ruby)
		endif(NOT YAF_BINDINGS_RUBY_DIR)
		
	else(NOT WIN32)

		if(NOT YAF_BINDINGS_RUBY_DIR)
			set(YAF_BINDINGS_RUBY_DIR ${YAF_BIN_DIR}/ruby)
		endif(NOT YAF_BINDINGS_RUBY_DIR)

	endif(NOT WIN32)

	include_directories(${YAF_INCLUDE_DIRS} ${RUBY_INCLUDE_DIR})
	include(${SWIG_USE_FILE})

	set(CMAKE_SWIG_FLAGS -c++ -w-801)

	set_source_files_properties(yafrayinterface.i PROPERTIES CPLUSPLUS ON)
	SWIG_ADD_MODULE(yafrayinterface RUBY yafrayinterface.i)
	SWIG_LINK_LIBRARIES(yafrayinterface yafaraycore yafarayplugin ${RUBY_LIBRARY})
	install(TARGETS ${SWIG_MODULE_yafrayinterface_REAL_NAME} DESTINATION ${YAF_BINDINGS_RUBY_DIR})

	if(WITH_QT)
		set_source_files_properties(yafqt.i PROPERTIES CPLUSPLUS ON)
		SWIG_ADD_MODULE(yafqt RUBY yafqt.i)
		SWIG_LINK_LIBRARIES(yafqt yafarayqt ${RUBY_LIBRARY})
		install(TARGETS ${SWIG_MODULE_yafqt_REAL_NAME} DESTINATION ${YAF_BINDINGS_RUBY_DIR})
	endif(WITH_QT)

endif (WITH_YAF_RUBY_BINDINGS)

