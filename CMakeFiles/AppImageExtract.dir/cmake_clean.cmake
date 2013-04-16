FILE(REMOVE_RECURSE
  "AppImageAssistant"
  "AppImageExtract"
  "./AppImageAssistant.AppDir/runtime"
  "./AppImageAssistant.AppDir/usr/lib/"
  "./AppImageExtract.AppDir/AppRun"
  "./AppImageExtract.AppDir/usr/lib/"
  "./AppImageAssistant.AppDir/usr/bin/xorriso"
  "./AppImageExtract.AppDir/usr/bin/xorriso"
  "CMakeFiles/AppImageExtract"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/AppImageExtract.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
