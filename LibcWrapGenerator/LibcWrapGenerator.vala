/*
 * Copyright (C) 2011 Jan Niklas Hasse <jhasse@gmail.com>
 * Copyright (C) 2013 Upstairs Laboratories Inc.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Jan Niklas Hasse <jhasse@gmail.com>
 *   Tristan Van Berkom <tristan@upstairslabs.com>
 */
using GLib;

static const string DEFAULT_TARGET      = "2.7";
static const string DEFAULT_TARGET_HELP = "Target glibc ABI (Default 2.7)";

/***************************************************************
 *                      Debugging Facilities                   *
 ***************************************************************/
[Flags]
public enum DF { 
	COLLECT,
	FILTER,
	VERSION_PARSE,
	VERSION_COMPARE,
	DUMP_FILES
}

static const GLib.DebugKey[] libcwrap_debug_keys = {
	{ "collect",         DF.COLLECT         },
	{ "filter",          DF.FILTER          },
	{ "version-parse",   DF.VERSION_PARSE   },
	{ "version-compare", DF.VERSION_COMPARE },
	{ "dump-files",      DF.DUMP_FILES      }
};

private uint libcwrap_debug_mask = 0;
public delegate void DebugFunc ();

public void libcwrap_note (int domain, DebugFunc debug_func)
{
	if ((libcwrap_debug_mask & domain) != 0)
		debug_func ();
}

/***************************************************************
 *                      VersionNumber class                    *
 ***************************************************************/
class VersionNumber : Object
{
	private int major    { get; set; } // x.0.0
	private int minor    { get; set; } // 0.x.0
	private int revision { get; set; } // 0.0.x
	private string originalString;
	
	public VersionNumber (string version) {

		originalString = version;

		try {
			var regex = new Regex("([[:digit:]]*)\\.([[:digit:]]*)\\.*([[:digit:]]*)");
			var split = regex.split(version);
			
			assert (split.length > 1); // TODO: Don't use assert, print a nice error message instead
			major = int.parse (split[1]);
			
			if (split.length > 2)
				minor = int.parse (split[2]);
			else
				minor = 0;

			if (split.length > 3)
				revision = int.parse (split[3]);
			else
				revision = 0;
		} catch (GLib.RegexError e) {
			stdout.printf ("Error compiling regular expression: %s", e.message);
			Posix.exit (-1);
		}

		libcwrap_note (DF.VERSION_PARSE, () =>
					   stdout.printf ("Version string '%s' parsed as major: %d minor: %d rev: %d\n", 
									  originalString, major, minor, revision));
	}

	public bool newerThan(VersionNumber other) {
		bool newerThanOther = false;

		if (major > other.major) {
			newerThanOther = true;
		} else if (major == other.major) {

			if (minor > other.minor) {
				newerThanOther = true;
			} else if (minor == other.minor) {

				if(revision > other.revision) {
					newerThanOther = true;
				}
			}
		}

		libcwrap_note (DF.VERSION_COMPARE, () =>
					   stdout.printf ("Version '%s' is %s than version '%s'\n", 
									  originalString,
									  newerThanOther ? "newer" : "older",
									  other.getString()));

		return newerThanOther;
	}

	public string getString() {
		return originalString;
	}
}

/***************************************************************
 *                             Main                            *
 ***************************************************************/
public class Main : Object {

	/* Command line options */
	private static string? libdir = null;
	private static string? output = null;
	private static string? target = null;

	private const GLib.OptionEntry[] options = {
		{ "libdir", 'l', 0, OptionArg.FILENAME, ref libdir, "Library directory", "<DIRECTORY>" },
		{ "output", 'o', 0, OptionArg.STRING,   ref output, "Header to create",  "<FILENAME>" },
		{ "target", 't', 0, OptionArg.STRING,   ref target, DEFAULT_TARGET_HELP, "<MAJOR.MINOR[.MICRO]>" },
		{ null }
	};

	/* Local variables */
	private static VersionNumber minimumVersion;
	private static Gee.HashMap<string, VersionNumber>symbolMap;
	private static Gee.HashSet<string>filterMap;

