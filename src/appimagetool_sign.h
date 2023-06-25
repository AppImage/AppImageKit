#pragma once

static const char* const FORCE_SIGN_ENV_VAR = "APPIMAGETOOL_FORCE_SIGN";

bool sign_appimage(char* appimage_filename, char* key_id, bool verbose);
