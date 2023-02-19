# https://github.com/apache/httpd/commit/dd4561dd17a669a8c1757ada0ca875dfa840d0e7
# https://github.com/apache/httpd/pull/343

set(cmakelists ${apache_source}/CMakeLists.txt)

file(READ ${cmakelists} content)

string(REPLACE
	"pcre2-8d.lib"
	"pcre2-8-staticd.lib"
	content "${content}"
)

string(REPLACE
	"pcre2-8.lib"
	"pcre2-8-static.lib"
	content "${content}"
)

string(REPLACE
	"\"-DHAVE_PCRE2\""
	"\"-DHAVE_PCRE2 -DPCRE2_STATIC\""
	content "${content}"
)

file(WRITE ${cmakelists} "${content}")
