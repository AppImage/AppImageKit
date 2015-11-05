#!/bin/bash
#
# Author : Ismael Barros² <ismael@barros2.org>
# License : BSD http://en.wikipedia.org/wiki/BSD_license
#
# Monitors one or more directories, waiting for AppImages to be added or deleted,
# creating or removing menu entries accordingly.
#
# By default, directory ~/Applications/ will always be monitored,
# but additional directories can be specified as arguments.
#
# Specific AppImage files may also be passed as argument,
# in order to add them to the menu.
#
# Usage:
#         ./monitorAppImages.sh [AppImages or directories containing AppImages...]


DefaultAppImageDirectory="$HOME/Applications/"
AppImageFilenameFilter='.*\.(run|AppImage)'

DesktopFilesDirectory="/$HOME/.local/share/applications"

add() {
	iso_extract() {
		AppImage="$1"
		shift
		LD_LIBRARY_PATH="./usr/lib/:${LD_LIBRARY_PATH}" PATH="./usr/bin:${PATH}" \
			xorriso -indev "$AppImage" -osirrox on -extract $@ 2>/dev/null
	}

	iso_ls() {
		AppImage="$1"
		shift
		LD_LIBRARY_PATH="./usr/lib/:${LD_LIBRARY_PATH}" PATH="./usr/bin:${PATH}" \
			xorriso -indev "$AppImage" -osirrox on -ls $@ 2>/dev/null \
			| sed -e "s/^'\(.*\)'$/\1/"
	}

	trimp() { sed -e 's/^[ \t]*//g' -e 's/[ \t]*$//g'; }
	desktopFile_getParameter() { file=$1; parameter=$2; grep "^${parameter}=" "$file" | cut -d= -f2- | cut -d\" -f2 | trimp; }


	local app="$1"
	shift
	[ -n "$app" ] || return 1;
	[ -f "$app" ] || return 1;

	file -kib "$app" | grep -q "application/x-executable" || return 1;
	file -kib "$app" | grep -q "application/x-iso9660-image" || return 1;

	echo "Adding ${app}..."

	if [ ! -x "$app" ]; then
		echo "Marking the AppImage executable"
		chmod +x "$app"
	fi
	
	XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
	local name="$(basename "$app")"
	local appImage_icon="$XDG_DATA_HOME/icons/application-x-appimage-${name}.png"
	if [ ! -f "$appImage_icon" ]; then
		# In order to find out the icon filename, we must extract the .desktop file inside the AppImage

		local innerDesktopFilePath=$(iso_ls "$app" "/*.desktop" | head -n1)
		[ -n "$innerDesktopFilePath" ] || { echo "Desktop file not found" >&2; return 1; }
		local appImage_desktopFile="/tmp/AppImage-${name}-tmp.desktop"
		appImage_desktopFile=${appImage_desktopFile// /_}
		#echo "  $innerDesktopFilePath --> $appImage_desktopFile"
		iso_extract "$app" "$innerDesktopFilePath" "$appImage_desktopFile" || { echo "Failed to extract '$innerDesktopFilePath' file" >&2; return 1; }

		local innerIconPath=$(desktopFile_getParameter "$appImage_desktopFile" Icon)
		rm "$appImage_desktopFile"

		# Then, we can actually extract the icon

		[ -n "$innerIconPath" ] || { echo "Icon file not found" >&2; return 1; }
		appImage_icon=${appImage_icon// /_}
		#echo "  $innerIconPath --> $appImage_icon"
		iso_extract "$app" "$innerIconPath" "$appImage_icon" || { echo "Failed to extract icon" >&2; return 1; }
	fi

	local desktopFile="/tmp/AppImage-${name}.desktop"
	desktopFile=${desktopFile// /_} # xdg-desktop-menu hates spaces
	echo -e "[Desktop Entry]\nType=Application\nName=$name\nExec=\"$(readlink -f "$app")\"\nIcon=$appImage_icon" > "$desktopFile"
	xdg-desktop-menu install "$desktopFile" "$@"
	rm "$desktopFile"
}

del() {
	local app="$1"
	shift
	[ ! -n "$app" ] && return 1;

	echo "Removing ${app}..."

	local name="$(basename "$app")"
	local desktopFile="$DesktopFilesDirectory/AppImage-${name}.desktop"
	desktopFile=${desktopFile// /_} # xdg-desktop-menu hates spaces
	if [ -f "$desktopFile" ]; then
		xdg-desktop-menu uninstall "$desktopFile" "$@"
	fi
}


if [ "$1" == "--clean" ]; then
	echo "Removing all the AppImage menu entries in ${DesktopFilesDirectory}/..."
	xdg-desktop-menu uninstall "$DesktopFilesDirectory"/AppImage-*.desktop
	echo "Done"
else
	renice -n 1 $$

	files=()
	dirs=()
	for arg in "$@"; do
		[ -f "$arg" ] && files+=("$arg")
		[ -d "$arg" ] && dirs+=("$arg")
	done

	if [ -n "$files" ]; then
		for file in "${files[@]}"; do
			add "$file" --noupdate
		done
		update-desktop-database "$DesktopFilesDirectory"
	elif [ ! -n "$dirs" ]; then
		if [ -d "$DefaultAppImageDirectory" ]; then
			dirs=("$DefaultAppImageDirectory")
		else
			echo "Directory $DefaultAppImageDirectory doesn't exist"
		fi
	fi

	if [ -n "$dirs" ]; then
		echo "Watching ${dirs[@]}..."

		export -f add
		FindFilter="$(echo "$AppImageFilenameFilter" | sed -e"s/\([()|]\)/\\\\\1/g")"
		find "${dirs[@]}" -iregex "$FindFilter" -exec bash -c 'add "$0" --noupdate' {} \;
		update-desktop-database "$DesktopFilesDirectory"

		inotifywait -m -r -e CREATE -e DELETE -e MOVED_FROM -e MOVED_TO --format '%e:%w%f' $dirs |
			while read event; do
				case "$event" in
					CREATE:*|MOVED_TO:*)
						add "${event#*:}"
						;;
					DELETE:*|MOVED_FROM:*)
						del "${event#*:}"
						;;
				esac
			done
	fi
fi
