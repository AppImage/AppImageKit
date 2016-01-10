CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

option(USE_LIBC_WRAP_GENERATOR "Use LibcWrapGenerator" ON)

ADD_CUSTOM_COMMAND(
  OUTPUT LibcWrapGenerator
  COMMAND valac --pkg gee-0.8 --pkg posix --pkg glib-2.0 --pkg gio-2.0 ./LibcWrapGenerator.vala
)
ADD_CUSTOM_COMMAND(
  OUTPUT libcwrap.h
  DEPENDS LibcWrapGenerator
  COMMAND ./LibcWrapGenerator --target 2.14 --libdir /lib --output libcwrap.h
)

ADD_CUSTOM_TARGET(libcwrap DEPENDS libcwrap.h)
