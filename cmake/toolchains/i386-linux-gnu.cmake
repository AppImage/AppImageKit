# this toolchain file works for cross compiling on Ubuntu when the following prerequisites are given:
# - all dependencies that would be needed for a normal build must be installed as i386 versions
# - building XZ/liblzma doesn't work yet, so one has to install liblzma-dev:i386 and set -DUSE_SYSTEM_XZ=ON
# - building GTest doesn't work yet, so one has to install libgtest-dev:i386 and set -DUSE_SYSTEM_GTEST=ON
# - building libarchive doesn't work yet, so one has to install liblzma-dev:i386 and set -DUSE_SYSTEM_LIBARCHIVE=ON (TODO: link system libarchive statically like liblzma)
# some of the packets interfere with their x86_64 version (e.g., libfuse-dev:i386, libglib2-dev:i386), so building on a
# normal system will most likely not work, but on systems like Travis it should work fine

set(CMAKE_SYSTEM_NAME Linux CACHE STRING "" FORCE)
set(CMAKE_SYSTEM_PROCESSOR i386 CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS "-m32" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-m32" CACHE STRING "" FORCE)

# CMAKE_SHARED_LINKER_FLAGS, CMAKE_STATIC_LINKER_FLAGS etc. must not be set, but CMAKE_EXE_LINKER_FLAGS is necessary
set(CMAKE_EXE_LINKER_FLAGS "-m32" CACHE STRING "" FORCE)

set(DEPENDENCIES_CFLAGS "-m32" CACHE STRING "" FORCE)
set(DEPENDENCIES_CPPFLAGS "-m32" CACHE STRING "" FORCE)
set(DEPENDENCIES_LDFLAGS "-m32" CACHE STRING "" FORCE)

# host = target system
# build = build system
# both must be specified
set(EXTRA_CONFIGURE_FLAGS "--host=i686-pc-linux-gnu" "--build=x86_64-pc-linux-gnu" CACHE STRING "" FORCE)

# may help with some rare issues
set(CMAKE_PREFIX_PATH /usr/lib/i386-linux-gnu CACHE STRING "" FORCE)

# makes sure that at least on Ubuntu pkg-config will search for the :i386 packages
set(ENV{PKG_CONFIG_PATH} /usr/lib/i386-linux-gnu/pkgconfig/ CACHE STRING "" FORCE)
