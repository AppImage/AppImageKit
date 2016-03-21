#!/bin/bash
#
# Author : Ismael Barros² <ismael@barros2.org>
# License : BSD http://en.wikipedia.org/wiki/BSD_license
#
# Misc. utilities for monitorAppImages.sh, registerAppImage.sh and unregisterAppImage.sh


desktopFileVendor="AppImage"


cmdExists() {
	command -v "$1" >/dev/null
}

getPathOwner() {
	local path="$1"
	if [[ "$(readlink -f "$path")" =~ ^/home/([^/]*)/.*$ ]]; then
		echo "${BASH_REMATCH[1]}"
	else
		echo "root"
	fi
}

getXDGDataHome() {
	if [ $EUID -eq 0 ]; then
		local path="$1"

		local appImageOwner="root"
		if [ -n "$path" ]; then
			appImageOwner="$(getPathOwner "$path")"
		fi

		if [ "$appImageOwner" = "root" ]; then
			# Root user registering an AppImage inside a system directory
			# The AppImage will be registered for ALL users (system-wide)
			echo "${XDG_DATA_HOME:-/usr/local/share}"
		else
			# Root user registering an AppImage inside a user's directory
			# The AppImage will be registered only to that user
			appImageOwnerHome="$(eval echo "~$appImageOwner")"
			echo "$appImageOwnerHome/.local/share"
		fi
	else
		# Regular user registering any AppImage
		# The AppImage will be registered only to that user
		echo "${XDG_DATA_HOME:-$HOME/.local/share}"
	fi
}

getXDGCacheHome() {
	if [ $EUID -eq 0 ]; then
		local path="$1"

		local appImageOwner="root"
		if [ -n "$path" ]; then
			appImageOwner="$(getPathOwner "$path")"
		fi

		if [ "$appImageOwner" = "root" ]; then
			# Root user registering an AppImage inside a system directory
			# The AppImage will be registered for ALL users (system-wide)
			echo "${XDG_DATA_HOME:-$HOME/.cache}"
		else
			# Root user registering an AppImage inside a user's directory
			# The AppImage will be registered only to that user
			appImageOwnerHome="$(eval echo "~$appImageOwner")"
			echo "$appImageOwnerHome/.cache"
		fi
	else
		# Regular user registering any AppImage
		# The AppImage will be registered only to that user
		echo "${XDG_DATA_HOME:-$HOME/.cache}"
	fi
}

getDesktopFilesDirectory() {
	local path="$1"
	echo "$(getXDGDataHome "$path")/applications"
}

getAppImageDesktopFile() {
	local AppImage="$1"

	local desktopFilesDirectory="$(getDesktopFilesDirectory "$AppImage")"
	local name="$(basename "$AppImage")"
	name="${name// /_}"

	echo "${desktopFilesDirectory}/${desktopFileVendor}-${name}.desktop"
}
getAppImageIcon() {
	local AppImage="$1"

	local xdgDataHome="$(getXDGDataHome "$AppImage")"
	local prefix="application-x-appimage"
	local name="$(basename "$AppImage")"
	name="${name// /_}"

	echo "${xdgDataHome}/icons/${prefix}-${name}.png"
}

getAppImageThumbnail() {
	local AppImage="$1"

	local xdgCacheHome="$(getXDGCacheHome "$AppImage")"
	local url="file://$(readlink -f "$AppImage")"
	local checksum="$(echo -n "$url" | md5sum | cut -c -32)"

	echo "${xdgCacheHome}/thumbnails/normal/${checksum}.png"
}

desktopFile_getParameter() {
	file="$1"
	parameter="$2"
	grep "^${parameter}=" "$file" | cut -d= -f2- | cut -d\" -f2 | sed -e 's/^[ \t]*//g' -e 's/[ \t]*$//g'
}
