#!/bin/bash

# Push AppImages and related metadata to Bintray
# https://bintray.com/docs/api/

trap 'exit 1' ERR

API=https://api.bintray.com
FILE=$1

[ -f "$FILE" ] || exit

BINTRAY_USER="$BINTRAY_USER"
BINTRAY_API_KEY="$BINTRAY_API_KEY" # env
BINTRAY_REPO="$BINTRAY_REPO"
BINTRAY_REPO_OWNER="$BINTRAY_USER" # owner and user not always the same
PCK_NAME=$(basename $1)
WEBSITE_URL="http://appimage.org"
ISSUE_TRACKER_URL="https://github.com/probonopd/AppImages/issues"
VCS_URL="https://github.com/probonopd/AppImages.git" # Mandatory for packages in free Bintray repos

# Figure out whether we should use sudo
SUDO=''
if (( $EUID != 0 )); then
  SUDO='sudo'
fi

if [ -e /usr/bin/apt-get ]; then
  $SUDO apt-get update
  $SUDO apt-get -y install curl bsdtar zsync
fi

if [ -e /usr/bin/yum ]; then
  $SUDO yum -y install install curl bsdtar zsync
fi

if [ -e /usr/bin/pacman ]; then
  builddeps=('curl' 'libarchive' 'zsync')
  for i in "${builddeps[@]}"; do
    if ! pacman -Q "$i"; then
      $SUDO pacman -S "$i"
    fi
  done
fi

which curl || exit 1
which bsdtar || exit 1 # https://github.com/libarchive/libarchive/wiki/ManPageBsdtar1 ; isoinfo cannot read zisofs
which grep || exit 1
which zsyncmake || exit 1

if [ ! $(env | grep BINTRAY_API_KEY ) ] ; then
  echo "Environment variable \$BINTRAY_API_KEY missing"
  exit 1
fi

# Do not upload artefacts generated as part of a pull request
if [ $(env | grep TRAVIS_PULL_REQUEST ) ] ; then
  if [ "$TRAVIS_PULL_REQUEST" != "false" ] ; then
    echo "Not uploading since this is a pull request"
    exit 0
  fi
fi

CURL="curl -u${BINTRAY_USER}:${BINTRAY_API_KEY} -H Content-Type:application/json -H Accept:application/json"

# Get metadata from the desktop file inside the AppImage
DESKTOP=$(bsdtar -tf "${FILE}" | grep ^./[^/]*.desktop$ | head -n 1)
# Extract the description from the desktop file

echo "* DESKTOP $DESKTOP"

