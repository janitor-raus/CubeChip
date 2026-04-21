CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

# ==================================================================================== #

function(hash_directory DIR OUT_VAR)

    file(GLOB_RECURSE FILES RELATIVE "${DIR}" "${DIR}/*")
    set(HASH_INPUT "")
    foreach(F IN LISTS FILES)
        file(SHA256 "${DIR}/${F}" FILE_HASH)
        string(APPEND HASH_INPUT "${F}:${FILE_HASH}\n")
    endforeach()
    string(SHA256 FINAL_HASH "${HASH_INPUT}")
    set(${OUT_VAR} "${FINAL_HASH}" PARENT_SCOPE)

endfunction()

# ==================================================================================== #

function(apply_patches LIB_NAME PATCH_BASE_DIR)
    set(PATCH_DIR "${PATCH_BASE_DIR}/${LIB_NAME}")

    if(NOT EXISTS "${PATCH_DIR}" OR NOT IS_DIRECTORY "${PATCH_DIR}")
        return()
    endif()
	
    file(GLOB PATCH_FILES "${PATCH_DIR}/*.patch")
    if(NOT PATCH_FILES)
        return()
    endif()

    # determine if any patch is new or modified since its stamp
    set(NEEDS_APPLY FALSE)
    foreach(PATCH_FILE IN LISTS PATCH_FILES)
        get_filename_component(PATCH_NAME "${PATCH_FILE}" NAME)
        set(STAMP_FILE "${${LIB_NAME}_SOURCE_DIR}/.patch_applied_${PATCH_NAME}")
        if(NOT EXISTS "${STAMP_FILE}" OR "${PATCH_FILE}" IS_NEWER_THAN "${STAMP_FILE}")
            set(NEEDS_APPLY TRUE)
            break()
        endif()
    endforeach()

    if(NOT NEEDS_APPLY)
        message(STATUS "[${LIB_NAME}] All patches up-to-date, skipping.")
        return()
    endif()

    # wipe all stamps and reset the source tree to pristine
    message(STATUS "[${LIB_NAME}] Patch state dirty, resetting source tree.")
    file(GLOB OLD_STAMPS "${${LIB_NAME}_SOURCE_DIR}/.patch_applied_*")
    file(REMOVE ${OLD_STAMPS})
    execute_process(
        COMMAND git checkout -- .
        WORKING_DIRECTORY "${${LIB_NAME}_SOURCE_DIR}"
        RESULT_VARIABLE RESET_RESULT
    )
    if(NOT RESET_RESULT EQUAL 0)
        message(FATAL_ERROR "[${LIB_NAME}] Failed to reset source tree before patching.")
    endif()

    # apply all patches in order
    foreach(PATCH_FILE IN LISTS PATCH_FILES)
        get_filename_component(PATCH_NAME "${PATCH_FILE}" NAME)
        set(STAMP_FILE "${${LIB_NAME}_SOURCE_DIR}/.patch_applied_${PATCH_NAME}")
        message(STATUS "[${LIB_NAME}] Applying patch: ${PATCH_NAME}")
        execute_process(
            COMMAND git apply "${PATCH_FILE}"
            WORKING_DIRECTORY "${${LIB_NAME}_SOURCE_DIR}"
            RESULT_VARIABLE PATCH_RESULT
        )
        if(NOT PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR "[${LIB_NAME}] Failed to apply patch: ${PATCH_NAME}")
        endif()
        file(TOUCH "${STAMP_FILE}")
    endforeach()
    message(STATUS "[${LIB_NAME}] All patches applied successfully.")
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
		message(FATAL_ERROR "FetchContent failed for ${LIB_NAME}, cannot proceed with vendor copy.")
	endif()

    apply_patches("${LIB_NAME}" "${CMAKE_SOURCE_DIR}/patches")

	unset(FETCH_HASH)
	hash_directory("${${LIB_NAME}_SOURCE_DIR}" FETCH_HASH)
	unset(VENDOR_HASH)
	hash_directory("${DEST_DIR}" VENDOR_HASH)

	if(FETCH_HASH STREQUAL VENDOR_HASH)
		message(STATUS "Vendor copy for ${LIB_NAME} is up-to-date, skipping.")
	else()
		message(STATUS "Vendor copying fetched content ${LIB_NAME} to: ${DEST_DIR}")
		file(REMOVE_RECURSE "${DEST_DIR}")
		file(COPY "${${LIB_NAME}_SOURCE_DIR}/" DESTINATION "${DEST_DIR}")

		unset(NEW_HASH)
		hash_directory("${DEST_DIR}" NEW_HASH)
		if(NOT NEW_HASH STREQUAL FETCH_HASH)
			message(FATAL_ERROR "Vendor copy failed or incomplete for ${LIB_NAME}!")
		else()
			message(STATUS "Vendor copy successful.")
		endif()
	endif()

	if (ADD_SUBDIRECTORY_AT_DEST)
		add_subdirectory("${DEST_DIR}" EXCLUDE_FROM_ALL)
	endif()

endfunction()
