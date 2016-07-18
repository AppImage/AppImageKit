# AppImageKit roadmap

Volunteers wanted for the following topics. Feel free to send a PR that adds your name to list item(s).

## Downloading appimages from some kind of software catalog websites and app centers

* Define formal AppImage spec (probonopd)
* Work on tool that lets one extract metadata like application name, author, license, etc. from an AppImage hosted somewhere without having to download it (think "Google for AppImages")
* Get AppImage downloads into KDE Discover
* Write GNOME Software plugin for AppImage

## Running  appimage and browsing its content  via file roller/ark or any other archive tool

* Follow up with upstream projects and/or send PRs
* file-roller
* ark

## Upgrading appimage via right click

* Make zsync_curl use subsequent range requests rather than requesting multiple ranges at once; otherwise Akamai is broken
* Make AppImageUpdate not need the curl binary but expand zsync_curl to include the required curl functionality
* Possibly replace the AppImageUpdate bash script by C code, e.g., in zsync_curl
* Qt GUI in addition to the Vala/GTK 3 one

## Desktop integration

* Resurrect appimaged (probonopd; contact me if you want to take this over)
* Sandbox using firejail (see the sandbox GUI integration experiments in this repository)
* Test/use GNOME portals
