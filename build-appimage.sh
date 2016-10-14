# Build an AppImage containing appimagetool and mksquashfs

if [ ! -d ./build ] ; then
  echo "You need to run build.sh first"
fi

mkdir -p appimagetool.AppDir/usr/bin
cp -f build/appimagetool appimagetool.AppDir/usr/bin

# Add -offset option to skip n bytes : https://github.com/plougher/squashfs-tools/pull/13
# It seems squashfs-tools need a new maintainer.
wget -c "https://github.com/plougher/squashfs-tools/archive/46afc0d.zip"
unzip 46afc0d.zip
cd squashfs-tools-*/
wget https://patch-diff.githubusercontent.com/raw/plougher/squashfs-tools/pull/13.diff
patch -p1 < 13.diff

cd squashfs-tools
make XZ_SUPPORT=1 mksquashfs
strip mksquashfs
cp mksquashfs ../../build

cd ../../

cp build/appimagetool appimagetool.AppDir/usr/bin/
cp build/mksquashfs appimagetool.AppDir/usr/bin/

cp resources/appimagetool.desktop appimagetool.AppDir
cp resources/appimagetool.png appimagetool.AppDir

cd appimagetool.AppDir
ln -s appimagetool.png .DirIcon
ln -s usr/bin/appimagetool AppRun
cd ..

# Eat our own dogfood
PATH="$PATH:build"
appimagetool appimagetool.AppDir

# Test whether it has worked
ls -lh ./*.AppImage
