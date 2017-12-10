#! /usr/bin/env python3

import argparse
import babel.support
import jinja2
import os
import subprocess
import sys

LOCALES = ["en"]

this_dir = os.path.abspath(os.path.dirname(__file__))


def render_pages():
    localedir = os.path.join(this_dir, "locale")

    for locale in os.listdir(localedir):
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(this_dir),
            extensions=[
                "jinja2.ext.i18n",
                "jinja2.ext.autoescape",
                "jinja2.ext.with_"
            ],
        )

        locale = locale.split(".mo")[0]

        template = jinja_env.get_template("index.jinja2")

        translation = babel.support.Translations.load(localedir, [locale])

        jinja_env.install_gettext_translations(translation)

        if locale == "en":
            filename = "index.html"
        else:
            filename = "index.%s.html" % locale

        print("Rendering page %s for locale %s" % (filename, locale))

        with open(os.path.join(this_dir, "www", filename), "w") as f:
            f.write(template.render())

    return True


class cd:
    def __init__(self, cd_to):
        self.orig_cwd = None
        self.cd_to = cd_to

    def __enter__(self):
        self.orig_cwd = os.getcwd()
        os.chdir(self.cd_to)

    def __exit__(self, exc_type, exc_val, exc_tb):
        os.chdir(self.orig_cwd)
        self.orig_cwd = None


def extract_strings():
    print("Extracting strings...")
    with cd(this_dir):
        return subprocess.call(["pybabel", "extract", "-F", "babel.cfg", "-o", "messages.pot", "."]) == 0


def init_translations():
    print("Initializating translations...")

    retval = 0

    with cd(this_dir):
        for locale in LOCALES:
            this_retval = subprocess.call(["pybabel", "init", "-i", "messages.pot", "-l", locale, "-d", "locale/"])
            if this_retval > retval:
                retval = this_retval

    return retval == 0


def update_translations():
    print("Updating translations...")

    retval = 0

    with cd(this_dir):
        for locale in LOCALES:
            this_retval = subprocess.call(["pybabel", "update", "-i", "messages.pot", "-l", locale, "-d", "locale/"])
            if this_retval > retval:
                retval = this_retval

    return retval == 0


def compile_translations():
    print("Compiling translations...")

    retval = 0

    with cd(this_dir):
        for locale in LOCALES:
            this_retval = subprocess.call(["pybabel", "compile", "-d", "locale/", "-l", locale])
            if this_retval > retval:
                retval = this_retval

    return retval == 0


def main():
    # parse arguments
    parser = argparse.ArgumentParser("AppImage website translator")

    parser.add_argument("--render-pages", action='store_true')
    parser.add_argument("--extract-strings", action='store_true')
    parser.add_argument("--init", action='store_true')
    parser.add_argument("--update", action='store_true')
    parser.add_argument("--compile", action='store_true')

    ns = parser.parse_args()

    arg_given = any([getattr(ns, i, False) for i in [
        "render_pages", "extract_strings", "init", "update", "compile"
    ]])

    # define default set of operations: compile and render
    if not arg_given:
        ns.compile_translations = True
        ns.render_pages = True

    # execute all requested operations until one of them fails
    if getattr(ns, "extract_strings", False):
        if not extract_strings():
            print("Error extracting strings!")
            sys.exit(1)

    if getattr(ns, "init", False):
        if not init_translations():
            print("Error initializing translations!")
            sys.exit(1)

    if getattr(ns, "update", False):
        if not update_translations():
            print("Error updating translations!")
            sys.exit(1)

    if getattr(ns, "compile", False):
        if not compile_translations():
            print("Error compiling translations!")
            sys.exit(1)

    if getattr(ns, "render_pages", False):
        if not render_pages():
            print("Error rendering pages!")
            sys.exit(1)


if __name__ == "__main__":
    main()