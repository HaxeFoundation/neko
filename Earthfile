VERSION 0.6
FROM ubuntu:18.04
WORKDIR /neko-builder

build-all-platforms:
    BUILD --platform=linux/amd64 --platform=linux/arm64 +build

build:
    ARG APT_PACKAGES=cmake ninja-build pkg-config libgtk2.0-dev git
    ARG STATIC_DEPS=all
    ARG CMAKE_BUILD_TYPE=RelWithDebInfo
    
    COPY . .
    
    RUN set -ex && \
        apt-get update -qqy && \
        apt-get install -qqy $APT_PACKAGES    
    RUN set -ex && \
        cmake . -G Ninja -DSTATIC_DEPS=$STATIC_DEPS -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE
    RUN ninja download_static_deps || ninja download_static_deps || ninja download_static_deps
    RUN ninja
    RUN ldd -v ./bin/neko && \
        ldd -v ./bin/nekoc && \
        ldd -v ./bin/nekoml && \
        ldd -v ./bin/nekotools
    RUN ctest --verbose
    RUN ninja package
    RUN ./bin/neko -version
    ARG TARGETPLATFORM
    SAVE ARTIFACT ./bin/* AS LOCAL bin/$TARGETPLATFORM/