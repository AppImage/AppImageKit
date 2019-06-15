# Contributing

Welcome to AppImage. A good starting point for most questions is our wiki at https://github.com/AppImage/AppImageKit/wiki

## How to contribute

### I would like to request an AppImage for an application

If there is no AppImage of your favorite application available, please request it from the author(s) of the application, e.g., as a feature request in the issue tracker of the application. For example, if you would like to see an AppImage of Mozilla Firefox, then please leave a comment at https://bugzilla.mozilla.org/show_bug.cgi?id=1249971. The more people request an AppImage from the upstream authors, the more likely is that an AppImage will be provided.

### I would like to provide an AppImage for my application

If you are an application author and would like to make your application available as an AppImage, please start here: https://github.com/AppImage/AppImageKit/wiki/Creating-AppImages. Feel free to open a [GitHub Issue](https://github.com/AppImage/AppImageKit/issues) in the AppImageKit project if you need support using the tools provided by AppImageKit.

### I would like to have my AppImage included in the AppImageHub central directory of available AppImages

Please see the [AppImageHub Readme](https://github.com/AppImage/appimage.github.io/blob/master/README.md).

### I would like to contribute to AppImageKit development

Your contribution is especially welcome in these ares:

* [__High-priority__ issues we need help with](https://github.com/search?utf8=%E2%9C%93&q=org%3AAppImage+label%3Ahelp-wanted+state%3Aopen+label%3Ahigh-priority&type=Issues)
* [Issues we need help with](https://github.com/search?utf8=%E2%9C%93&q=org%3AAppImage+state%3Aopen+label%3Ahigh-priority&type=Issues)
* [Open bugs](https://github.com/search?utf8=%E2%9C%93&q=org%3AAppImage+label%3Abug+state%3Aopen&type=Issues)

If you would like to report issues with AppImageKit itself, or would like to contribute to its development, please see get in touch with us in `#AppImage` on `irc.freenode.net`. We welcome pull requests addressing any of the open issues and/or other bugfixes and/or feature additions. In the case of complex feature additions, it is best to contact us first, before you spend much time.

## Project governance

Founder and project leader Simon Peter (probonopd) reserves the right to define the AppImage format, brand, high-level architecture, user experience, and project governance 

## Rules for contributing

* The only supported version is git master
* We do not bugfix any versions other than git master
* Versions other than git master are considered outdated and obsolete
* Our software comes in AppImage format. Everything else is unsupported by us
* If git master breaks, all other work stops and every contributor focuses exclusively on un-breaking master
* Every contribution shall be sent as a pull request, which must build "green" before it is merged
* Pinned issues shall be closed before other issues are being worked on
