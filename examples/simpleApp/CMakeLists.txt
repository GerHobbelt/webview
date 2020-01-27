cmake_minimum_required(VERSION 2.8)

project(simpleApp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(simpleApp WIN32 MACOSX_BUNDLE simpleApp.cc)


#OSX
set_target_properties(simpleApp PROPERTIES
                      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
                      )

#WIN

target_compile_options(simpleApp PUBLIC/PRIVATE /MT)
target_link_options(simpleApp PUBLIC/PRIVATE /INCREMENTAL:NO /NODEFAULTLIB:MSVCRT)

target_link_libraries(simpleApp PUBLIC webview)
