project(AppImage)
cmake_minimum_required(VERSION 3.5)

########## Configure Project #############
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/externals/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})

execute_process(
    COMMAND git describe --tags --always --abbrev=7
    OUTPUT_VARIABLE VERSION_NUMBER
)
string(STRIP ${VERSION_NUMBER} APPIMAGE_VERSION)

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH
  "Installation directory for header files")

set(DEF_INSTALL_CMAKE_DIR lib/cmake/${PROJECT_NAME})
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH 
    "Installation directory for CMake files")
  
include(cmake/cmake_make_absolute_paths.cmake)

################# SETUP TARGETS ######################

add_subdirectory(src)

############# Configure CMake Module #################

# Add all targets to the build-tree export set
export(TARGETS appimage
FILE "${PROJECT_BINARY_DIR}/AppImageTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE AppImage)

# Create the AppImageConfig.cmake and AppImageConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
 "${INSTALL_INCLUDE_DIR}")
# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
configure_file(cmake/AppImageConfig.cmake.in
"${PROJECT_BINARY_DIR}/AppImageConfig.cmake" @ONLY)
# ... for the install tree
set(CONF_INCLUDE_DIRS "${INSTALL_INCLUDE_DIR}")
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

################## TESTING #####################

enable_testing()
add_subdirectory(test)

########### Add uninstall target ###############

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)
    
ADD_CUSTOM_TARGET(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
)