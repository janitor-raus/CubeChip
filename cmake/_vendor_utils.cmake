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

	hash_directory("${${LIB_NAME}_SOURCE_DIR}" FETCH_HASH)
	hash_directory("${DEST_DIR}" VENDOR_HASH)

	if(FETCH_HASH STREQUAL VENDOR_HASH)
		message(STATUS "Vendor copy for ${LIB_NAME} is up-to-date, skipping.")
	else()
		message(STATUS "Vendor copying fetched content ${LIB_NAME} to: ${DEST_DIR}")
		file(REMOVE_RECURSE "${DEST_DIR}")
		file(COPY "${${LIB_NAME}_SOURCE_DIR}/" DESTINATION "${DEST_DIR}")

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
