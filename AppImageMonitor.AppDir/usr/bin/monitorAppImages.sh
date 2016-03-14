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


AppImageFilenameFilter='.*\.(run|AppImage)'
if [[ $EUID -eq 0 ]]; then
	DefaultAppImageDirectories=("/Applications/")
	for dir in /home/*/Applications; do
		[ -d "$dir" ] && DefaultAppImageDirectories+=("$dir")
	done
else
	DefaultAppImageDirectories=("$HOME/Applications/")
fi

src="$(dirname "$(readlink -f "$0")")"
source "$src/util.sh"


for arg in "$@"; do
	case "$arg" in
		"--clean") export flag_clean=1; shift ;;
		"--enable-notifications") export flag_enableNotifications=1; shift ;;
		"--help") export flag_help=1; shift ;;
	esac
done

showUsage() {
	local self="$1"
	local b=$(echo -e "\e[1m")
	local n=$(echo -e "\e[0m")

	echo "${b}${self}${n} will watch the given AppImage directories for added or removed AppImages,"
	echo "in order to register or unregister their menu entries, thumbnails, MIME types, etc."
	echo
	echo "  It can be run:"
	echo "    ${b}As regular user${n}: it will only watch AppImages for that user, and register ${b}user-specific${n} menu entries"
	echo "    ${b}As root${n}: it'll watch AppImages for all users, and register menu entries either:"
	echo "      ${b}user-spcific${n} menu entries, for AppImages located inside a user's home directory (i.e. \$HOME/Applications/)"
	echo "      ${b}system-wide${n} menu entries, for AppImages located outside any user's home directory (i.e. /Applications)"
	echo
	echo "${b}Usage:${n}"
	echo "  ${self} [AppImage directories...] [AppImages...] [Options...]"
	echo
	echo "    ${b}AppImage directories${n}:  List of directories to be watched"
	echo "    ${b}AppImages${n}:             If one or more AppImages are specified, they will be registered and ${self} will quit"
	echo "    ${b}Options:${n}"
	echo "      --clean:                     Remove all AppImage menu entries in the desktop directories associated to the given AppImage directories"
	echo "      --help:                      Print this help message"
	echo "      --enable-notifications:      Enable desktop notifications every time an AppImage is registered or unregistered"

}

deleteOrphanedDesktopFiles() {

	# Remove orphan menu entries inside the directory corresponding to the given path.
	# If the given path is inside a user's home, that user's desktop directories will
	# be scanned; otherwise the system's desktop directories will be scanned.

	local path="$1"
	local desktopFilesDirectory="$(getDesktopFilesDirectory "$path")"

	echo "Checking $desktopFilesDirectory for orphaned files..."

	for desktopFile in "$desktopFilesDirectory"/${desktopFileVendor}-*.desktop; do
		[ -f "$desktopFile" ] || continue

		local TryExec=$(desktopFile_getParameter "$desktopFile" TryExec)
		[ -n "$TryExec" ] || continue # desktop file doesn't have any TryExec
		[ -f "$TryExec" ] && continue # AppImage still exists

		"$src/unregisterAppImage.sh" "$TryExec"
	done
}

registerAllAppImages() {

	# Register all AppImages contained in the given path, including its subdirectories.

	local path="$1"

	if [ -d "$path" ]; then
		echo "Registering all AppImages in ${path}..."

		local findFilter="$(echo "$AppImageFilenameFilter" | sed -e"s/\([()|]\)/\\\\\1/g")"
		find "$path" -iregex "$findFilter" -exec "$src/registerAppImage.sh" {} \;
	fi
}

unregisterAllAppImages() {

	# Unregister all AppImages contained in the given path.

	local path="$1"
	local desktopFilesDirectory=$(getDesktopFilesDirectory "$path")

	if [ -d "$desktopFilesDirectory" ]; then
		echo "Unregistering all AppImage menu entries in ${desktopFilesDirectory}..."

		rm -vf "$desktopFilesDirectory"/${desktopFileVendor}-*.desktop
	fi
}


watchAppImages() {

	# Watch the given path for AppImages added or removed

	local path="$1"

	echo "Watching AppImages in ${dir}..."

	if [ ! -d "$path" ]; then
		if [ $flag_createAppImageDirectories ]; then
			mkdir -p "$path" || { echo "Could not create $path"; return 1; }
		else
			echo "AppImage directory '$path' doesn't exist yet, waiting for it to be created"
			sleep 1m
			watchAppImages "$path"
			return
		fi
	fi

	inotifywait -m -r -e CREATE -e DELETE -e MOVED_FROM -e MOVED_TO --format '%e:%w%f' "$path" |
		while read event; do
			case "$event" in
				CREATE:*|MOVED_TO:*)
					"$src/registerAppImage.sh" "${event#*:}"
					;;
				DELETE:*|MOVED_FROM:*)
					"$src/unregisterAppImage.sh" "${event#*:}"
					;;
			esac
		done
}


if [ $flag_help ]; then
	[ -n "$APPIMAGE" ] && self="$(basename "$APPIMAGE")" || self="$(basename "$0")"
	showUsage "$self"
	exit 0
fi


files=()
dirs=()
for arg in "$@"; do
	[ -f "$arg" ] && files+=("$arg")
	[ -d "$arg" ] && dirs+=("$arg")
done

if [ ! -n "$dirs" ]; then
	for dir in "${DefaultAppImageDirectories[@]}"; do
		dirs+=("$dir")
	done
fi

if [ -n "$files" ]; then
	for file in "${files[@]}"; do
		"$src/registerAppImage.sh" "$file"
		update-desktop-database "$(getDesktopFilesDirectory "$file")"
	done
	exit 0
fi

if [ -n "$dirs" ]; then
	if [ $flag_clean ]; then
		for dir in ${dirs[@]}; do
			unregisterAllAppImages "$dir"
		done
		exit 0
	fi

	# Drop priority in order to be more background-friendly
	renice -n 1 $$

	for dir in ${dirs[@]}; do
		deleteOrphanedDesktopFiles "$dir"
		registerAllAppImages "$dir"
		update-desktop-database "$(getDesktopFilesDirectory "$dir")"
	done

	for dir in ${dirs[@]}; do
		watchAppImages "$dir" &
	done

	wait
fi
