cmake_minimum_required(VERSION 3.28)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please create and use a separate build directory.")
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "")
endif()

# ==================================================================================== #

project("CubeChip" LANGUAGES CXX)
option(CUBECHIP_WINDOWS_NO_ROUNDED_CORNERS "Windows only: disable rounded window corners (requires Windows 11 22H2 / Build 22621 or newer to take effect)" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PROJECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(PROJECT_RESOURCE_DIR "${PROJECT_SOURCE_DIR}/resource")
set(PROJECT_VENDOR_DIR "${CMAKE_BINARY_DIR}/include/vendor")
set(PROJECT_EMBED_DIR "${CMAKE_BINARY_DIR}/embed")

include("cmake/_project_filelist.cmake")
include("cmake/_vendor_utils.cmake")
include("cmake/_versioning.cmake")
include("cmake/_generate_embed.cmake")

include("cmake/atomic_queue.cmake")
include("cmake/tl-expected.cmake")
include("cmake/cxxopts.cmake")
include("cmake/fmt.cmake")
include("cmake/mio.cmake")
include("cmake/tomlplusplus.cmake")
include("cmake/nlohmann-json.cmake")
include("cmake/sdl3.cmake")
include("cmake/imgui.cmake")

# ==================================================================================== #

add_executable("${PROJECT_NAME}"
	${PROJECT_SOURCE_FILES}
)

embed_binary_file(
	"${PROJECT_RESOURCE_DIR}/app_logo.png"
	"${PROJECT_EMBED_DIR}/app_logo.data"
	"${CMAKE_SOURCE_DIR}/cmake/_hexdump.cmake"
)
add_custom_target(generate_embedded_assets
    DEPENDS "${PROJECT_EMBED_DIR}/app_logo.data"
)
add_dependencies("${PROJECT_NAME}" generate_embedded_assets)

target_sources("${PROJECT_NAME}" PRIVATE
    FILE_SET project_modules TYPE CXX_MODULES
    FILES
		${PROJECT_MODULE_FILES}
)

target_sources("${PROJECT_NAME}" PRIVATE
	${PROJECT_HEADER_FILES}
	${PROJECT_SOURCE_FILES}
)

if(WIN32)
    target_sources("${PROJECT_NAME}" PRIVATE
        "${PROJECT_RESOURCE_DIR}/app.rc"
        #"${PROJECT_RESOURCE_DIR}/app.ico"
    )
endif()

source_group(
    TREE ${PROJECT_INCLUDE_DIR}
    FILES
        ${PROJECT_HEADER_FILES}
        ${PROJECT_SOURCE_FILES}
        ${PROJECT_MODULE_FILES}
)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)
target_compile_definitions(
	"${PROJECT_NAME}" PRIVATE
	FMT_HEADER_ONLY
	TOML_EXCEPTIONS=0
	PROJECT_NAME="${PROJECT_NAME}"
)
if(WIN32 AND CUBECHIP_WINDOWS_NO_ROUNDED_CORNERS)
    target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		WINDOWS_NO_ROUNDED_CORNERS
	)
endif()

target_include_directories(
	"${PROJECT_NAME}" PRIVATE
	"${PROJECT_EMBED_DIR}"
	"${PROJECT_INCLUDE_DIR}/shims"
	"${PROJECT_INCLUDE_DIR}/utilities"
	"${PROJECT_INCLUDE_DIR}/components"
	"${PROJECT_INCLUDE_DIR}/services"
	"${PROJECT_INCLUDE_DIR}/services/audio"
	"${PROJECT_INCLUDE_DIR}/services/video"
	"${PROJECT_INCLUDE_DIR}/systems"
	"${PROJECT_INCLUDE_DIR}/frontend"
	"${PROJECT_INCLUDE_DIR}/vendor/jthread" # hardcoded
)

target_link_libraries(
	"${PROJECT_NAME}" PRIVATE
	max0x7ba::atomic_queue
	tomlplusplus::tomlplusplus
	nlohmann_json::nlohmann_json
	cxxopts::cxxopts
	tl::expected
	fmt::fmt
	mio::mio
	imgui
)

if(UNIX AND NOT APPLE)
    find_package(TBB QUIET)

    if (NOT TBB_FOUND)
        message(FATAL_ERROR
            "TBB is required on this platform (libstdc++ uses it for std::jthread).\n"
            "Install it via:\n"
            "  Arch:     yay -S onetbb   (or: sudo pacman -S intel-oneapi-tbb)\n"
            "  Debian:   sudo apt install libtbb-dev\n"
            "  Fedora:   sudo dnf install tbb-devel\n"
        )
    endif()

    target_link_libraries("${PROJECT_NAME}" PRIVATE TBB::tbb)
endif()

# ==================================================================================== #

if(MSVC)

	set_property(
		DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		PROPERTY VS_STARTUP_PROJECT "${PROJECT_NAME}"
	)
	set_target_properties(
		"${PROJECT_NAME}" PROPERTIES
		PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb"
	)

	target_compile_options(
		"${PROJECT_NAME}" PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
		$<$<CONFIG:Release>: /W4 /MP /utf-8 /O2 /Ob2 /Oi /Ot /Zi /GT /GL>
		$<$<CONFIG:Debug>: /W4 /MP /utf-8 /Od /Zi /RTC1>
	)

	target_link_options(
		"${PROJECT_NAME}" PRIVATE
		$<$<CONFIG:Release>: /LTCG /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /OPT:ICF /DEBUG>
		$<$<CONFIG:Debug>: /SUBSYSTEM:WINDOWS /INCREMENTAL /DEBUG>
	)

else()

	target_compile_options(
		"${PROJECT_NAME}" PRIVATE
		$<$<CONFIG:Release>: -O3 -march=native -flto=auto>
		$<$<CONFIG:Debug>: -Og -g>
	)

	target_link_options(
		"${PROJECT_NAME}" PRIVATE
		$<$<CONFIG:Release>: -flto=auto>
		$<$<CONFIG:Debug>: >
	)

	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		if(UNIX AND NOT APPLE)
			add_custom_command(
				TARGET "${PROJECT_NAME}" POST_BUILD
				COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
				COMMENT "Stripping symbols from $<TARGET_FILE_NAME:${PROJECT_NAME}>"
			)
		elseif(APPLE)
			add_custom_command(
				TARGET "${PROJECT_NAME}" POST_BUILD
				COMMAND strip -x $<TARGET_FILE:${PROJECT_NAME}>
				COMMENT "Stripping local symbols from $<TARGET_FILE_NAME:${PROJECT_NAME}>"
			)
		endif()
	endif()

endif()
