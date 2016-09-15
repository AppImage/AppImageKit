# Build an AppImage containing appimagetool and mksquashfs

if [ ! -d ./build ] ; then
  echo "You need to run build.sh first"
fi

mkdir -p appimagetool.AppDir/usr/bin
cp -f build/appimagetool appimagetool.AppDir/usr/bin

wget -c "https://github.com/plougher/squashfs-tools/archive/46afc0d.zip"
unzip 46afc0d.zip
cd squashfs-tools-*/
wget https://patch-diff.githubusercontent.com/raw/plougher/squashfs-tools/pull/13.diff
patch -p1 < 13.diff

cd squashfs-tools
make
strip mksquashfs

cd ../../

cp squashfs-tools-*/squashfs-tools/mksquashfs appimagetool.AppDir/usr/bin/
cp build/appimagetool appimagetool.AppDir/usr/bin/

cd appimagetool.AppDir

cat > appimagetool.desktop <<\EOF
[Desktop Entry]
Name=appimagetool
Exec=appimagetool
Comment=Tool to generate AppImages from AppDirs
Icon=appimagetool
Categories=Development;
Terminal=true
EOF

wget "https://avatars2.githubusercontent.com/u/16617932?v=3&s=200" -O appimagetool.png
ln -s appimagetool.png .DirIcon

ln -s usr/bin/appimagetool AppRun

cd ..

# Eat our own dogfood
build/appimagetool appimagetool.AppDir appimagetool.AppImage

# Test whether it has worked
ls -lh ./appimagetool.AppImage
./appimagetool.AppImage --version
