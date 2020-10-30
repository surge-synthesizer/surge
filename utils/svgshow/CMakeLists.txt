# Subordinate CMakeList for svgshow

add_executable( svgshow )
message( STATUS "Include SVGSHOW build target" )

set( SVGSHOW_INCLUDE ${SURGE_COMMON_INCLUDES} ${SURGE_GUI_INCLUDES} ${OS_INCLUDE_DIRECTORIES} )
list( TRANSFORM SVGSHOW_INCLUDE PREPEND "../../" )

target_sources( svgshow PRIVATE
  src/main.cpp
  ../../src/common/gui/CScalableBitmap.cpp
  )
target_include_directories( svgshow PRIVATE ${SVGSHOW_INCLUDE} )

target_compile_definitions( svgshow
  PRIVATE
  ${OS_COMPILE_DEFINITIONS}
  )

if( WIN32 )
  list( FILTER OS_LINK_LIBRARIES EXCLUDE REGEX "libpng" )
endif()

target_link_libraries(svgshow ${OS_LINK_LIBRARIES})

if( APPLE )
  # macOS
  target_sources( svgshow PRIVATE src/cocoa_minimal_main.mm )
  target_sources( svgshow PRIVATE ../../vstgui.surge/vstgui/vstgui_mac.mm
    ../../vstgui.surge/vstgui/vstgui_uidescription_mac.mm )


elseif( UNIX AND NOT APPLE )
  # linux - not supported yet
else()
  # windows
  set_target_properties( svgshow PROPERTIES WIN32_EXECUTABLE TRUE )
  target_sources( svgshow PRIVATE src/win32Minimal.cpp )
  target_sources( svgshow PRIVATE ../../vstgui.surge/vstgui/vstgui_win32.cpp
    ../../vstgui.surge/vstgui/vstgui_uidescription_win32.cpp )
  target_compile_options(svgshow PRIVATE /MT)

endif()
