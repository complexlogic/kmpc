set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
set(SOURCES "main.cpp")
qt_wrap_ui(SOURCES "mainwindow.ui")
if (UNIX)
  add_executable(${EXECUTABLE_TITLE} ${SOURCES})
  target_link_libraries(${EXECUTABLE_TITLE} 
    PkgConfig::FMT
    PkgConfig::SQLITE3
  )
  if (USE_QT6)
    target_link_libraries(${EXECUTABLE_TITLE} Qt6::Widgets Qt6::Gui)
  else ()
    target_link_libraries(${EXECUTABLE_TITLE} Qt5::Widgets Qt5::Gui)
  endif ()
elseif (WIN32)
  set(APP_ICON_RESOURCE_WINDOWS "${PROJECT_SOURCE_DIR}/config/${EXECUTABLE_TITLE}.rc")
  set(MANIFEST_FILE "${PROJECT_BINARY_DIR}/${EXECUTABLE_TITLE}.manifest")
  add_executable(${EXECUTABLE_TITLE} WIN32 ${SOURCES} ${MANIFEST_FILE} ${APP_ICON_RESOURCE_WINDOWS})
  target_compile_options(${EXECUTABLE_TITLE} PUBLIC "/Zc:preprocessor")
  target_include_directories(${EXECUTABLE_TITLE} PUBLIC
    ${FMT_INCLUDE_DIR}
    ${SQLITE3_INCLUDE_DIR}
  )
  target_link_libraries(${EXECUTABLE_TITLE}
    ${FMT}
    ${SQLITE3}
    Qt::Gui
    Qt::Widgets
  )

  # Copy the Qt configuration libraries
  add_custom_command(TARGET ${EXECUTABLE_TITLE} 
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/Qt6/plugins/platforms" "${PROJECT_BINARY_DIR}/$<CONFIG>/platforms"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/Qt6/plugins/styles" "${PROJECT_BINARY_DIR}/$<CONFIG>/styles"
  )
endif ()
