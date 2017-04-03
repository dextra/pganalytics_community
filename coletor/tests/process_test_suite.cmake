separate_arguments(test_cmds)
file(WRITE ${output_file} "")
foreach(test_cmd ${test_cmds})
	message(STATUS "Processing tests for: ${test_cmd}")
	file(APPEND ${output_file} "# Tests for ${test_cmd}\n")
	execute_process(
		COMMAND ${binary_dir}/${test_cmd} --gtest_list_tests
		OUTPUT_VARIABLE contents
		)
	# This is the prefix used on add_test
	string(REGEX REPLACE " " "_" prefix ${test_cmd})   # replace spaces by _ (shouldn't have anyway)
	string(REGEX REPLACE "[/\\]" "." prefix ${prefix}) # replace directory separator by dot

	# Get all test groups as a list
	string(REGEX MATCHALL "\n[a-zA-Z_][a-zA-Z_0-9]*\\.(\n[ ]+[a-zA-Z_][a-zA-Z_0-9]*)+" contents "dummy\n${contents}")

	foreach(group ${contents})
		# Get the parent (with the trailing dot)
		string(REGEX MATCH "\n[a-zA-Z_][a-zA-Z_0-9]*\\." parent "${group}")
		# Get the children as a list
		string(REGEX MATCHALL "\n[ ]+[a-zA-Z_][a-zA-Z_0-9]*" children "${group}")
		# Remove spaces and line breaks
		string(REGEX REPLACE "\n" "" parent "${parent}")
		string(REGEX REPLACE "[\n ]" "" children "${children}")
		foreach(child ${children})
			file(APPEND ${output_file} "add_test(${prefix}.${parent}${child} ${binary_dir}/${test_cmd} --gtest_filter=${parent}${child})\n")
		endforeach(child ${children})
	endforeach(group ${contents})
endforeach(test_cmd ${test_cmds})

