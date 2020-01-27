cmake_minimum_required(VERSION 2.8)

project(simpleApp)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(simpleApp WIN32 MACOSX_BUNDLE simpleApp.cc)


#OSX
set_target_properties(simpleApp PROPERTIES
                      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
                      )

#WIN
set_property(TARGET simpleApp PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


target_link_libraries(simpleApp PUBLIC webview)
