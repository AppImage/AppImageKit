#!/bin/bash
#
# Author : Ismael Barros² <ismael@barros2.org>
# License : BSD http://en.wikipedia.org/wiki/BSD_license
#
# Removes desktop integration for the given AppImage, inluding
# everything that's been installed by registerAppImage.sh


source "$(dirname "$(readlink -f "$0")")/util.sh"


appImage="$1"
[ ! -n "$appImage" ] && exit 1;
appImageOwner="$(getPathOwner "$appImage")"

if [ $EUID -eq 0 -a "$appImageOwner" != "root" ] && cmdExists sudo; then
	echo "  Dropping privileges to user '${appImageOwner}'..."
#	su "$appImageOwner" -c "$0" "$appImage"
	sudo -u "$appImageOwner" "$0" "$appImage"
	exit
fi

echo "Unregistering ${appImage}..."

desktopFile="$(getAppImageDesktopFile "$appImage")"
if [ -f "$desktopFile" ]; then
	echo "  Removing desktop file ${desktopFile}..."
	rm -vf "$desktopFile"
fi

appImage_icon="$(getAppImageIcon "$appImage")"

if [ $flag_enableNotifications ] && cmdExists notify-send; then
	notify-send \
		"$(basename "$appImage") uninstalled" \
		--urgency=low \
		--app-name="AppImage" \
		--icon="$appImage_icon"
fi

rm -vf "$appImage_icon"

appImage_thumbnail="$(getAppImageThumbnail "$appImage")"
rm -vf "$appImage_thumbnail"
