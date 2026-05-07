CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

# ==================================================================================== #

function(are_patches_dirty LIB_NAME PATCH_BASE_DIR DEST_DIR OUT_VAR)
    set(${OUT_VAR} FALSE PARENT_SCOPE)
    set(PATCH_DIR "${PATCH_BASE_DIR}/${LIB_NAME}")
    if(NOT EXISTS "${PATCH_DIR}" OR NOT IS_DIRECTORY "${PATCH_DIR}")
        return()
    endif()
    file(GLOB PATCH_FILES "${PATCH_DIR}/*.patch")
    foreach(PATCH_FILE IN LISTS PATCH_FILES)
        get_filename_component(PATCH_NAME "${PATCH_FILE}" NAME)
        set(STAMP_FILE "${DEST_DIR}/.patch_applied_${PATCH_NAME}")
        if(NOT EXISTS "${STAMP_FILE}" OR "${PATCH_FILE}" IS_NEWER_THAN "${STAMP_FILE}")
            set(${OUT_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# ==================================================================================== #

function(apply_patches LIB_NAME PATCH_BASE_DIR DEST_DIR)
    set(PATCH_DIR "${PATCH_BASE_DIR}/${LIB_NAME}")
    if(NOT EXISTS "${PATCH_DIR}" OR NOT IS_DIRECTORY "${PATCH_DIR}")
        return()
    endif()

    file(GLOB PATCH_FILES "${PATCH_DIR}/*.patch")
    if(NOT PATCH_FILES)
        return()
    endif()

    file(GLOB OLD_STAMPS "${DEST_DIR}/.patch_applied_*")
    file(REMOVE ${OLD_STAMPS})
    list(LENGTH PATCH_FILES PATCH_COUNT)
    set(PATCH_FAIL_COUNT 0)
    
    foreach(PATCH_FILE IN LISTS PATCH_FILES)
        get_filename_component(PATCH_NAME "${PATCH_FILE}" NAME)
        message(STATUS "[${LIB_NAME}] Applying patch: ${PATCH_NAME}")

        execute_process(
            COMMAND git apply "${PATCH_FILE}"
            WORKING_DIRECTORY "${DEST_DIR}"
            RESULT_VARIABLE PATCH_RESULT
        )

        if(NOT PATCH_RESULT EQUAL 0)
            message(WARNING "[${LIB_NAME}] Failed to apply patch: ${PATCH_NAME}")
            math(EXPR PATCH_FAIL_COUNT "${PATCH_FAIL_COUNT} + 1")
        else()
            file(TOUCH "${DEST_DIR}/.patch_applied_${PATCH_NAME}")
        endif()
    endforeach()

    math(EXPR PATCH_OK_COUNT "${PATCH_COUNT} - ${PATCH_FAIL_COUNT}")
    if(PATCH_FAIL_COUNT EQUAL 0)
        message(STATUS "[${LIB_NAME}] All ${PATCH_COUNT} patches applied successfully.")
    else()
        message(WARNING "[${LIB_NAME}] ${PATCH_OK_COUNT}/${PATCH_COUNT} patches applied, ${PATCH_FAIL_COUNT} failed.")
    endif()
endfunction()


# ==================================================================================== #

function(fetch_and_vendor GIT_REPO GIT_TAG NEED_SHALLOW DEST_DIR ADD_SUBDIRECTORY_AT_DEST)

	get_filename_component(LIB_NAME "${DEST_DIR}" NAME)

	include(FetchContent)
	FetchContent_Declare(
		${LIB_NAME}
		GIT_REPOSITORY ${GIT_REPO}
		GIT_TAG        ${GIT_TAG}
		GIT_SHALLOW    ${NEED_SHALLOW}
		GIT_PROGRESS   TRUE
	)
	FetchContent_Populate(${LIB_NAME})
	FetchContent_GetProperties(${LIB_NAME})

	if(NOT ${LIB_NAME}_POPULATED)
		message(FATAL_ERROR "FetchContent failed for [${LIB_NAME}], cannot proceed with vendor copy.")
	endif()

    # get current commit of fetched source
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY "${${LIB_NAME}_SOURCE_DIR}"
        OUTPUT_VARIABLE FETCH_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # check vendor stamp
    set(VENDOR_STAMP "${DEST_DIR}/.vendor_stamp")
    set(VENDOR_COMMIT "")
    if(EXISTS "${VENDOR_STAMP}")
        file(READ "${VENDOR_STAMP}" VENDOR_COMMIT)
        string(STRIP "${VENDOR_COMMIT}" VENDOR_COMMIT)
    endif()

    # check if any patches are dirty independently of vendor stamp
    set(PATCHES_DIRTY FALSE)
    are_patches_dirty("${LIB_NAME}" "${CMAKE_SOURCE_DIR}/patches" "${DEST_DIR}" PATCHES_DIRTY)

    if(FETCH_COMMIT STREQUAL VENDOR_COMMIT AND NOT PATCHES_DIRTY)
        message(STATUS "[${LIB_NAME}] Vendor copy is up-to-date, skipping.")
    else()
        if(NOT FETCH_COMMIT STREQUAL VENDOR_COMMIT)
            message(STATUS "[${LIB_NAME}] Upstream changed, copying fetched content to: ${DEST_DIR}")
        else()
            message(STATUS "[${LIB_NAME}] Patches dirty, re-copying and re-patching: ${DEST_DIR}")
        endif()
        file(REMOVE_RECURSE "${DEST_DIR}")
        file(COPY "${${LIB_NAME}_SOURCE_DIR}/" DESTINATION "${DEST_DIR}")
        apply_patches("${LIB_NAME}" "${CMAKE_SOURCE_DIR}/patches" "${DEST_DIR}")
        file(WRITE "${VENDOR_STAMP}" "${FETCH_COMMIT}")
    endif()

	if (ADD_SUBDIRECTORY_AT_DEST)
		add_subdirectory("${DEST_DIR}" EXCLUDE_FROM_ALL)
	endif()

endfunction()
