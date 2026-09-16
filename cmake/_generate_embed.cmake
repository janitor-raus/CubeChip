
function(embed_binary_file input_file output_data_file hexdump_script)
    add_custom_command(
        OUTPUT "${output_data_file}"
        COMMAND ${CMAKE_COMMAND}
                -DINPUT_FILE=${input_file}
                -DOUTPUT_FILE=${output_data_file}
                -P "${hexdump_script}"
        DEPENDS "${input_file}" "${hexdump_script}"
        COMMENT "Embedding '${input_file}'"
        VERBATIM
    )
endfunction()
