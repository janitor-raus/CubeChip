
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "Build package files as well." FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree." FORCE)

fetch_and_vendor(
    "https://github.com/TartanLlama/expected.git"
    "v1.2.0" TRUE
    "${PROJECT_VENDOR_DIR}/tl-expected" TRUE
)
