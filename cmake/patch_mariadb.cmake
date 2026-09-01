# https://jira.mariadb.org/browse/CONC-174
set(cmakelists ${mariadb_source}/CMakeLists.txt)

file(READ ${cmakelists} content)

# do not use replace /MD with /MT
string(REPLACE
	"STRING(REPLACE \"/MD\" \"/MT\" COMPILER_FLAGS \${COMPILER_FLAGS})"
	"# STRING(REPLACE \"/MD\" \"/MT\" COMPILER_FLAGS \${COMPILER_FLAGS})"
	content "${content}"
)

file(WRITE ${cmakelists} "${content}")

# https://jira.mariadb.org/browse/CONC-764
if(${processor} STREQUAL "arm64")
	set(ma_context ${mariadb_source}/libmariadb/ma_context.c)

	file(READ ${ma_context} content)

	string(REPLACE
		"\"x18\", "
		""
		content "${content}"
	)

	file(WRITE ${ma_context} "${content}")
endif()