PCK_NAME=$(bsdtar -f "${FILE}" -O -x ./"${DESKTOP}" | grep -e "^Name=" | head -n 1 | sed s/Name=//g | cut -d " " -f 1 | xargs)
if [ "$PCK_NAME" == "" ] ; then
  bsdtar -f "${FILE}" -O -x ./"${DESKTOP}"
  echo "PCK_NAME missing in ${DESKTOP}, exiting"
  exit 1
else
  echo "* PCK_NAME $PCK_NAME"
fi

DESCRIPTION=$(bsdtar -f "${FILE}" -O -x ./"${DESKTOP}" | grep -e "^Comment=" | sed s/Comment=//g)

ICONNAME=$(bsdtar -f "${FILE}" -O -x "${DESKTOP}" | grep -e "^Icon=" | sed s/Icon=//g)

# Look for .DirIcon first
ICONFILE=$(bsdtar -tf "${FILE}" | grep /.DirIcon$ | head -n 1 )

# Look for svg next
if [ "$ICONFILE" == "" ] ; then
 ICONFILE=$(bsdtar -tf "${FILE}" | grep ${ICONNAME}.svg$ | head -n 1 )
fi

# If there is no svg, then look for pngs in usr/share/icons and pick the largest
if [ "$ICONFILE" == "" ] ; then
  ICONFILE=$(bsdtar -tf "${FILE}" | grep usr/share/icons.*${ICONNAME}.png$ | sort -V | tail -n 1 )
fi

# If there is still no icon, then take any png
if [ "$ICONFILE" == "" ] ; then
  ICONFILE=$(bsdtar -tf "${FILE}" | grep ${ICONNAME}.png$ | head -n 1 )
fi

if [ ! "$ICONFILE" == "" ] ; then
  echo "* ICONFILE $ICONFILE"
  bsdtar -f "${FILE}" -O -x "${ICONFILE}" > /tmp/_tmp_icon
  echo "xdg-open /tmp/_tmp_icon"
fi

# Check if there is appstream data and use it
APPDATANAME=$(echo ${DESKTOP} | sed 's/.desktop/.appdata.xml/g' | sed 's|./||'  )
APPDATAFILE=$(bsdtar -tf "${FILE}" | grep ${APPDATANAME}$ | head -n 1 || true)
APPDATA=$(bsdtar -f "${FILE}" -O -x "${APPDATAFILE}" || true)
if [ "$APPDATA" == "" ] ; then
  echo "* APPDATA missing"
else
  echo "* APPDATA found"
  DESCRIPTION=$(echo $APPDATA | grep -o -e "<description.*description>" | sed -e 's/<[^>]*>//g')
  WEBSITE_URL=$(echo $APPDATA | grep "homepage" | head -n 1 | cut -d ">" -f 2 | cut -d "<" -f 1)
fi

if [ "$DESCRIPTION" == "" ] ; then
  bsdtar -f "${FILE}" -O -x ./"${DESKTOP}"
  echo "DESCRIPTION missing and no Comment= in ${DESKTOP}, exiting"
  exit 1
else
  echo "* DESCRIPTION $DESCRIPTION"
fi

if [ "$VERSION" == "" ] ; then
  echo "* VERSION missing, trying to get from the filename"
  if [[ "$(basename "$FILE")" =~ (.*)\ (.*)-([^- ]*).AppImage ]]; then
    # NAME="${BASH_REMATCH[1]}"
    VERSION="${BASH_REMATCH[2]}"
    # ARCH="${BASH_REMATCH[3]}"
  else
    echo "Could not parse filename $FILE"
  fi
fi

if [ "$VERSION" == "" ] ; then
  echo "* VERSION missing, exiting"
  exit 1
else
  echo "* VERSION $VERSION"
fi

# exit 0
##########

echo ""
echo "Creating package ${PCK_NAME}..."
    data="{
    \"name\": \"${PCK_NAME}\",
    \"desc\": \"${DESCRIPTION}\",
    \"desc_url\": \"auto\",
    \"website_url\": [\"${WEBSITE_URL}\"],
    \"vcs_url\": [\"${VCS_URL}\"],
    \"issue_tracker_url\": [\"${ISSUE_TRACKER_URL}\"],
    \"licenses\": [\"MIT\"],
    \"labels\": [\"AppImage\", \"AppImageKit\"]
    }"
${CURL} -X POST -d "${data}" ${API}/packages/${BINTRAY_REPO_OWNER}/${BINTRAY_REPO}

if [ $(which zsyncmake) ] ; then
  echo ""
  echo "Embedding update information into ${FILE}..."
  # Clear ISO 9660 Volume Descriptor #1 field "Application Used" 
  # (contents not defined by ISO 9660) and write URL there
  dd if=/dev/zero of="${FILE}" bs=1 seek=33651 count=512 conv=notrunc
  # Example for next line: Subsurface-_latestVersion-x86_64.AppImage
  NAMELATESTVERSION=$(echo $(basename ${FILE}) | sed -e "s|${VERSION}|_latestVersion|g")
  # Example for next line: bintray-zsync|probono|AppImages|Subsurface|Subsurface-_latestVersion-x86_64.AppImage.zsync
  LINE="bintray-zsync|${BINTRAY_REPO_OWNER}|${BINTRAY_REPO}|${PCK_NAME}|${NAMELATESTVERSION}.zsync"
  echo "${LINE}" | dd of="${FILE}" bs=1 seek=33651 count=512 conv=notrunc
  echo ""
  echo "Uploading and publishing zsync file for ${FILE}..."
  # Workaround for:
  # https://github.com/probonopd/zsync-curl/issues/1
  zsyncmake -u http://dl.bintray.com/${BINTRAY_REPO_OWNER}/${BINTRAY_REPO}/$(basename ${FILE}) ${FILE} -o ${FILE}.zsync
  ${CURL} -T ${FILE}.zsync "${API}/content/${BINTRAY_REPO_OWNER}/${BINTRAY_REPO}/${PCK_NAME}/${VERSION}/$(basename ${FILE}).zsync?publish=1&override=1"
else
  echo "zsyncmake not found, skipping zsync file generation and upload"
fi

echo ""
echo "Uploading and publishing ${FILE}..."
${CURL} -T ${FILE} "${API}/content/${BINTRAY_REPO_OWNER}/${BINTRAY_REPO}/${PCK_NAME}/${VERSION}/$(basename ${FILE})?publish=1&override=1"

if [ $(env | grep TRAVIS_JOB_ID ) ] ; then
echo ""
echo "Adding Travis CI log to release notes..."
BUILD_LOG="https://api.travis-ci.org/jobs/${TRAVIS_JOB_ID}/log.txt?deansi=true"
    data='{
  "bintray": {
    "syntax": "markdown",
    "content": "'${BUILD_LOG}'"
  }
}'
${CURL} -X POST -d "${data}" ${API}/packages/${BINTRAY_REPO_OWNER}/${BINTRAY_REPO}/${PCK_NAME}/versions/${VERSION}/release_notes
fi

# Seemingly this works only after the second time running this script - thus disabling for now (FIXME)
# echo ""
# echo "Adding ${FILE} to download list..."
# sleep 5 # Seemingly needed
#     data="{
#     \"list_in_downloads\": true
#     }"
# ${CURL} -X PUT -d "${data}" ${API}/file_metadata/${BINTRAY_REPO_OWNER}/${BINTRAY_REPO}/$(basename ${FILE})
# echo "TODO: Remove earlier versions of the same architecture from the download list"

# echo ""
# echo "TODO: Uploading screenshot for ${FILE}..."
