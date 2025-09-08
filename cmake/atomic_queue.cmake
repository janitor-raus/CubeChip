
set(ATOMIC_QUEUE_BUILD_TESTS OFF CACHE BOOL "Build atomic_queue tests" FORCE)
set(ATOMIC_QUEUE_BUILD_EXAMPLES OFF CACHE BOOL "Build atomic_queue examples" FORCE)

fetch_and_vendor(
	"https://github.com/max0x7ba/atomic_queue.git"
	"de8d18338ec1ad7493137348d53e770b066a6150" FALSE
	"${PROJECT_VENDOR_DIR}/atomic_queue" TRUE
)
