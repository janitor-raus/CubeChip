
set(FMT_INSTALL OFF CACHE BOOL "Generate the install target." FORCE)

fetch_and_vendor(
    "https://github.com/fmtlib/fmt.git"
    "12.1.0" TRUE
    "${PROJECT_VENDOR_DIR}/fmt" TRUE
)
