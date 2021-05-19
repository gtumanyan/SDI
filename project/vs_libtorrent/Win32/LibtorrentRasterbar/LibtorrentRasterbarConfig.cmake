# - Config file for the libtorrent package
# It defines the following variables
#  libtorrent_INCLUDE_DIRS - include directories for libtorrent
#  libtorrent_LIBRARIES    - libraries to link against


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was LibtorrentRasterbarConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

####################################################################################

include(CMakeFindDependencyMacro)
find_dependency(Threads)
find_dependency(Boost)

include("${CMAKE_CURRENT_LIST_DIR}/LibtorrentRasterbarTargets.cmake")

get_target_property(libtorrent_INCLUDE_DIRS LibtorrentRasterbar::torrent-rasterbar INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_lt_iface_link_libraries LibtorrentRasterbar::torrent-rasterbar INTERFACE_LINK_LIBRARIES)
get_target_property(_lt_imported_location LibtorrentRasterbar::torrent-rasterbar IMPORTED_LOCATION)
set(libtorrent_LIBRARIES "${_lt_imported_location};${_lt_iface_link_libraries}")
