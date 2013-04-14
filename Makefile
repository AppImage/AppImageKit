all: runtime AppRun AppImageAssistant AppImageExtract

AppImageAssistant: runtime
	cp runtime ./AppImageAssistant.AppDir
	cp ./bundled-dependencies-i386/xorriso ./AppImageAssistant.AppDir/usr/bin/
	cp ./bundled-dependencies-i386/libglade-2.0.so.0 ./bundled-dependencies-i386/libvte.so.9 ./bundled-dependencies-i386/libisofs.so.6 ./bundled-dependencies-i386/libisoburn.so.1 ./bundled-dependencies-i386/libburn.so.4 ./AppImageAssistant.AppDir/usr/lib/
	./AppImageAssistant.AppDir/package ./AppImageAssistant.AppDir ./AppImageAssistant

AppImageExtract: AppRun
	cp AppRun ./AppImageExtract.AppDir
	cp ./bundled-dependencies-i386/xorriso ./AppImageExtract.AppDir/usr/bin/
	cp ./bundled-dependencies-i386/libisofs.so.6 ./bundled-dependencies-i386/libisoburn.so.1 ./bundled-dependencies-i386/libburn.so.4 ./AppImageExtract.AppDir/usr/lib/
	./AppImageAssistant.AppDir/package ./AppImageExtract.AppDir ./AppImageExtract

runtime: fuseiso.o isofs.o runtime.o
	gcc -Os -D_FILE_OFFSET_BITS=64 -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -Wall -g -pthread -o runtime ./runtime.o ./fuseiso.o ./isofs.o /usr/lib/libglib-2.0.so -lz /usr/lib/libfuse.so -lrt -ldl
	strip ./runtime
	# ./exepak ./runtime

fuseiso.o: fuseiso.c
	gcc -Os -DHAVE_CONFIG_H -I. -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse glib-2.0` -Wall -g -MT fuseiso.o -MD -MP -MF ".deps/fuseiso.Tpo" -c -o fuseiso.o fuseiso.c

isofs.o: isofs.c
	gcc -Os -DHAVE_CONFIG_H -I. -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse glib-2.0` -Wall -g -MT isofs.o -MD -MP -MF ".deps/isofs.Tpo" -c -o isofs.o isofs.c

runtime.o: runtime.c
	gcc -Os runtime.c -c

AppRun: 
	gcc -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections AppRun.c -o AppRun
	strip --strip-all ./AppRun

clean:
	rm -f *.a *.o runtime AppRun AppImageAssistant AppImageExtract ./AppImageAssistant.AppDir/runtime ./AppImageAssistant.AppDir/usr/lib/lib* ./AppImageExtract.AppDir/AppRun ./AppImageExtract.AppDir/usr/lib/lib* ./AppImageAssistant.AppDir/usr/bin/xorriso ./AppImageExtract.AppDir/usr/bin/xorriso
