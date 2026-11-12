cmake_minimum_required(VERSION 3.24)
project(FileTraceStudioSDK VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanFileTraceStudio STATIC
  "sdk/SDK/FileTrace.cpp"
)
add_library(FileTraceStudioSDK::sdk ALIAS TitanFileTraceStudio)
set_target_properties(TitanFileTraceStudio PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanFileTraceStudio PUBLIC cxx_std_17)
target_compile_definitions(TitanFileTraceStudio PUBLIC )
target_compile_definitions(TitanFileTraceStudio PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanFileTraceStudio PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanFileTraceStudio PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanFileTraceStudio PUBLIC comdlg32 fltlib gdi32 shell32 user32)
install(TARGETS TitanFileTraceStudio EXPORT FileTraceStudioSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/file-trace-studio)
install(EXPORT FileTraceStudioSDKTargets NAMESPACE FileTraceStudioSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FileTraceStudioSDK)
configure_file("FileTraceStudioSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/FileTraceStudioSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/FileTraceStudioSDKConfigVersion.cmake" VERSION 0.1.0 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/FileTraceStudioSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/FileTraceStudioSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FileTraceStudioSDK)
