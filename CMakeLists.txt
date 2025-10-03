cmake_minimum_required(VERSION 3.25)
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
add_definitions(-DPROJECT_NAME=\"${PROJECT_NAME}\")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PROJECT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(PROJECT_VENDOR_DIR "${PROJECT_INCLUDE_DIR}/vendor")

include("cmake/_project_filelist.cmake")
include("cmake/_vendor_utils.cmake")
include("cmake/_versioning.cmake")

include("cmake/atomic_queue.cmake")
include("cmake/tl-expected.cmake")
include("cmake/cxxopts.cmake")
include("cmake/fmt.cmake")
include("cmake/tomlplusplus.cmake")
include("cmake/nlohmann-json.cmake")
include("cmake/sdl3.cmake")
include("cmake/imgui.cmake")

# ==================================================================================== #

add_executable("${PROJECT_NAME}"
	${FRONTEND_SOURCES}
	${COMPONENTS_SOURCES}
	${UTILITIES_SOURCES}
	${SERVICES_SOURCES}
	${SYSTEMS_SOURCES}
	${SYSTEM_CHIP8_SOURCES}
	${SYSTEM_BYTEPUSHER_SOURCES}
	${SYSTEM_GAMEBOY_SOURCES}
)

target_sources("${PROJECT_NAME}" PRIVATE ${SHIMS_HEADERS})
target_sources("${PROJECT_NAME}" PRIVATE ${FRONTEND_HEADERS}          ${FRONTEND_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${COMPONENTS_HEADERS}        ${COMPONENTS_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${UTILITIES_HEADERS}         ${UTILITIES_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${SERVICES_HEADERS}          ${SERVICES_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${SYSTEMS_HEADERS}           ${SYSTEMS_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${SYSTEM_CHIP8_HEADERS}      ${SYSTEM_CHIP8_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${SYSTEM_BYTEPUSHER_HEADERS} ${SYSTEM_BYTEPUSHER_SOURCES})
target_sources("${PROJECT_NAME}" PRIVATE ${SYSTEM_GAMEBOY_HEADERS}    ${SYSTEM_GAMEBOY_SOURCES})

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)
target_compile_definitions("${PROJECT_NAME}" PRIVATE FMT_HEADER_ONLY)

target_include_directories(
	"${PROJECT_NAME}" PRIVATE
	"${PROJECT_INCLUDE_DIR}/shims"
	"${PROJECT_INCLUDE_DIR}/utilities"
	"${PROJECT_INCLUDE_DIR}/components"
	"${PROJECT_INCLUDE_DIR}/services"
	"${PROJECT_INCLUDE_DIR}/systems"
	"${PROJECT_INCLUDE_DIR}/frontend"
	"${PROJECT_VENDOR_DIR}/jthread" # hardcoded
)

target_link_libraries(
	"${PROJECT_NAME}" PRIVATE
	max0x7ba::atomic_queue
	tomlplusplus::tomlplusplus
	nlohmann_json::nlohmann_json
	cxxopts::cxxopts
	tl::expected
	fmt::fmt
	imgui
)

# ==================================================================================== #

if(WIN32)

	if (CMAKE_SYSTEM_VERSION VERSION_LESS "10.0.22000.0")
		message(WARNING "Windows SDK version is lower than 22000. Some features may be disabled.")
		target_compile_definitions("${PROJECT_NAME}" PRIVATE OLD_WINDOWS_SDK)
	endif()

	target_link_libraries(
		"${PROJECT_NAME}"
		PRIVATE
		"Dwmapi.lib"
	)

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
		$<$<CONFIG:Release>: /W4 /MP /utf-8 /O2 /Ob2 /Oi /Ot /Zi /GT /GL>
		$<$<CONFIG:Debug>: /W4 /MP /utf-8 /Od /Zi /RTC1>
	)

	target_link_options(
		"${PROJECT_NAME}" PRIVATE
		$<$<CONFIG:Release>: /LTCG /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /OPT:ICF /DEBUG>
		$<$<CONFIG:Debug>: /SUBSYSTEM:CONSOLE /INCREMENTAL /DEBUG>
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
