# AppImageUpdate

AppImageUpdate lets you update AppImages in a decentral way using information embedded in the AppImage itself. No central repository is involved. This enables upstream application projects to release AppImages that can be easily updated. Since AppImageKit uses delta updates, the downloads are very small and efficient.

## Try it out

```
# Make sure curl is installed on your local machine
# (this dependency will go away)
which curl

# Get Subsurface 449
wget https://bintray.com/artifact/download/probono/AppImages/Subsurface-4.5.1.449-x86_64.AppImage
chmod a+x Subsurface-4.5.1.449-x86_64.AppImage

# Run it
./Subsurface-4.5.1.449-x86_64.AppImage

# Get the updater
wget https://bintray.com/artifact/download/probono/AppImages/AppImageUpdate-20151218-x86_64.AppImage
chmod a+x AppImageUpdate-20151218-x86_64.AppImage

# Update Subsurface to the latest version
./AppImageUpdate-20151218-x86_64.AppImage ./Subsurface-4.5.1.449-x86_64.AppImage

# Look there is a newer version! Run it
NEW=$(ls Subsurface-* | sort -V | tail -n 1)
./$NEW
```

Notice how quick the update was. Combined with fully automated continuous or nightly builds, this should make software "fluid", as users can get the latest development versions very rapidly.
