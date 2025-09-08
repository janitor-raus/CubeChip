
set(VENDOR_DIR "${PROJECT_VENDOR_DIR}/imgui")

fetch_and_vendor(
	"https://github.com/ocornut/imgui.git"
	"docking" TRUE
	"${VENDOR_DIR}" FALSE
)

if (NOT TARGET imgui)
	add_library(imgui
		STATIC EXCLUDE_FROM_ALL
		"${VENDOR_DIR}/imgui.cpp"
		"${VENDOR_DIR}/imgui_demo.cpp"
		"${VENDOR_DIR}/imgui_draw.cpp"
		"${VENDOR_DIR}/imgui_tables.cpp"
		"${VENDOR_DIR}/imgui_widgets.cpp"
		"${VENDOR_DIR}/misc/cpp/imgui_stdlib.cpp"
		"${VENDOR_DIR}/backends/imgui_impl_sdl3.cpp"
		"${VENDOR_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
	)
endif()

target_include_directories(imgui
	PUBLIC
	"${VENDOR_DIR}"
	"${VENDOR_DIR}/backends"
	"${VENDOR_DIR}/misc/cpp"
	PRIVATE
	"${SDL3_INCLUDE_DIRS}"
)

target_link_libraries(
	imgui PUBLIC
	SDL3::SDL3
)
