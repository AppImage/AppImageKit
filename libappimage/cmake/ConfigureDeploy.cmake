

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH
  "Installation directory for header files")
set(INSTALL_CMAKE_DIR lib/CMake/AppImage CACHE PATH
  "Installation directory for CMake files")

install(TARGETS libappimage
    EXPORT AppImageTargets
    ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDE_DIR}
)

 
# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

 
# Add all targets to the build-tree export set
export(TARGETS libappimage
FILE "${PROJECT_BINARY_DIR}/AppImageTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE AppImage)

# Create the AppImageConfig.cmake and AppImageConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
 "${INSTALL_INCLUDE_DIR}")

set(CONF_INCLUDE_DIRS "\${APPIMAGE_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(cmake/AppImageConfig.cmake.in
"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/AppImageConfig.cmake" @ONLY)
# ... for both
configure_file(cmake/AppImageConfigVersion.cmake.in
"${PROJECT_BINARY_DIR}/AppImageConfigVersion.cmake" @ONLY)

# Install the AppImageConfig.cmake and AppImageConfigVersion.cmake
install(FILES
"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/AppImageConfig.cmake"
"${PROJECT_BINARY_DIR}/AppImageConfigVersion.cmake"
DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT AppImageTargets DESTINATION
"${INSTALL_CMAKE_DIR}" COMPONENT dev)