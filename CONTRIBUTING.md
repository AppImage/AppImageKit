# Contributing

We love pull requests from everyone. 
When contributing, please consider to join us on IRC. We are on __#AppImage on irc.freenode.net__.

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

[pr]: https://github.com/probonopd/AppImageKit/compare/appimagetool/master

At this point you're waiting on us. We like to at least comment on pull requests
within three business days (and, typically, one business day). We may suggest
some changes or improvements or alternatives.
