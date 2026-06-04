# MQ2CamHeight — adjustable third-person camera focal-point height (RoF2 EMU).
#
# Standard MacroQuest custom plugin. This repo IS the plugin folder: clone it into
# <MacroQuest>/plugins/MQ2CamHeight/ and configure the MQ build with
# MQ_BUILD_CUSTOM_PLUGINS=ON; the output DLL lands in build/bin/<cfg>/plugins/.
# See the README for commands and install.
#
# Includes Plugin.cmake by absolute path (a custom plugin lives in the top-level
# plugins/<name>/, not src/plugins/<name>/) and uses target_Plugin_props(), which
# wires include dirs, the C++20 std, the output dir, and MQ2Main + pluginapi.

include("${CMAKE_SOURCE_DIR}/src/Plugin.cmake")

find_package(fmt REQUIRED)

set(MQ2CamHeight_SOURCES
    "MQ2CamHeight.cpp"
)
source_groups("Source Files" ${MQ2CamHeight_SOURCES})

add_library(MQ2CamHeight SHARED
    ${MQ2CamHeight_SOURCES}
)

set_target_properties(MQ2CamHeight PROPERTIES FOLDER "plugins")

target_Plugin_props(MQ2CamHeight)

target_link_libraries(MQ2CamHeight PRIVATE
    fmt::fmt
    MQ2Main
)
