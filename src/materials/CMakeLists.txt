include_directories(${YAF_INCLUDE_DIRS})

add_definitions(-DBUILDING_YAFRAYPLUGIN)

add_library(glass SHARED glass.cc)
target_link_libraries(glass yafaraycore)

add_library(roughglass SHARED roughglass.cc)
target_link_libraries(roughglass yafaraycore)

add_library(blend SHARED blend.cc)
target_link_libraries(blend yafaraycore)

add_library(glossy SHARED glossy.cc)
target_link_libraries(glossy yafaraycore)

add_library(glossy2 SHARED glossy2.cc)
target_link_libraries(glossy2 yafaraycore)

add_library(shinydiffuse SHARED shinydiffuse.cc)
target_link_libraries(shinydiffuse yafaraycore)

add_library(coatedglossy SHARED coatedglossy.cc)
target_link_libraries(coatedglossy yafaraycore)

add_library(simple SHARED simple.cc mask.cc)
target_link_libraries(simple yafaraycore)

add_library(volumetric SHARED beer.cc)
target_link_libraries(volumetric yafaraycore)

install (TARGETS glass roughglass blend glossy glossy2 shinydiffuse coatedglossy simple volumetric
		${YAF_TARGET_TYPE} DESTINATION ${YAF_PLUGIN_DIR})
