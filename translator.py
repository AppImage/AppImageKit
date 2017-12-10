#! /usr/bin/env python3

import argparse
import babel.support
import gettext
import jinja2
import os
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
    with cd(this_dir):
        return os.system("pybabel extract -F babel.cfg -o messages.pot .")


def init_translations():
    retval = 0

    with cd(this_dir):
        for locale in LOCALES:
            this_retval = os.system("pybabel init -i messages.pot -l %s -d locale/" % locale)
            if this_retval > retval:
                retval = this_retval

    return retval


def update_translations():
    retval = 0

    with cd(this_dir):
        for locale in LOCALES:
            this_retval = os.system("pybabel update -i messages.pot -l %s -d locale/" % locale)
            if this_retval > retval:
                retval = this_retval

    return retval


def compile_translations():
    retval = 0

    with cd(this_dir):
        for locale in LOCALES:
            this_retval = os.system("pybabel compile -d locale/ -l %s" % locale)
            if this_retval > retval:
                retval = this_retval

    return retval


def main():
    # parse arguments
    parser = argparse.ArgumentParser("AppImage website translator")

    parser.add_argument("--render-pages", action='store_true')
    parser.add_argument("--extract-strings", action='store_true')
    parser.add_argument("--init-translations", action='store_true')
    parser.add_argument("--update-translations", action='store_true')
    parser.add_argument("--compile-translations", action='store_true')

    ns = parser.parse_args()

    arg_given = False

    # execute all requested operations until one of them fails
    if getattr(ns, "render_pages", False):
        arg_given = True
        if not render_pages():
            sys.exit(1)

    if getattr(ns, "extract_strings", False):
        arg_given = True
        if not extract_strings():
            sys.exit(1)

    if getattr(ns, "init_translations", False):
        arg_given = True
        if not init_translations():
            sys.exit(1)

    if getattr(ns, "update_translations", False):
        arg_given = True
        if not update_translations():
            sys.exit(1)

    if getattr(ns, "compile_translations", False):
        arg_given = True
        if not compile_translations():
            sys.exit(1)

    if not arg_given:
        parser.print_help()
        sys.exit(0)


if __name__ == "__main__":
    main()