
file(READ "${INPUT_FILE}" hex_content HEX)
string(REGEX REPLACE "(..)" "0x\\1," formatted_hex "${hex_content}")
string(REGEX REPLACE ",$" "" formatted_hex "${formatted_hex}")  # strip trailing comma
file(WRITE "${OUTPUT_FILE}" "${formatted_hex}")
