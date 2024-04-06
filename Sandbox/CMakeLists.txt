include(FetchContent)

set(SOURCE_LIST
	"main.cpp"
	"Options.hpp"
	"UI.cpp"
	"UI.hpp")

add_executable(Sandbox ${SOURCE_LIST})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE_LIST})

target_include_directories(Sandbox PRIVATE "Engine")

target_link_libraries(Sandbox PRIVATE Engine)

set_property(TARGET Sandbox PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set_compile_flags(Sandbox)

# Download & extract level asset
FetchContent_Declare(LevelAsset 
  URL https://renscreations.com/files/Bistro_small.7z
  SOURCE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DownloadedAssets)
FetchContent_MakeAvailable(LevelAsset)

# Copy local assets
file(COPY Fonts DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(COPY Materials DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(COPY Textures DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Compile shaders
if(UNIX)
  if(DEFINED ENV{VULKAN_SDK})
    set(GLSLC "$ENV{VULKAN_SDK}/bin/glslc")
  else()
    set(GLSLC "glslc")
  endif()
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSLC "$ENV{VULKAN_SDK}/Bin/glslc.exe")
else()
  set(GLSLC "$ENV{VULKAN_SDK}/Bin32/glslc.exe")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
  "Shaders/*.vert"
  "Shaders/*.frag"
  "Shaders/*.comp")

# Thanks to: https://gist.github.com/evilactually/a0d191701cb48f157b05be7f74d79396
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(STEM ${GLSL} NAME_WLE)
  get_filename_component(EXT ${GLSL} LAST_EXT)
  string(SUBSTRING ${EXT} 1 -1 EXT)
  set(SPIRV "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders/${STEM}_${EXT}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSLC} ${GLSL} --target-env=vulkan1.3 -g -o ${SPIRV} # Debug flag (-g) needed for SPIRV-Reflect
    DEPENDS ${GLSL} ${GLSL_HEADER_FILES})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(Sandbox compile_shaders)
