cmake_minimum_required(VERSION 2.8)

project(webview)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
	# set(WEBVIEW_COMPILE_DEFS "-DWEBVIEW_COCOA=1")
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13)
	set(WEBVIEW_LIBS "-framework WebKit")
elseif(WIN32)
	# set(WEBVIEW_COMPILE_DEFS "-DWEBVIEW_EDGE=1")# no need
	set(WEBVIEW_LIBS ole32 comctl32 oleaut32 uuid)
	install(
		DIRECTORY "dll/x64/"
		DESTINATION .
		FILES_MATCHING PATTERN "*.dll"
	)
	install(
		DIRECTORY "script/Microsoft.Web.WebView2.0.8.355/build/native/x64"
		DESTINATION .
		FILES_MATCHING PATTERN "*.dll.lib"
	)
	set(WEBVIEW_COMPILE_INCS script/ script/Microsoft.Web.WebView2.0.8.355/build/native/include)
else()
	# set(WEBVIEW_COMPILE_DEFS "-DWEBVIEW_GTK=1")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)
	set(WEBVIEW_COMPILE_INCS ${GTK3_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})
	set(WEBVIEW_LIBS ${GTK3_LIBRARIES} ${WEBKIT2_LIBRARIES})
endif()

add_library(webview ${CMAKE_CURRENT_BINARY_DIR}/webview.cpp)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/webview.cpp "#define WEBVIEW_IMPLEMENTATION\n#include <webview.h>")
target_include_directories(webview PUBLIC ${PROJECT_SOURCE_DIR} ${WEBVIEW_COMPILE_INCS})
target_compile_definitions(webview PUBLIC ${WEBVIEW_COMPILE_DEFS})
target_compile_options(webview PRIVATE ${WEBVIEW_COMPILE_OPTS})
target_link_libraries(webview ${WEBVIEW_LIBS})


add_executable(main WIN32 MACOSX_BUNDLE main.cc)
set_target_properties(main PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_link_libraries(main PRIVATE webview ${CMAKE_THREAD_LIBS_INIT})

# add_executable(webview_test WIN32 MACOSX_BUNDLE webview_test.cc)
# set_target_properties(webview_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
# find_package(Threads)
# target_link_libraries(webview_test PRIVATE webview ${CMAKE_THREAD_LIBS_INIT})
# enable_testing ()
# add_test(NAME webview_test COMMAND webview_test)
