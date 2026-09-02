file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests/)

add_test(NAME build_goto.n
	COMMAND nekoc -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests/ ${CMAKE_SOURCE_DIR}/tests/goto.neko
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_test(NAME run_goto.n
	COMMAND nekovm tests/goto.n
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(run_goto.n PROPERTIES
  PASS_REGULAR_EXPRESSION "startcontinue1continue2end"
  FIXTURES_SETUP build_goto.n
)
