# AppImageUpdate

Copyright (c) 2004-16 Simon Peter <probono@puredarwin.org> and contributors.

AppImageUpdate lets you update AppImages in a decentral way using information embedded in the AppImage itself. No central repository is involved. This enables upstream application projects to release AppImages that can be updated easily. Since AppImageKit uses delta updates, the downloads are very small and efficient.

## Try it out

```
# Make sure curl is installed on your local machine
# (this dependency will go away)
which curl

# Get Subsurface 449
wget http://bintray.com/artifact/download/probono/AppImages/Subsurface-4.5.1.449-x86_64.AppImage
chmod a+x Subsurface-4.5.1.449-x86_64.AppImage

# Run it
./Subsurface-4.5.1.449-x86_64.AppImage

# Get the updater
wget https://dl.bintray.com/probono/AppImages/AppImageUpdate-408.gb8f778d-x86_64.AppImage
chmod a+x AppImageUpdate-408.gb8f778d-x86_64.AppImage

# Update Subsurface to the latest version
./AppImageUpdate-408.gb8f778d-x86_64.AppImage ./Subsurface-4.5.1.449-x86_64.AppImage

# Look there is a newer version! Run it
NEW=$(ls Subsurface-*.AppImage | sort -V | tail -n 1)
./$NEW
```

Notice how quick the update was. Combined with fully automated continuous or nightly builds, this should make software "fluid", as users can get the latest development versions very rapidly.

## Motivation

### Use cases

Here are some concrete use cases for AppImageUpdate:

 * "As a user, I want to get updates for my AppImages easily, without the need to add repositories to my system like EPEL or ppa. I want to be sure nothing is touched on my system apart from the one application I want to update. And I want to keep the old version until I know for sure that the new version works for me. Of course, I want to check with GPG signatures where the update is coming from.
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
 3. __Be Fast__. Increase velocity by making software updates really fast. This means using delta updates, and allow leveraging existing content delivery networks (CDNs).
 4. __Be extensible__. Allow for other transport and distribution mechanisms (like peer-to-peer) in the future.
 5. __Inherit the intensions of the AppImage format__.

## Update information overview

In order for AppImageUpdate to do its magic, __Update information__ must be embedded inside the AppImage, such as:
 * How can I find out the latest version (e.g., the URL that tells me the latest version)?
 * How can I find out the delta (the portions of the applications that have changed) between my local version and the latest version?
 * How can I download the delta between my local version and the latest version (e.g., the URL of the download server)?

While all of this information could simply be put inside the AppImage, this could be a bit inconvenient since that would mean changing the download server location would require the AppImage to be re-created. Hence, this information is not put into the file system inside the AppImage, but rather embedded into the AppImage in a way that makes it very easy to change this information should it be required, e.g., if you put the files onto a different download server. As you will probably know, an AppImage is both an ISO 9660 file (that you can mount and examine) and an ELF executable (that you can execute). We are using the ISO 9660 Volume Descriptor #1 field "Application Used" to store this information.

You can read this information from an existing AppImage like using `./AppImageUpdate ./Some.AppImage read`. For example:

```
./AppImageUpdate-20151218-x86_64.AppImage ./Subsurface-4.5.1.449-x86_64.AppImage read
bintray-zsync|probono|AppImages|Subsurface|Subsurface-_latestVersion-x86_64.AppImage.zsync
```

Currently two transport mechanisms are implemented:
 * zsync
 * bintray-zsync
 
### zsync

The __zsync__ transport requires only a HTTP server that can handle HTTP range requests. Its update information is in the form

```
zsync|http://server.domain/path/Application-latest-x86_64.AppImage.zsync
```

