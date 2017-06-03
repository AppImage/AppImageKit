#include "squashfuse_dlopen.h"

int have_libloaded = 0;

const char *load_library_errmsg =
  "AppImages require FUSE to run. \n"
  "You might still be able to extract the contents of this AppImage \n"
  "if you run it with the --appimage-extract option. \n"
  "See https://github.com/AppImage/AppImageKit/wiki/FUSE \n"
  "for more information\n";

