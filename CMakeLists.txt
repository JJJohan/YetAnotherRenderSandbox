cmake_minimum_required(VERSION 3.20)

include(CMake/compile_flags.cmake)

project(
  YetAnotherRenderSandbox
  VERSION 0.1
  DESCRIPTION "A Vulkan rendering engine."
  LANGUAGES CXX C)

set(CMAKE_SKIP_INSTALL_RULES ON)

# Sanity check Vulkan SDK exists somewhere.
if(NOT DEFINED ENV{VULKAN_SDK})
    MESSAGE(STATUS "VULKAN_SDK environment variable not set. Required for build.")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  cmake_policy(SET CMP0135 NEW)
endif()

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Binaries)

# Options
option(FASTGLTF_USE_CUSTOM_SMALLVECTOR "Uses a custom SmallVector type optimised for small arrays" ON)
option(ENABLE_AVX2 "Compile with AVX2 vectorisation" OFF)
option(ENABLE_LTO "Compile with Link Time Optimisation" OFF)

# Build Third party dependencies
add_library(fastgltf STATIC
    "ThirdParty/simdjson/simdjson.cpp" 
    "ThirdParty/simdjson/simdjson.h" 
    "ThirdParty/fastgltf/src/fastgltf.cpp" 
	"ThirdParty/fastgltf/src/base64.cpp"
    "ThirdParty/fastgltf/include/fastgltf/base64.hpp" 
	"ThirdParty/fastgltf/include/fastgltf/glm_element_traits.hpp"
	"ThirdParty/fastgltf/include/fastgltf/core.hpp" 
	"ThirdParty/fastgltf/include/fastgltf/tools.hpp" 
	"ThirdParty/fastgltf/include/fastgltf/types.hpp" 
	"ThirdParty/fastgltf/include/fastgltf/util.hpp")

target_compile_options(fastgltf PRIVATE "-w")
target_compile_definitions(fastgltf PUBLIC "FASTGLTF_USE_CUSTOM_SMALLVECTOR=$<BOOL:${FASTGLTF_USE_CUSTOM_SMALLVECTOR}>")
target_include_directories(fastgltf PRIVATE "ThirdParty/simdjson/" "ThirdParty/fastgltf/include/")

add_library(meshoptimizer STATIC
    "ThirdParty/meshoptimizer/src/meshoptimizer.h"
    "ThirdParty/meshoptimizer/src/allocator.cpp"
    "ThirdParty/meshoptimizer/src/clusterizer.cpp"
    "ThirdParty/meshoptimizer/src/indexcodec.cpp"
    "ThirdParty/meshoptimizer/src/indexgenerator.cpp"
    "ThirdParty/meshoptimizer/src/overdrawanalyzer.cpp"
    "ThirdParty/meshoptimizer/src/overdrawoptimizer.cpp"
    "ThirdParty/meshoptimizer/src/quantization.cpp"
    "ThirdParty/meshoptimizer/src/simplifier.cpp"
    "ThirdParty/meshoptimizer/src/spatialorder.cpp"
    "ThirdParty/meshoptimizer/src/stripifier.cpp"
    "ThirdParty/meshoptimizer/src/vcacheanalyzer.cpp"
    "ThirdParty/meshoptimizer/src/vcacheoptimizer.cpp"
    "ThirdParty/meshoptimizer/src/vertexcodec.cpp"
    "ThirdParty/meshoptimizer/src/vertexfilter.cpp"
    "ThirdParty/meshoptimizer/src/vfetchanalyzer.cpp"
    "ThirdParty/meshoptimizer/src/vfetchoptimizer.cpp")	
	
add_library(lz4 STATIC
    "ThirdParty/lz4/lib/lz4.h"
    "ThirdParty/lz4/lib/lz4.c")

add_library(bc7enc_rdo STATIC
    "ThirdParty/bc7enc_rdo/bc7enc.h"
    "ThirdParty/bc7enc_rdo/bc7enc.cpp"
    "ThirdParty/bc7enc_rdo/rgbcx.h"
    "ThirdParty/bc7enc_rdo/rgbcx.cpp"
    "ThirdParty/bc7enc_rdo/rgbcx_table4.h")
	
add_library(spirv_reflect STATIC
    "ThirdParty/SPIRV-Reflect/spirv_reflect.h"
    "ThirdParty/SPIRV-Reflect/spirv_reflect.cpp")
	
add_library(imgui STATIC
	"ThirdParty/imgui/imgui.cpp"
	"ThirdParty/imgui/imgui.h"
	"ThirdParty/imgui/imgui_demo.cpp"
	"ThirdParty/imgui/imgui_draw.cpp"
	"ThirdParty/imgui/imgui_internal.h"
	"ThirdParty/imgui/imgui_stacklayout.cpp"
	"ThirdParty/imgui/imgui_stacklayout.h"
	"ThirdParty/imgui/imgui_stacklayout_internal.h"
	"ThirdParty/imgui/imgui_tables.cpp"
	"ThirdParty/imgui/imgui_widgets.cpp"
	"ThirdParty/imgui/imstb_rectpack.h"
	"ThirdParty/imgui/imstb_textedit.h"
	"ThirdParty/imgui/imstb_truetype.h"
	"ThirdParty/imgui/backends/imgui_impl_win32.cpp"
	"ThirdParty/imgui/backends/imgui_impl_win32.h"
	"ThirdParty/imgui/backends/imgui_impl_vulkan.cpp"
	"ThirdParty/imgui/backends/imgui_impl_vulkan.h")
	
target_compile_options(imgui PRIVATE "-w")
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS IMGUI_DISABLE_OBSOLETE_KEYIO)
target_include_directories(imgui PRIVATE "ThirdParty/imgui/" "$ENV{VULKAN_SDK}/Include")
	
add_library(imgui-node-editor STATIC
	"ThirdParty/imgui-node-editor/imgui_bezier_math.h"
	"ThirdParty/imgui-node-editor/imgui_canvas.cpp"
	"ThirdParty/imgui-node-editor/imgui_canvas.h"
	"ThirdParty/imgui-node-editor/imgui_extra_math.h"
	"ThirdParty/imgui-node-editor/imgui_node_editor.cpp"
	"ThirdParty/imgui-node-editor/imgui_node_editor.h"
	"ThirdParty/imgui-node-editor/imgui_node_editor_api.cpp"
	"ThirdParty/imgui-node-editor/imgui_node_editor_internal.h")
	
target_compile_definitions(imgui-node-editor PUBLIC IMPLOT_DISABLE_OBSOLETE_FUNCTIONS)
target_include_directories(imgui-node-editor PRIVATE "ThirdParty/imgui/")	
target_link_libraries(imgui-node-editor imgui)
	
add_library(implot STATIC
	"ThirdParty/implot/implot.cpp"
	"ThirdParty/implot/implot.h"
	"ThirdParty/implot/implot_internal.h"
	"ThirdParty/implot/implot_items.cpp")

target_include_directories(implot PRIVATE "ThirdParty/imgui/")
target_link_libraries(implot imgui)

mark_third_party(fastgltf)
mark_third_party(meshoptimizer)
mark_third_party(lz4)
mark_third_party(bc7enc_rdo)
mark_third_party(spirv_reflect)
mark_third_party(imgui)
mark_third_party(imgui-node-editor)
mark_third_party(implot)

# Build local projects
add_subdirectory(Engine)
add_subdirectory(Sandbox)

# Compile shaders
# TODO: Copy shader compile from niagara CMakeLists.txt