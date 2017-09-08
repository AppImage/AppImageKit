# Contributing

Welcome to AppImage. A good starting point for most questions is our wiki at https://github.com/AppImage/AppImageKit/wiki.

### I would like to request an AppImage for an application

If there is no AppImage of your favorite application available, please request it from the author(s) of the application, e.g., as a feature request in the issue tracker of the application. For example, if you would like to see an AppImage of Mozilla Firefox, then please leave a comment at https://bugzilla.mozilla.org/show_bug.cgi?id=1249971. The more people request an AppImage from the upstream authors, the more likely is that an AppImage will be provided.

### I would like to provide an AppImage for my application

If you are an application author and would like to make your application available as an AppImage, please start here: https://github.com/AppImage/AppImageKit/wiki/Creating-AppImages. Feel free to open a [GitHub Issue](https://github.com/AppImage/AppImageKit/issues) in the AppImageKit project if you need support using the tools provided by AppImageKit.

### I would like to have my AppImage included in the AppImageHub central directory of available AppImages

Please see the [AppImageHub Readme](https://github.com/AppImage/appimage.github.io/blob/master/README.md).

### I would like to contribute to AppImageKit development

If you would like to report issues with AppImageKit itself, or would like to contribute to its development, please see our [list of issues](https://github.com/AppImage/AppImageKit/issues) and get in touch with us in `#AppImage` on `irc.freenode.net`. We welcome pull requests addressing any of the open issues and/or other bugfixes and/or feature additions. In the case of complex feature additions, it is best to contact us first, before you spend much time.

Fork, then clone the __appimagetool/master__ branch of the repo:

    git clone -b appimagetool/master --single-branch --recursive git@github.com:your-username/AppImageKit.git

Compile:

```
cd AppImageKit/
sudo bash -ex install-build-deps.sh
bash -ex build.sh
```

Make your change. Build again to be sure you didn't break anything. Run the resulting binaries to test them.

Push to your fork and [submit a pull request][pr].

[pr]: https://github.com/AppImage/AppImageKit/compare/appimagetool/master

At this point you're waiting on us. We like to at least comment on pull requests
within three business days (and, typically, one business day). We may suggest
some changes or improvements or alternatives.
