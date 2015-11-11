CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

include(ExternalProject)

option(USE_GNU_XORRISO "Use GNU xorriso instead of the one provided by libburnia" ON)

ExternalProject_Add(libattr
  URL "http://download.savannah.gnu.org/releases/attr/attr-2.4.47.src.tar.gz"
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
  INSTALL_COMMAND make install-lib install-dev
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(libacl
  DEPENDS libcwrap libattr
  URL "http://download.savannah.gnu.org/releases/acl/acl-2.2.52.src.tar.gz"
  CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/install/include
                    LDFLAGS=-L${CMAKE_CURRENT_SOURCE_DIR}/install/lib
                    ./configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
  BUILD_COMMAND CPPFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/install/include
                LDFLAGS=-L${CMAKE_CURRENT_SOURCE_DIR}/install/lib
                make
  INSTALL_COMMAND make install-lib
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
  BUILD_IN_SOURCE 1
)

if (USE_GNU_XORRISO)
  # xorriso provided by GNU xorriso

  ExternalProject_Add(xorriso
    DEPENDS libcwrap
    URL "https://www.gnu.org/software/xorriso/xorriso-1.4.0.tar.gz"
    CONFIGURE_COMMAND ./configure --disable-libacl --disable-libreadline --disable-libbz2
                                  --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
    BUILD_IN_SOURCE 1
  )
else (USE_GNU_XORRISO)
  # xorriso provided by libburnia

  ExternalProject_Add(libburn
    DEPENDS libcwrap
    #SVN_REPOSITORY "http://svn.libburnia-project.org/libburn/trunk"
    URL "http://files.libburnia-project.org/releases/libburn-1.4.0.tar.gz"
    CONFIGURE_COMMAND ./configure --disable-ldconfig-at-install --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
    BUILD_IN_SOURCE 1
  )

  ExternalProject_Add(libisofs
    DEPENDS libcwrap
    #HG_REPOSITORY "lp:~libburnia-team/libisofs/scdbackup"
    URL "http://files.libburnia-project.org/releases/libisofs-1.4.0.tar.gz"
    CONFIGURE_COMMAND ./configure --disable-ldconfig-at-install --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
    BUILD_IN_SOURCE 1
  )

  ExternalProject_Add(libisoburn
    DEPENDS libcwrap libisofs libburn libacl
    URL "http://files.libburnia-project.org/releases/libisoburn-1.4.0.tar.gz"
    CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_CURRENT_SOURCE_DIR}/install/include
                      LDFLAGS=-L${CMAKE_CURRENT_SOURCE_DIR}/install/lib
                      ./configure --disable-ldconfig-at-install
                                  --enable-static-libisofs --enable-static-libburn --enable-static-libacl
                                  --disable-libacl --disable-xattr --disable-libjte --disable-libcdio --disable-libreadline --disable-libedit --disable-launch-frontend
                                  --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
    BUILD_IN_SOURCE 1
  )

  ADD_CUSTOM_TARGET(xorriso DEPENDS libisoburn)
endif (USE_GNU_XORRISO)


#ExternalProject_Add(libglade
#  DEPENDS libcwrap
#  URL "http://ftp.acc.umu.se/pub/GNOME/sources/libglade/2.6/libglade-2.6.4.tar.bz2"
#  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
#  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
#  BUILD_IN_SOURCE 1
#)

#ExternalProject_Add(libvte
#  DEPENDS libcwrap
#  URL "http://ftp.gnome.org/pub/GNOME/sources/vte/0.43/vte-0.43.0.tar.xz"
#  CONFIGURE_COMMAND ./configure --disable-static --enable-introspection --with-gtk=2.0 --disable-gnome-pty-helper --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
#  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
#  BUILD_IN_SOURCE 1
#)


ExternalProject_Add(inotify-tools
  DEPENDS libcwrap
  URL "http://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz"
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(desktop-file-utils
  DEPENDS libcwrap
  URL "http://www.freedesktop.org/software/desktop-file-utils/releases/desktop-file-utils-0.22.tar.xz"
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(xdg-utils
  URL "http://portland.freedesktop.org/download/xdg-utils-1.1.1.tar.gz"
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
  PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/build
  BUILD_IN_SOURCE 1
)

