CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(AppImageKit C)

# Set required libraries.
set(LIBFUSE "fuse")
set(LIBPTHREAD "pthread")
set(LIBGLIB2 "glib-2.0")
set(LIBZ "z")

SET(REQUIRED_LIBRARIES ${LIBFUSE} ${LIBPTHREAD})
foreach(LIB ${REQUIRED_LIBRARIES})
  find_library(FOUND${LIB} ${LIB} PATHS "/lib64" "/usr/lib64")
  if (NOT FOUND${LIB})
    message(FATAL_ERROR "The required library '${LIB}' was not found. Please install it on your system first.")
  endif(NOT FOUND${LIB})
endforeach(LIB)

ADD_DEFINITIONS(-g -Os -D_FILE_OFFSET_BITS=64)

# Begin find glib

if (GLIB_PKG_FOUND)
    find_path(GLIB_INCLUDE_DIR  NAMES glib.h PATH_SUFFIXES glib-2.0
       PATHS
       ${GLIB_PKG_INCLUDE_DIRS}
       /usr/include/glib-2.0
       /usr/include
       /usr/local/include
    )
    find_path(GLIB_CONFIG_INCLUDE_DIR NAMES glibconfig.h PATHS ${GLIB_PKG_LIBDIR} PATH_SUFFIXES glib-2.0/include)

    find_library(GLIB_LIBRARIES NAMES glib-2.0
       PATHS
       ${GLIB_PKG_LIBRARY_DIRS}
       /usr/lib
       /usr/local/lib
    )

else (GLIB_PKG_FOUND)
    # Find Glib even if pkg-config is not working (eg. cross compiling to Windows)
    find_library(GLIB_LIBRARIES NAMES glib-2.0)
    string (REGEX REPLACE "/[^/]*$" "" GLIB_LIBRARIES_DIR ${GLIB_LIBRARIES})

    find_path(GLIB_INCLUDE_DIR NAMES glib.h PATH_SUFFIXES glib-2.0)
    find_path(GLIB_CONFIG_INCLUDE_DIR NAMES glibconfig.h PATHS ${GLIB_LIBRARIES_DIR} PATH_SUFFIXES glib-2.0/include)

endif (GLIB_PKG_FOUND)

if (GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GLIB_LIBRARIES)
    set(GLIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIR} ${GLIB_CONFIG_INCLUDE_DIR})
endif (GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GLIB_LIBRARIES)

if(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
   set(GLIB_FOUND TRUE CACHE INTERNAL "glib-2.0 found")
   message(STATUS "Found glib-2.0: ${GLIB_INCLUDE_DIR}, ${GLIB_LIBRARIES}")
else(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
   set(GLIB_FOUND FALSE CACHE INTERNAL "glib-2.0 found")
   message(STATUS "glib-2.0 not found.")
endif(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)

mark_as_advanced(GLIB_INCLUDE_DIR GLIB_CONFIG_INCLUDE_DIR GLIB_INCLUDE_DIRS GLIB_LIBRARIES)

# End find glib

INCLUDE_DIRECTORIES(. ${GLIB_INCLUDE_DIRS})

ADD_EXECUTABLE(AppRun AppRun.c)
add_custom_command(TARGET AppRun POST_BUILD COMMAND ${CMAKE_STRIP} AppRun)

ADD_LIBRARY(isofs isofs.c isofs.h)
TARGET_LINK_LIBRARIES(isofs ${LIBPTHREAD} ${LIBGLIB2} ${LIBZ}）

ADD_LIBRARY(fuseiso fuseiso.c isofs.h)
TARGET_LINK_LIBRARIES(fuseiso isofs ${LIBFUSE} ${LIBPTHREAD} ${LIBGLIB2}）

ADD_EXECUTABLE(runtime runtime.c)
TARGET_LINK_LIBRARIES(runtime fuseiso isofs ${LIBFUSE} ${LIBPTHREAD} ${LIBGLIB2} ${LIBZ})
add_custom_command(TARGET runtime POST_BUILD COMMAND ${CMAKE_STRIP} runtime)

SET(BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/binary-dependencies/bundle")
SET(PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}/AppImageAssistant.AppDir/package")

ADD_CUSTOM_TARGET(AppImageAssistant ALL DEPENDS runtime)
ADD_CUSTOM_COMMAND(TARGET AppImageAssistant
  COMMAND cp runtime ./AppImageAssistant.AppDir
  COMMAND ${BUNDLE}
  COMMAND rm ./AppImageAssistant || continue
  COMMAND ${PACKAGE} ./AppImageAssistant.AppDir ./AppImageAssistant
  VERBATIM
)

ADD_CUSTOM_TARGET(AppImageExtract ALL DEPENDS AppRun AppImageAssistant)
ADD_CUSTOM_COMMAND(TARGET AppImageExtract
  COMMAND cp AppRun ./AppImageExtract.AppDir
  COMMAND ${BUNDLE}
  COMMAND rm ./AppImageExtract || continue
  COMMAND ${PACKAGE} ./AppImageExtract.AppDir ./AppImageExtract
  VERBATIM
)

ADD_CUSTOM_TARGET(AppImageMonitor ALL DEPENDS AppRun AppImageAssistant)
ADD_CUSTOM_COMMAND(TARGET AppImageMonitor
  COMMAND cp AppRun ./AppImageMonitor.AppDir
  COMMAND ${BUNDLE}
  COMMAND rm ./AppImageMonitor || continue
  COMMAND ${PACKAGE} ./AppImageMonitor.AppDir ./AppImageMonitor
  VERBATIM
)

SET_DIRECTORY_PROPERTIES(PROPERTIES   
 ADDITIONAL_MAKE_CLEAN_FILES "AppImageAssistant;AppImageExtract;AppImageMonitor;./AppImageAssistant.AppDir/runtime;./AppImageAssistant.AppDir/usr/share/pyshared/vte.so;./AppImageAssistant.AppDir/usr/bin/unionfs-fuse;./AppImageAssistant.AppDir/usr/lib/;./AppImageExtract.AppDir/AppRun;./AppImageMonitor.AppDir/AppRun;./AppImageExtract.AppDir/usr/lib/;./AppImageMonitor.AppDir/usr/lib/;./AppImageAssistant.AppDir/usr/bin/xorriso;./AppImageExtract.AppDir/usr/bin/xorriso;./AppImageMonitor.AppDir/usr/bin/xorriso") 
