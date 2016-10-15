# Build an AppImage containing appimagetool and mksquashfs

if [ ! -d ./build ] ; then
  echo "You need to run build.sh first"
fi

mkdir -p appimagetool.AppDir/usr/bin
cp -f build/appimagetool appimagetool.AppDir/usr/bin

cd squashfs-tools/squashfs-tools
make
strip mksquashfs
sudo cp mksquashfs /usr/local/bin

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

cp ../appimagetool.svg .
ln -s appimagetool.svg .DirIcon

ln -s usr/bin/appimagetool AppRun

cd ..

# Eat our own dogfood
build/appimagetool appimagetool.AppDir

# Test whether it has worked
ls -lh ./*.AppImage
