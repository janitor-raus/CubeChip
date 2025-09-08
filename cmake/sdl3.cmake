
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3_test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build the test directory" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "Install test-cases" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build the examples directory" FORCE)

set(SDL_SHARED OFF CACHE BOOL "Build a shared version of the library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build a static version of the library" FORCE)

fetch_and_vendor(
	"https://github.com/libsdl-org/SDL.git"
	"release-3.2.18" TRUE
	"${PROJECT_VENDOR_DIR}/sdl3" TRUE
)
