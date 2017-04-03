function(add_test_command_output NAME COMMAND ARGS OUTPUT_GENERATED OUTPUT_EXPECTED)
	MESSAGE(STATUS "NAME: ${NAME}, COMMAND: ${COMMAND}, OUTPUT_GENERATED: ${OUTPUT_GENERATED}, OUTPUT_EXPECTED: ${OUTPUT_EXPECTED}")
	add_test(NAME "${NAME}"
		COMMAND ${CMAKE_COMMAND}
			-Dtest_cmd=${COMMAND}
			-Dtest_args=${ARGS}
			-Doutput=${OUTPUT_GENERATED}
			-Dexpected=${OUTPUT_EXPECTED}
			-P ${CMAKE_SOURCE_DIR}/tests/command_output_check.cmake
		)
endfunction(add_test_command_output)

