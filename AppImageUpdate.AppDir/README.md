# AppImageUpdate

Copyright (c) 2004-15 Simon Peter <probono@puredarwin.org> and contributors.

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
wget http://bintray.com/artifact/download/probono/AppImages/AppImageUpdate-20151218-x86_64.AppImage
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
As you can see, you just need to point to a fixed URL that has the `latest` zsync file. zsync can work with most servers that support returning partial content from files, as is also used for video streaming. See [http://zsync.moria.orc.uk/server-issues](http://zsync.moria.orc.uk/server-issues) for more information.

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

## TODO

 * Implement gpg signature checking
 * Implement a GUI
 * Implement more transport mechanisms, e.g. peer-to-peer
