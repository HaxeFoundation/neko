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

add_test(NAME build_cond_timed_wait.n
	COMMAND nekoc -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests/ ${CMAKE_SOURCE_DIR}/tests/cond_timed_wait.neko
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(build_cond_timed_wait.n PROPERTIES
  FIXTURES_SETUP cond_timed_wait
)

add_test(NAME run_cond_timed_wait.n
	COMMAND nekovm tests/cond_timed_wait.n
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_tests_properties(run_cond_timed_wait.n PROPERTIES
  PASS_REGULAR_EXPRESSION "timeout:false,true;held:false;nonpositive:false,false,false,false;huge:true,false,true;"
  FIXTURES_REQUIRED cond_timed_wait
  TIMEOUT 60
)
