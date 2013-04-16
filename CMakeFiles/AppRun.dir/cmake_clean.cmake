FILE(REMOVE_RECURSE
  "AppImageAssistant"
  "AppImageExtract"
  "./AppImageAssistant.AppDir/runtime"
  "./AppImageAssistant.AppDir/usr/lib/"
  "./AppImageExtract.AppDir/AppRun"
  "./AppImageExtract.AppDir/usr/lib/"
  "./AppImageAssistant.AppDir/usr/bin/xorriso"
  "./AppImageExtract.AppDir/usr/bin/xorriso"
  "CMakeFiles/AppRun.dir/AppRun.c.o"
  "AppRun.pdb"
  "AppRun"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang C)
  INCLUDE(CMakeFiles/AppRun.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
