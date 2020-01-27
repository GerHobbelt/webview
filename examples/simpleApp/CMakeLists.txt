cmake_minimum_required(VERSION 2.8)

project(simpleApp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(simpleApp WIN32 MACOSX_BUNDLE simpleApp.cc)


#OSX
if(APPLE)
set_target_properties(simpleApp PROPERTIES
                      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
                      )

#WIN
elseif(WIN32)
target_compile_options(simpleApp PUBLIC /MT$<$<CONFIG:Debug>:d>)
target_link_options(simpleApp PUBLIC /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT$<$<CONFIG:Debug>:D>)

endif()

target_link_libraries(simpleApp PUBLIC webview)
