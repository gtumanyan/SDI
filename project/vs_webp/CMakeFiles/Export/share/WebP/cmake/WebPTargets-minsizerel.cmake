#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "WebP::webpdecoder" for configuration "MinSizeRel"
set_property(TARGET WebP::webpdecoder APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(WebP::webpdecoder PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/webpdecoder.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS WebP::webpdecoder )
list(APPEND _IMPORT_CHECK_FILES_FOR_WebP::webpdecoder "${_IMPORT_PREFIX}/lib/webpdecoder.lib" )

# Import target "WebP::webp" for configuration "MinSizeRel"
set_property(TARGET WebP::webp APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(WebP::webp PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/webp.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS WebP::webp )
list(APPEND _IMPORT_CHECK_FILES_FOR_WebP::webp "${_IMPORT_PREFIX}/lib/webp.lib" )

# Import target "WebP::webpdemux" for configuration "MinSizeRel"
set_property(TARGET WebP::webpdemux APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(WebP::webpdemux PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "C"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/webpdemux.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS WebP::webpdemux )
list(APPEND _IMPORT_CHECK_FILES_FOR_WebP::webpdemux "${_IMPORT_PREFIX}/lib/webpdemux.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