	public static int main (string[] args) {

		/* Initialize debugging */
		string libcwrap_debug_env = GLib.Environment.get_variable ("LIBCWRAP_DEBUG");
		libcwrap_debug_mask = GLib.parse_debug_string (libcwrap_debug_env, libcwrap_debug_keys);

		/* Initialize the default here */
		target = DEFAULT_TARGET;

		try {
			var opt_context = new OptionContext ("- Libc compatibility header generator");
			opt_context.set_help_enabled (true);
			opt_context.add_main_entries (options, null);
			opt_context.parse (ref args);
		} catch (OptionError e) {
			stdout.printf ("error: %s\n", e.message);
			stdout.printf ("Run '%s --help' for a list of available command line options.\n", args[0]);
			return 0;
		}

		if (libdir == null) {
			stdout.printf ("Must specify --libdir\n");
			stdout.printf ("Run '%s --help' for a list of available command line options.\n", args[0]);
			return 0;
		}

		if (output == null) {
			stdout.printf ("Must specify --output\n");
			stdout.printf ("Run '%s --help' for a list of available command line options.\n", args[0]);
			return 0;
		}

		/* Initialize local resources */
		minimumVersion = new VersionNumber (target);

		/* All symbols, containing the newest possible version before 'minimumVersion' if possible */
		symbolMap = new Gee.HashMap<string, VersionNumber>((Gee.HashDataFunc<string>)GLib.str_hash,
														   (Gee.EqualDataFunc<string>)GLib.str_equal);

		/* All symbols which did not have any version > minimumVersion */
		filterMap = new Gee.HashSet<string>((Gee.HashDataFunc<string>)GLib.str_hash,
											(Gee.EqualDataFunc<string>)GLib.str_equal);

		try {

			stdout.printf ("Generating %s (glibc %s) from libs at '%s' .", output, minimumVersion.getString(), libdir);

			parseLibraries ();
			generateHeader ();

		} catch (Error e) {

			warning("%s", e.message);
			return 1;
		}

		stdout.printf(" OK\n");

		return 0;
	}

	private static void parseLibrary (Regex regex, FileInfo fileinfo) throws Error {
		string commandToUse = "objdump -T";
		string output, errorOutput;
		int returnCode;

		/* For testing on objdump files which might be collected on other systems,
		 * run the generator with --libdir /path/to/objdump/files/
		 */
		if ((libcwrap_debug_mask & DF.DUMP_FILES) != 0)
			commandToUse = "cat";

		Process.spawn_command_line_sync (commandToUse + " " + libdir + "/" + fileinfo.get_name(), 
										 out output, out errorOutput, out returnCode);

		if (returnCode != 0)
			return;

		foreach (var line in output.split ("\n")) {

			if (regex.match (line) && !("PRIVATE" in line)) {
				var version      = new VersionNumber (regex.split (line)[3]);
				var symbolName   = regex.split (line)[7];
				var versionInMap = symbolMap.get (symbolName);

				/* Some versioning symbols exist in the objdump output, let's skip those */
				if (symbolName.has_prefix ("GLIBC"))
					continue;

				libcwrap_note (DF.COLLECT, () =>
							   stdout.printf ("Selected symbol '%s' version '%s' from objdump line %s\n", 
											  symbolName, version.getString(), line));

				if (versionInMap == null) {

					symbolMap.set (symbolName, version);

					/* First occurance of the symbol, if it's older
					 * than the minimum required, put it in that table also
					 */
					if (minimumVersion.newerThan (version)) {
						filterMap.add (symbolName);
						libcwrap_note (DF.FILTER, () =>
									   stdout.printf ("Adding symbol '%s %s' to the filter\n", 
													  symbolName, version.getString()));
					}

				} else {

					/* We want the newest possible version of a symbol which is older than the
					 * minimum glibc version specified (or the only version of the symbol if
					 * it's newer than the minimum version)
					 */

					/* This symbol is <= minimumVersion
					 */
					if (!version.newerThan (minimumVersion)) {

						/* What we have in the map is > minimumVersion, so we need this version */
						if (versionInMap.newerThan (minimumVersion))
							symbolMap.set (symbolName, version);

						/* What we have in the map is already <= minimumVersion, but we want
						 * the most recent acceptable symbol
						 */
						else if (version.newerThan (versionInMap))
							symbolMap.set (symbolName, version);


					} else { /* This symbol is > minimumVersion */

						/* If there are only versions > minimumVersion, then we want
						 * the lowest possible version, this is because we try to provide
						 * information in the linker warning about what version the symbol
						 * was initially introduced in.
						 */
						if (versionInMap.newerThan (minimumVersion) &&
							versionInMap.newerThan (version))
							symbolMap.set (symbolName, version);
					}

					/* While trucking along through the huge symbol list, remove symbols from
					 * the 'safe to exclude' if there is a version found which is newer
					 * than the minimum requirement
					 */
					if (version.newerThan (minimumVersion)) {
						filterMap.remove(symbolName);
						libcwrap_note (DF.FILTER, () =>
									   stdout.printf ("Removing symbol '%s' from the filter, found version '%s'\n", 
													  symbolName, version.getString()));
					}
				}

			} else {
				libcwrap_note (DF.COLLECT, () => stdout.printf ("Rejected objdump line %s\n", line));
			}
		}
	}

