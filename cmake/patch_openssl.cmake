set(configure ${openssl_source}/Configure)

file(READ ${configure} content)

# Make it possible to disable building and running tests
# https://github.com/openssl/openssl/pull/1514/files
string(REPLACE 
	"my @disablables = \("
	"my @disablables = \( \"tests\","
	content "${content}"
)


file(WRITE ${configure} "${content}")