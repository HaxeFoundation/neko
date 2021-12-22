VERSION 0.6
WORKDIR /neko-builder

build-all-platforms:
    BUILD --platform=linux/amd64 --platform=linux/arm64 +build

build:
    LOCALLY
    RUN ls -lah bin
    
    FROM ubuntu:18.04
    ARG TARGETPLATFORM
    ARG LINK_TYPE=static # or dynamic
    
    ENV CMAKE_BUILD_TYPE=RelWithDebInfo
    
    # validate args
    IF [ "$LINK_TYPE" != "static" ] && [ "$LINK_TYPE" != "dynamic" ]
        RUN exit 1
    END
    
    
    IF [ "$LINK_TYPE" = "static" ]
        ENV APT_PACKAGES=wget cmake ninja-build pkg-config libgtk2.0-dev git
        ENV STATIC_DEPS=all
    ELSE
        ENV APT_PACKAGES=wget cmake ninja-build pkg-config libgtk2.0-dev libgc-dev libpcre3-dev zlib1g-dev apache2-dev libmysqlclient-dev libsqlite3-dev git
        ENV STATIC_DEPS=none
    END
    
    RUN set -ex && \
        apt-get update -qqy && \
        apt-get install -qqy $APT_PACKAGES
        
    IF [ "$LINK_TYPE" = "dynamic" ]
        RUN set -ex && \
            CWD=$PWD && \
            mkdir ~/mbedtls && \
            cd ~/mbedtls && \
            wget https://tls.mbed.org/download/mbedtls-2.2.1-apache.tgz && \
            tar xzf mbedtls-2.2.1-apache.tgz && \
            cd mbedtls-2.2.1 && sed -i "s/\/\/#define MBEDTLS_THREADING_PTHREAD/#define MBEDTLS_THREADING_PTHREAD/; s/\/\/#define MBEDTLS_THREADING_C/#define MBEDTLS_THREADING_C/; s/#define MBEDTLS_SSL_PROTO_SSL3/\/\/#define MBEDTLS_SSL_PROTO_SSL3/" include/mbedtls/config.h && \
            SHARED=1 make lib && \
            make install && \
            cd $CWD
    END
    
    COPY --dir boot cmake libs src vm CMakeLists.txt CHANGES LICENSE README.md .
        
    RUN set -ex && \
        cmake . -G Ninja -DSTATIC_DEPS=$STATIC_DEPS -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE
        
    IF [ "$LINK_TYPE" = "static" ]
        RUN ninja download_static_deps || ninja download_static_deps || ninja download_static_deps
    END
    
    RUN ninja
    RUN ldd -v ./bin/neko && \
        ldd -v ./bin/nekoc && \
        ldd -v ./bin/nekoml && \
        ldd -v ./bin/nekotools
    RUN ctest --verbose
    RUN ninja package
    RUN ./bin/neko -version
    
    SAVE ARTIFACT ./bin/* AS LOCAL bin/$LINK_TYPE/$TARGETPLATFORM/