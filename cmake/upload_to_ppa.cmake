find_package(Git REQUIRED)

set(neko_debian_dir ${bin_dir}/neko-debian)

# format SNAPSHOT_VERSION
execute_process(
	COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
	OUTPUT_VARIABLE COMMIT_SHA
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND ${GIT_EXECUTABLE} show -s --format=%ct HEAD
	OUTPUT_VARIABLE COMMIT_TIME
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND date -u -d @${COMMIT_TIME} +%Y%m%d%H%M%S
	OUTPUT_VARIABLE COMMIT_TIME
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(SNAPSHOT_VERSION ${NEKO_VERSION}+1SNAPSHOT${COMMIT_TIME}+${COMMIT_SHA})
message(STATUS "building source package version ${SNAPSHOT_VERSION}")

message(STATUS "setting up neko-debian repo")
if (EXISTS ${neko_debian_dir})
	execute_process(
		COMMAND ${GIT_EXECUTABLE} fetch --all
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} clean -fx
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} reset --hard HEAD
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} tag -d upstream/${SNAPSHOT_VERSION}
		WORKING_DIRECTORY ${neko_debian_dir}
	)
else()
	execute_process(
		COMMAND ${GIT_EXECUTABLE} clone https://github.com/HaxeFoundation/neko-debian.git ${neko_debian_dir}
	)
endif()
foreach(branch upstream next)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} checkout ${branch}
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} reset --hard origin/${branch}
		WORKING_DIRECTORY ${neko_debian_dir}
	)
endforeach()

message(STATUS "import changes from source archive to neko-debian")
get_filename_component(source_archive_name ${source_archive} NAME)
file(COPY ${source_archive} DESTINATION ${bin_dir})
file(RENAME ${bin_dir}/${source_archive_name} ${bin_dir}/neko_${SNAPSHOT_VERSION}.orig.tar.gz)
execute_process(
	COMMAND gbp import-orig ${bin_dir}/neko_${SNAPSHOT_VERSION}.orig.tar.gz -u ${SNAPSHOT_VERSION} --debian-branch=next
	WORKING_DIRECTORY ${neko_debian_dir}
)

set(distros
	trusty
	xenial
	zesty
	artful
	bionic
)

if (DEFINED ENV{PPA})
	set(PPA $ENV{PPA})
else()
	set(PPA "ppa:haxe/snapshots")
endif()

foreach(distro ${distros})
	message(STATUS "backporting to ${distro} and will upload to ${PPA}")
	execute_process(
		COMMAND ${GIT_EXECUTABLE} checkout .
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} checkout next-${distro}
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} clean -fx
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} reset --hard origin/next-${distro}
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} merge next -m "merge"
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND dch -v "${SNAPSHOT_VERSION}-1" --urgency low "snapshot build"
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND debuild -S -sa
		WORKING_DIRECTORY ${neko_debian_dir}
	)
	execute_process(
		COMMAND backportpackage -d ${distro} --upload ${PPA} --yes neko_${SNAPSHOT_VERSION}-1.dsc
		WORKING_DIRECTORY ${bin_dir}
	)
endforeach()