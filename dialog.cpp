/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017, djcj <djcj@gmx.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_PNG_Image.H>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "desktopenvironments.h"
#include "dialog_images.h"

const char *msg_launch      = "\nLaunch app\n\n";
const char *msg_menu        = "\nCreate menu entry\nand launch app";
const char *msg_checkbox    = " Don't show this message again";

const char *msg_launch_de   = "\nStarte App\n\n";
const char *msg_menu_de     = "\nMenüeintrag erstellen\nund App starten";
const char *msg_checkbox_de = " Dieses Fenster nicht erneut anzeigen";

enum {
  RET_LAUNCH_APP = 0,
  RET_MENU_ENTRY = 1,
  RET_DISABLE_MESSAGE = 2,
  RET_CLOSE_WINDOW = 3
};

Fl_Double_Window *win;
int rv = 0;
bool checkbutton_set = false;

std::vector<std::string> splitString(const std::string &str, const char delimiter = ' ')
{
  std::vector<std::string> result;
  std::stringstream ss(str);
  std::string item;

  if (str.empty())
  {
    result.push_back("");
    return result;
  }

  while (std::getline(ss, item, delimiter))
  {
    result.push_back(item);
  }
  return result;
}

void get_system_font(std::string &font)
{
  std::vector<std::string> splitted;
  std::string last;
  std::stringstream fontSize;
  std::ostringstream newFont;

  IDesktopEnvironment *env = IDesktopEnvironment::getInstance();

  if (env == NULL || env->gtkInterfaceFont(font) == false)
  {
    font = "Helvetica";
    delete env;
    return;
  }

  delete env;

  splitted = splitString(font);
  last = splitted.back();
  fontSize << atoi(splitted.back().c_str());

  /* remove font size */
  if (fontSize.str() == last)
  {
    splitted.pop_back();
    last = splitted.back();
  }

  /* remove font slant */
  if (last == "Italic" || last == "Oblique" || last == "Roman") // Times New Roman = Times New??
  {
    splitted.pop_back();
    last = splitted.back();
  }

  /* remove font weight */
  if (last == "Bold" || last == "Light" || last == "Medium" || last == "Demi-Bold" || last == "Black")
  {
    splitted.pop_back();
  }

  std::copy(splitted.begin(), splitted.end()-1, std::ostream_iterator<std::string>(newFont, " "));
  newFont << splitted.back();
  font = newFont.str();
}

void close_cb(Fl_Widget *, long p)
{
  win->hide();
  rv = (int)p;
}

void launch_cb(Fl_Widget *, long p)
{
  win->hide();
  rv = checkbutton_set ? RET_DISABLE_MESSAGE : (int)p;
}

void checkbutton_cb(Fl_Widget *)
{
  checkbutton_set = checkbutton_set ? false : true;
}

int launcher(const char *title)
{
  char *lang = getenv("LANG");

  if (lang == NULL)
  {
    lang = getenv("LANGUAGE");
  }

  if (lang != NULL && strcmp(lang, "C") != 0 && strncmp(lang, "en", 2) != 0)
  {
    if (strncmp(lang, "de", 2) == 0)
    {
      msg_launch = msg_launch_de;
      msg_menu = msg_menu_de;
      msg_checkbox = msg_checkbox_de;
    }
  }

  win = new Fl_Double_Window(480, 286, title);
  win->callback(close_cb, RET_CLOSE_WINDOW);
  {
    { Fl_Button *o = new Fl_Button(40, 40, 180, 180, msg_launch);
      o->image(new Fl_PNG_Image(NULL, oxygen_launch_96x96_png, (int)oxygen_launch_96x96_png_len));
      o->box(FL_GLEAM_THIN_UP_BOX);
      o->down_box(FL_GLEAM_THIN_DOWN_BOX);
      o->clear_visible_focus();
      o->callback(launch_cb, RET_LAUNCH_APP); }

    { Fl_Button *o = new Fl_Button(260, 40, 180, 180, msg_menu);
      o->image(new Fl_PNG_Image(NULL, alacarte_96x96_png, (int)alacarte_96x96_png_len));
      o->box(FL_GLEAM_THIN_UP_BOX);
      o->down_box(FL_GLEAM_THIN_DOWN_BOX);
      o->clear_visible_focus();
      o->callback(close_cb, RET_MENU_ENTRY); }

    { Fl_Check_Button *o = new Fl_Check_Button(40, 240, 400, 26, msg_checkbox);
      o->clear_visible_focus();
      o->callback(checkbutton_cb); }
  }
  win->position((Fl::w() - win->w()) / 2, (Fl::h() - win->h()) / 2);
  win->end();
  win->show();

  Fl::run();
  return rv;
}

int main(int argc, char **argv)
{
  const char *title = "AppImage";

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0)
    {
      printf("Usage: %s WINDOWTITLE\n\n"
        "Return values:\n\tlaunch app -> %d\n\tmenu entry -> %d\n\t"
        "disable message + launch app -> %d\n\tclose window -> %d\n"
        /* identify usage as required per FLTK license LGPL exception condition 4 */
        "\n\nThis program is using FLTK v%d.%d.%d (http://www.fltk.org)\n",
        argv[0], RET_LAUNCH_APP, RET_MENU_ENTRY, RET_DISABLE_MESSAGE,
        RET_CLOSE_WINDOW, FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION);
      return 0;
    }
    title = argv[1];
  }

  std::string font;
  get_system_font(font);
  Fl::set_font(FL_HELVETICA, font.c_str());

  Fl::get_system_colors();
  Fl_Window::default_icon(new Fl_PNG_Image(NULL, appimagetool_48x48_png, (int)appimagetool_48x48_png_len));

  return launcher(title);
}

