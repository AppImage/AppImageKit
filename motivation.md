# Motivation

Linus addresses some core issues of Linux on the desktop in his [DebConf 14_ QA with Linus Torvalds talk](https://www.youtube.com/watch?v=5PmHRSeA2c8). At 05:40 Linus highlights application packaging: 

> I'm talking about actual application writers that want to make a package of their application for Linux. And I've seen this firsthand with the other project I've been involved with, which is my divelog application.

Obviously Linus is talking about [Subsurface](https://subsurface-divelog.org). 

> We make binaries for Windows and OS X. 

Both bundle not only the application itself, but also the required Qt libraries that the application needs to run. Also included are dependency libraries like `libssh2.1.dylib`and `libzip.2.dylib`.

> We basically don't make binaries for Linux. Why? Because binaries for Linux desktop applications is a major f*ing pain in the ass. Right. You don't make binaries for Linux. You make binaries for Fedora 19, Fedora 20, maybe there's even like RHEL 5 from ten years ago, you make binaries for debian stable.

So why not use the same approach as on Windows and OS X, namely, treat the base operating system as a _platform_ on top of which we run the application we care about. This means that we have to bundle the application with all their dependencies that are _not_ part of the base operating system. Welcome [application bundles](https://blogs.gnome.org/tvb/2013/12/10/application-bundles-for-glade/).

> Or actually you don't make binaries for debian stable because debian stable has libraries that are so old that anything that was built in the last century doesn't work. But you might make binaries for debian... whatever the codename is for unstable. And even that is a major pain because (...) debian has those rules that you are supposed to use shared libraries. Right. 

This is why binaries going into an AppImage should be built against the oldest still-supported LTS or Enterprise distributions, or _all_ dependencies should be privately bundled in the AppImage.

> And if you don't use shared libraries, getting your package in, like, is just painful. 

"Getting your package in" means that the distribution accepts the package as part of the base operating system. For an application, that might not be desired at all. As long as we can package the application in a way that it seamlessly runs on top of the base operating system. 

> But using shared libraries is not an option when the libraries are experimental and the libraries are used by two people and one of them is crazy, so every other day some ABI breaks. 

One simple way to achieve this is to bundle private copies of the libraries in question with the application that uses them. Preferably in a way that does not interfere with anything else that is running on the base operating system. Note that this does not have to be true for all libraries; core libraries that are matured, have stable interfaces and can reasonably expected to be present in all distributions do not necessarily have to be bundled with the application.

> So you actually want to just compile one binary and have it work. Preferably forever. And preferably across all Linux distributions. 

That is actually possible, as long as you stay away from any distribution-specific packaging, and as long as you privately bundle all dependencies.

> And I actually think distributions have done a horribly, horribly bad job. 

Distributions are all about building the base operating system. But I don't think distributions are a good way to get applications. Rather, I would prefer to get the latest versions of applications directly from the people who write them. And this is already a reality for software like Google Chrome, Eclipse, Arduino and other applications. Who uses the (mostly outdated and changed) versions that are part of the distributions? Probably most people don't.

> One of the things that I do on the kernel - and I have to fight this every single release and I think it's sad - we have one rule in the kernel, one rule: we don't break userspace. (...) People break userspace, I get really, really angry. (...) 

Excellent. Thank you for this policy! This is why I can still run the Mosaic browser from over a decade ago on modern Linux-based operating systems. (I tried and it works.)

> And then all the distributions come in and they screw it all up. Because they break binary compatibility left and right. 

Luckily, binaries built on older distributions tend to still work on newer distributions. At least that has been my experience over the last decade with building application bundles using AppImageKit, and before that, klik.

> They update glibc and everything breaks. (...) 

There is a [way around this](https://blogs.gnome.org/tvb/2013/12/14/application-bundles-revisited/), although not many people actually care to use the workaround (yet).

> So that's my rant. And that's what I really fundamentally think needs to change for Linux to work on the desktop because you can't have applications writers to do fifteen billion different versions.

AppImage to the rescue!
