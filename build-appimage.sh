#######################################################################

# Build appimagetool.AppDir

if [ ! -d ./build ] ; then
  echo "You need to run build.sh first"
fi

rm -rf appimagetool.AppDir/ || true
mkdir -p appimagetool.AppDir/usr/bin
cp -f build/appimagetool appimagetool.AppDir/usr/bin

# Build mksquashfs with -offset option to skip n bytes
# https://github.com/plougher/squashfs-tools/pull/13
cd squashfs-tools/squashfs-tools
make XZ_SUPPORT=1 mksquashfs # LZ4_SUPPORT=1 did not build yet on CentOS 6
strip mksquashfs
cp mksquashfs ../../build

cd ../../

cp build/AppRun appimagetool.AppDir/
cp build/appimagetool appimagetool.AppDir/usr/bin/
cp build/mksquashfs appimagetool.AppDir/usr/bin/

cp $(which zsyncmake) appimagetool.AppDir/usr/bin/

cp resources/appimagetool.desktop appimagetool.AppDir/
cp resources/appimagetool.svg appimagetool.AppDir/appimagetool.svg
( cd appimagetool.AppDir/ ; ln -s appimagetool.svg .DirIcon )

#######################################################################

# Build appimaged.AppDir

rm -rf appimaged.AppDir/ || true
mkdir -p appimaged.AppDir/usr/bin
mkdir -p appimaged.AppDir/usr/lib
cp -f build/appimaged appimaged.AppDir/usr/bin

cp build/AppRun appimaged.AppDir/
find /usr/lib -name libarchive.so.3 -exec cp {} appimaged.AppDir/usr/lib/ \;

cp resources/appimaged.desktop appimaged.AppDir/
cp resources/appimagetool.svg appimaged.AppDir/appimaged.svg
( cd appimaged.AppDir/ ; ln -s appimaged.svg .DirIcon )

#######################################################################
