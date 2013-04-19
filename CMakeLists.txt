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

ADD_DEFINITIONS(-g -O2 -D_FILE_OFFSET_BITS=64)

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

ADD_LIBRARY(fuseiso fuseiso.c)
ADD_LIBRARY(isofs isofs.c)

ADD_EXECUTABLE(runtime runtime.c)
TARGET_LINK_LIBRARIES(runtime fuseiso isofs ${LIBFUSE} ${LIBPTHREAD} ${LIBGLIB2} ${LIBZ})
add_custom_command(TARGET runtime POST_BUILD COMMAND ${CMAKE_STRIP} runtime)

ADD_CUSTOM_TARGET(AppImageAssistant ALL DEPENDS runtime)
ADD_CUSTOM_COMMAND(TARGET AppImageAssistant
  COMMAND cp runtime AppImageAssistant.AppDir/
  COMMAND ./binary-dependencies/bundle
  COMMAND ./AppImageAssistant.AppDir/package ./AppImageAssistant.AppDir ./AppImageAssistant
  VERBATIM
)

ADD_CUSTOM_TARGET(AppImageExtract ALL DEPENDS AppRun AppImageAssistant)
ADD_CUSTOM_COMMAND(TARGET AppImageExtract
  COMMAND cp AppRun ./AppImageExtract.AppDir
  COMMAND ./binary-dependencies/bundle
  COMMAND ./AppImageAssistant ./AppImageExtract.AppDir ./AppImageExtract
  VERBATIM
)

SET_DIRECTORY_PROPERTIES(PROPERTIES   
 ADDITIONAL_MAKE_CLEAN_FILES "AppImageAssistant;AppImageExtract;./AppImageAssistant.AppDir/runtime;./AppImageAssistant.AppDir/usr/share/pyshared/vte.so;./AppImageAssistant.AppDir/usr/lib/;./AppImageExtract.AppDir/AppRun;./AppImageExtract.AppDir/usr/lib/;./AppImageAssistant.AppDir/usr/bin/xorriso;./AppImageExtract.AppDir/usr/bin/xorriso") 