	private static void parseLibraries () throws Error {
		var libPath    = File.new_for_path (libdir);
		var enumerator = libPath.enumerate_children (FileAttribute.STANDARD_NAME, 0, null);
		var regex      = new Regex ("(.*)(GLIBC_)([0-9]+\\.([0-9]+\\.)*[0-9]+)(\\)?)([ ]*)(.+)");

		var counter    = 0;
		FileInfo fileinfo;

		while ((fileinfo = enumerator.next_file(null)) != null) {

			if (++counter % 50 == 0) {
				stdout.printf(".");
				stdout.flush();
			}

			parseLibrary (regex, fileinfo);
		}
	}

	private static void appendSymbols (StringBuilder headerFile, bool unavailableSymbols) {

		if (unavailableSymbols)
			headerFile.append("\n/* Symbols introduced in newer glibc versions, which must not be used */\n");
		else
			headerFile.append("\n/* Symbols redirected to earlier glibc versions */\n");

		foreach (var it in symbolMap.keys) {
			var version = symbolMap.get (it);
			string versionToUse;

			/* If the symbol only has occurrences older than the minimum required glibc version,
			 * then there is no need to output anything for this symbol
			 */
			if (filterMap.contains (it))
				continue;

			/* If the only available symbol is > minimumVersion, then redirect it
			 * to a comprehensible linker error, otherwise redirect the symbol
			 * to it's existing version <= minimumVersion.
			 */
			if (version.newerThan (minimumVersion)) {

				versionToUse = "DONT_USE_THIS_VERSION_%s".printf (version.getString());
				if (!unavailableSymbols)
					continue;

			} else {

				versionToUse = version.getString ();
				if (unavailableSymbols)
					continue;
			}

			headerFile.append("__asm__(\".symver %s, %s@GLIBC_%s\");\n".printf(it, it, versionToUse));
		}
	}

	private static void generateHeader () throws Error {
		var headerFile = new StringBuilder ();

		/* FIXME: Currently we do:
		 *
		 *   if !defined (__OBJC__) && !defined (__ASSEMBLER__)
		 *
		 * But what we want is a clause which accepts any form of C including C++ and probably
		 * also including ObjC. That said, the generated header works fine for C and C++ sources.
		 */
		headerFile.append ("/* glibc bindings for target ABI version glibc " + minimumVersion.getString() + " */\n");
		headerFile.append ("#if !defined (__LIBC_CUSTOM_BINDINGS_H__)\n");
		headerFile.append ("\n");
		headerFile.append ("#  if !defined (__OBJC__) && !defined (__ASSEMBLER__)\n");
		headerFile.append ("#    if defined (__cplusplus)\n");
		headerFile.append ("extern \"C\" {\n");
		headerFile.append ("#    endif\n");

		/* First generate the available redirected symbols, then the unavailable symbols */
		appendSymbols (headerFile, false);
		appendSymbols (headerFile, true);

		headerFile.append ("\n");
		headerFile.append ("#    if defined (__cplusplus)\n");
		headerFile.append ("}\n");
		headerFile.append ("#    endif\n");
		headerFile.append ("#  endif /* !defined (__OBJC__) && !defined (__ASSEMBLER__) */\n");
		headerFile.append ("#endif\n");

		FileUtils.set_contents (output, headerFile.str);
	}
}