For an overview about zsync and how to create `.zsync` files, see [http://zsync.moria.org.uk/](http://zsync.moria.org.uk/).
As you can see, you just need to point to a fixed URL that has the `latest` zsync file. zsync can work with most servers that support returning partial content from files, as is also used for video streaming. See [http://zsync.moria.org.uk/server-issues](http://zsync.moria.org.uk/server-issues) for more information.

### bintray-zsync

The __bintray-zsync__ transport extends the zsync transport in that it uses version information from [Bintray](http://bintray.com/). Its update information is in the form

```
bintray-zsync|probono|AppImages|Subsurface|Subsurface-_latestVersion-x86_64.AppImage.zsync
```

Bintray gives developers a CDN-based, reliable, download center with REST automation and support for generic software such as AppImages. According to [this](https://www.jfrog.com/support-service/whitepapers/four-reasons-to-move-distribution-of-docker-images-from-hub-to-bintray/) page, Bintray serves over 200 million downloads per month of 200 thousand packages that reside in 50 thousand repositories. Read ["What Is Bintray?"](http://bintray.com/docs/usermanual/whatisbintray/whatisbintray_whatisbintray.html) for more information.

Since Bintray knows metadata about the AppImages we upload (such as the version), we can use Bintray to figure out what the latest version is. `http://bintray.com/artifact/download/probono/AppImages/Subsurface-_latestVersion-x86_64.AppImage.zsync` ("dummy URL") does not work but luckily we can fill in `_latestVersion` by using the URL `http://bintray.com/probono/AppImages/Subsurface/_latestVersion` ("redirector URL") and parsing the version information from where it redirects to.

## Providing update-able AppImages

If you would like to provide update-able AppImages, you basically have to:
 1. Embed the update information (see above) inside your AppImage
 2. Create a `.zsync` file using `zsyncmake`
 3. Host both your AppImage and the `.zsync` file on a server that supports range requests

You can see this in action as part of an automated build process [here](https://github.com/probonopd/AppImages/blob/1249ce96f1a2bac1cb7a397bde1f74a87e86edf2/bintray.sh#L138-L151).

## Example how to make your app self-updateable


> Do you have more documentation, or maybe an example, on how to add the update information this way?

First, you would have to inject the update information into the AppImage, second you would have to upload the AppImage and the AppImage.zsync to an URL that matches the URL specified in the update information.

**Step 1: Inject the update information into the AppImage**

Assuming you have a file `Etcher-linux-x64.AppImage` in your current working directory, then the following command would tell the updater to look for `Etcher-linux-x64.AppImage.zsync` at the URL `https://resin-production-downloads.s3.amazonaws.com/etcher/latest/Etcher-linux-x64.AppImage.zsync`:

```
echo "zsync|https://resin-production-downloads.s3.amazonaws.com/etcher/latest/Etcher-linux-x64.AppImage.zsync" | dd of="Etcher-linux-x64.AppImage" bs=1 seek=33651 count=512 conv=notrunc 2>/dev/null
```

**Step 2: Upload the AppImage and the AppImage.zsync to an URL that matches the URL specified in the update information**

You would have to copy both `Etcher-linux-x64.AppImage` and `Etcher-linux-x64.AppImage.zsync` to `https://resin-production-downloads.s3.amazonaws.com/etcher/latest/` for this to work.

When you have a new version, simply put the updated  `Etcher-linux-x64.AppImage` and `Etcher-linux-x64.AppImage.zsync` at the *same* location.

At this point, AppImageUpdate should work. AppImageUpdate is really just an example GUI around the (work-in-progress but useable) [appimageupdate](https://github.com/probonopd/AppImageKit/blob/master/AppImageUpdate.AppDir/usr/bin/appimageupdate) bash script. You could put `appimageupdate` (and its dependency [zsync_curl](https://github.com/probonopd/zsync-curl)) inside `usr/bin` of the AppImage, and call `appimageupdate` from within your app and even have a little GUI around it; or reimplement it in the language of your app.

## Other example

```
# Let's use ".AppImage" as the standard suffix
mv krita-3.0-x86_64.appimage Krita-3.0-x86_64.AppImage

# Note that there is no "3" in the URL and no "3.0"
# as this URL must always point to the latest/current zsync file
URL="zsync|http://files.kde.org/krita/linux/Krita-latest-x86_64.AppImage.zsync"

wget https://github.com/probonopd/AppImageKit/raw/master/AppImageUpdate.AppDir/usr/bin/appimageupdate
wget https://github.com/probonopd/zsync-curl/releases/download/_binaries/zsync_curl
chmod a+x zsync_curl
PATH=.:$PATH bash appimageupdate Krita-3.0-x86_64.AppImage set $URL

# Should say:
# "Changed to zsync|http://files.kde.org/krita/linux/Krita-latest-x86_64.AppImage.zsync"

sudo apt install zsync
zsyncmake Krita-3.0-x86_64.AppImage
mv Krita-3.0-x86_64.AppImage.zsync Krita-latest-x86_64.AppImage.zsync

# Now upload Krita-3.0-x86_64.AppImage and Krita-latest-x86_64.AppImage.zsync to
# http://files.kde.org/krita/linux/

# Check whether appimageupdate finds the stuff on the server by running on the client:
PATH=.:$PATH bash appimageupdate ./Krita-3.0-x86_64.AppImage

# Assuming that 3.0.1 comes out, repeat the steps with 3.0.1
```

## TODO

 * Implement a GUI
 * Implement more transport mechanisms, e.g. peer-to-peer
