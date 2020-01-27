cmake_minimum_required(VERSION 2.8)

project(simpleApp)


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(simpleApp WIN32 MACOSX_BUNDLE simpleApp.cc)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})


#OSX
if(APPLE)
message(STATUS "compiling  simpleApp  for OSX")
set_target_properties(simpleApp PROPERTIES
                      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
                      )

#WIN
elseif(WIN32)
message("compiling   simpleApp for WIN")
set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION dist)
include(InstallRequiredSystemLibraries)


# target_compile_options(simpleApp PUBLIC /MT$<$<CONFIG:Debug>:d>)
# target_link_options(simpleApp PUBLIC /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT$<$<CONFIG:Debug>:D>)
else()
message("compiling simpleApp for Unknown")
endif()

target_link_libraries(simpleApp PUBLIC webview)

install(TARGETS simpleApp
        RUNTIME DESTINATION dist
        BUNDLE DESTINATION  dist
        LIBRARY DESTINATION dist
        )

