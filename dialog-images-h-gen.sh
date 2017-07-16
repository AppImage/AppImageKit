#!/bin/bash
set -e
set -x

inkscape -z -e appimagetool-48x48.png -w 48 -h 48 resources/appimagetool.svg
inkscape -z -e alacarte-96x96.png -w 96 -h 96 resources/alacarte.svg
inkscape -z -e oxygen-launch-96x96.png -w 96 -h 96 resources/oxygen-launch.svg

cat <<EOL> dialog_images.h
// LICENSE
#define WINDOW_ICON appimagetool_48x48_png
#define WINDOW_ICON_LEN appimagetool_48x48_png_len

// resources/alacarte.COPYING
#define MENU_ICON alacarte_96x96_png
#define MENU_ICON_LEN alacarte_96x96_png_len

// resources/oxygen.COPYING
#define LAUNCH_ICON oxygen_launch_96x96_png
#define LAUNCH_ICON_LEN oxygen_launch_96x96_png_len

EOL

xxd -i appimagetool-48x48.png >> dialog_images.h
echo "" >> dialog_images.h
xxd -i alacarte-96x96.png >> dialog_images.h
echo "" >> dialog_images.h
xxd -i oxygen-launch-96x96.png >> dialog_images.h
rm -f appimagetool-48x48.png alacarte-96x96.png oxygen-launch-96x96.png

sed -i 's|unsigned int|int|' dialog_images.h

