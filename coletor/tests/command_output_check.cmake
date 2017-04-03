# Based on: http://www.cmake.org/pipermail/cmake/2009-July/030595.html

# some argument checking:
# test_cmd is the command to run with all its arguments
IF(NOT test_cmd)
	MESSAGE(FATAL_ERROR "Variable test_cmd not defined")
ELSEIF(NOT test_cmd)
# expected contains the name of the "blessed" output file
ELSEIF(NOT expected)
	MESSAGE(FATAL_ERROR "Variable expected not defined")
ELSEIF(NOT expected)
# output contains the name of the output file the test_cmd will produce
ELSEIF(NOT output)
	MESSAGE(FATAL_ERROR "Variable output not defined")
ENDIF(NOT test_cmd)

EXECUTE_PROCESS(
	COMMAND ${test_cmd} ${test_args}
	#COMMAND ${CMAKE_COMMAND} -E compare_files "${expected}" "${output}"
	RESULT_VARIABLE test_not_successful
	#OUTPUT_QUIET
	#ERROR_QUIET
	)

IF(test_not_successful)
	MESSAGE(SEND_ERROR "`${test_cmd}' failed with error: ${test_not_successful}")
ELSE(test_not_successful)
	EXECUTE_PROCESS(
		COMMAND ${CMAKE_COMMAND} -E compare_files "${expected}" "${output}"
		RESULT_VARIABLE test_not_equal
		OUTPUT_QUIET
		ERROR_QUIET
		)
	IF(test_not_equal)
		MESSAGE(SEND_ERROR "`${output}' does not match `${expected}'")
	ENDIF(test_not_equal)
ENDIF(test_not_successful)

