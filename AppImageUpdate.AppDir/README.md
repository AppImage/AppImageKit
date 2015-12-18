# AppImageUpdate

Copyright (c) 2004-15 Simon Peter <probono@puredarwin.org> and contributors.

AppImageUpdate lets you update AppImages in a decentral way using information embedded in the AppImage itself. No central repository is involved. This enables upstream application projects to release AppImages that can be updated easily. Since AppImageKit uses delta updates, the downloads are very small and efficient.

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

## Motivation

### Use cases

Here are some concrete use cases for AppImageUpdate:

 * "As a user, I want to get updates for my AppImages easily, without the need to add repositories to my system like EPEL or ppa. I want to be sure nothing is touched on my system apart from the one application I want to update. And I want to keep the old version until I know for sure that the new version works for me.
 * As an application developer, I want to push out nightly or continuous builds to testers easily without having to go though compicated repository setups for multiple distributions. Since I want to support as many distributions as possible, I am looking for something simple and distribution independent.
 * As a server operator, I want to reduce my download traffic by having delta updates. So that users don't have to download the same Qt libs over and over again with each application release, even though they are bundled with the application.

### The problem space

With AppImages allowing upstream software developers to publish applications directly to end users, we need a way to easily update these applications. While we could accomplish this by putting AppImages into traditional `deb` and `rpm` packages and setting up a repository for these, this would have several disadvantages:

 * Setting up multiple repositories for multiple distributions is cumbersome (although some platforms like the openSUSE Build Service make this easier)
 * As the package formats supported by package managers are based on archive formats, the AppImages would have to be archived which means they would have to be extracted/installed - a step that takes additional time
 * Delta updates would be complicated to impossible which means a lot of bandwidth would be wasted (depending on what you consider "a lot")
 * Users would need to configure repositories in their systems (which is cumbersome) and would need root rights to update AppImages

AppImageUpdate to the rescue.

### Objectives

AppImageUpdate has been created with specific objectives in mind.

 1. __Be Simple__. Like using AppImages, updating them should be really easy. Also updates should be easy to understand, create, and manage.
 2. __Be decentral__. Do not rely on central repositories or distributions. While you _can_ use repositories like Bintray with AppImageUpdate, this is purely optional. As long as your webserver supports range requests, you can host your updates entirely yourself.
 3. __Be Fast__. Increase velocity by making software updates really fast.
 4. __Be extensible__. Allow for other transport and distribution mechanisms (like peer-to-peer) in the future.
 5. __Inherit the intensions of the AppImage format__.
