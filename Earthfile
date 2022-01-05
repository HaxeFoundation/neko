VERSION 0.6
FROM ubuntu:bionic

ARG LINK_TYPE=static # or dynamic
ARG NEKO_VERSION

ARG DEVCONTAINER_IMAGE_NAME_DEFAULT=haxe/neko_devcontainer

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

vscode-dev-containers-scripts:
    FROM curlimages/curl:7.80.0
    WORKDIR /tmp
    RUN curl -fsSLO https://raw.githubusercontent.com/microsoft/vscode-dev-containers/main/script-library/common-debian.sh
    RUN curl -fsSLO https://raw.githubusercontent.com/microsoft/vscode-dev-containers/main/script-library/docker-debian.sh
    SAVE ARTIFACT --keep-ts *.sh AS LOCAL .devcontainer/library-scripts/

devcontainer-base:
    FROM mcr.microsoft.com/vscode/devcontainers/base:0-bionic
    ARG --required TARGETARCH

    # Avoid warnings by switching to noninteractive
    ENV DEBIAN_FRONTEND=noninteractive

    ARG INSTALL_ZSH="false"
    ARG UPGRADE_PACKAGES="true"
    ARG ENABLE_NONROOT_DOCKER="true"
    ARG USE_MOBY="false" # moby-buildx is missing in bionic
    COPY .devcontainer/library-scripts/*.sh /tmp/library-scripts/
    RUN apt-get update \
        && /bin/bash /tmp/library-scripts/common-debian.sh "${INSTALL_ZSH}" "${USERNAME}" "${USER_UID}" "${USER_GID}" "${UPGRADE_PACKAGES}" "true" "true" \
        # Use Docker script from script library to set things up
        && /bin/bash /tmp/library-scripts/docker-debian.sh "${ENABLE_NONROOT_DOCKER}" "/var/run/docker-host.sock" "/var/run/docker.sock" "${USERNAME}" "${USE_MOBY}" \
        # Clean up
        && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/* /tmp/library-scripts/

    # Configure apt and install packages
    RUN apt-get update \
        && apt-get install -y --no-install-recommends apt-utils dialog 2>&1 \
        && apt-get install -y \
            iproute2 \
            procps \
            sudo \
            bash-completion \
            build-essential \
            curl \
            wget \
            software-properties-common \
            direnv \
            tzdata \
            python3-pip \
            # Neko deps
            cmake \
            ninja-build \
            pkg-config \
            libgtk2.0-dev \
            # Neko dynamic link deps
            libgc-dev \
            libpcre3-dev \
            zlib1g-dev \
            apache2-dev \
            libmysqlclient-dev \
            libsqlite3-dev \
            libmbedtls-dev \
        #
        # Clean up
        && apt-get autoremove -y \
        && apt-get clean -y \
        && rm -rf /var/lib/apt/lists/*

    # Switch back to dialog for any ad-hoc use of apt-get
    ENV DEBIAN_FRONTEND=

    # Setting the ENTRYPOINT to docker-init.sh will configure non-root access
    # to the Docker socket. The script will also execute CMD as needed.
    ENTRYPOINT [ "/usr/local/share/docker-init.sh" ]
    CMD [ "sleep", "infinity" ]

    # VS Code workspace
    RUN mkdir -m 777 "/workspace"
    WORKDIR /workspace

# Usage:
# COPY +earthly/earthly /usr/local/bin/
# RUN earthly bootstrap --no-buildkit --with-autocomplete
earthly:
    FROM +devcontainer-base
    ARG --required TARGETARCH
    RUN curl -fsSL https://github.com/earthly/earthly/releases/download/v0.6.2/earthly-linux-${TARGETARCH} -o /usr/local/bin/earthly \
        && chmod +x /usr/local/bin/earthly
    SAVE ARTIFACT /usr/local/bin/earthly

devcontainer:
    FROM +devcontainer-base

    # Install earthly
    COPY +earthly/earthly /usr/local/bin/
    RUN earthly bootstrap --no-buildkit --with-autocomplete

    USER $USERNAME

    # Config direnv
    COPY --chown=$USER_UID:$USER_GID .devcontainer/direnv.toml /home/$USERNAME/.config/direnv/config.toml

    # Config bash
    RUN echo 'eval "$(direnv hook bash)"' >> ~/.bashrc

    USER root

    ARG DEVCONTAINER_IMAGE_NAME="$DEVCONTAINER_IMAGE_NAME_DEFAULT"
    ARG DEVCONTAINER_IMAGE_TAG=latest
    SAVE IMAGE --push "$DEVCONTAINER_IMAGE_NAME:$DEVCONTAINER_IMAGE_TAG" "$DEVCONTAINER_IMAGE_NAME:latest"

devcontainer-rebuild:
    RUN --no-cache date +%Y%m%d%H%M%S | tee buildtime
    ARG DEVCONTAINER_IMAGE_NAME="$DEVCONTAINER_IMAGE_NAME_DEFAULT"
    BUILD \
        --platform=linux/amd64 \
        --platform=linux/arm64 \
        +devcontainer \
        --DEVCONTAINER_IMAGE_NAME="$DEVCONTAINER_IMAGE_NAME" \
        --DEVCONTAINER_IMAGE_TAG="$(cat buildtime)"
    BUILD +devcontainer-update-refs \
        --DEVCONTAINER_IMAGE_NAME="$DEVCONTAINER_IMAGE_NAME" \
        --DEVCONTAINER_IMAGE_TAG="$(cat buildtime)"

devcontainer-update-refs:
    ARG --required DEVCONTAINER_IMAGE_NAME
    ARG --required DEVCONTAINER_IMAGE_TAG
    BUILD +devcontainer-update-ref \
        --DEVCONTAINER_IMAGE_NAME="$DEVCONTAINER_IMAGE_NAME" \
        --DEVCONTAINER_IMAGE_TAG="$DEVCONTAINER_IMAGE_TAG" \
        --FILE='.devcontainer/docker-compose.yml'

devcontainer-update-ref:
    ARG --required DEVCONTAINER_IMAGE_NAME
    ARG --required DEVCONTAINER_IMAGE_TAG
    ARG --required FILE
    COPY "$FILE" file.src
    RUN sed -e "s#$DEVCONTAINER_IMAGE_NAME:[a-z0-9]*#$DEVCONTAINER_IMAGE_NAME:$DEVCONTAINER_IMAGE_TAG#g" file.src > file.out
    SAVE ARTIFACT --keep-ts file.out $FILE AS LOCAL $FILE

build-multiarch:
    BUILD --platform=linux/amd64 --platform=linux/arm64 +build

install-dependencies:
    ENV APT_PACKAGES=wget cmake ninja-build pkg-config libgtk2.0-dev libgc-dev libpcre3-dev zlib1g-dev apache2-dev libmysqlclient-dev libsqlite3-dev git

    RUN set -ex && \
        apt-get update -qqy && \
        apt-get install -qqy $APT_PACKAGES

    RUN set -ex && \
        mkdir ~/mbedtls && \
        cd ~/mbedtls && \
        wget https://tls.mbed.org/download/mbedtls-2.2.1-apache.tgz && \
        tar xzf mbedtls-2.2.1-apache.tgz && \
        cd mbedtls-2.2.1 && sed -i "s/\/\/#define MBEDTLS_THREADING_PTHREAD/#define MBEDTLS_THREADING_PTHREAD/; s/\/\/#define MBEDTLS_THREADING_C/#define MBEDTLS_THREADING_C/; s/#define MBEDTLS_SSL_PROTO_SSL3/\/\/#define MBEDTLS_SSL_PROTO_SSL3/" include/mbedtls/config.h && \
        SHARED=1 make lib && \
        make install

build:
    FROM +install-dependencies

    ENV CMAKE_BUILD_TYPE=RelWithDebInfo
    ARG TARGETPLATFORM

    # validate args
    IF [ "$LINK_TYPE" != "static" ] && [ "$LINK_TYPE" != "dynamic" ]
        RUN exit 1
    END

    IF [ "$LINK_TYPE" = "static" ]
        ENV STATIC_DEPS=all
    ELSE
        ENV STATIC_DEPS=none
    END

    COPY . .

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

    IF [ "$(./bin/neko -version)" = "$NEKO_VERSION" ]
        SAVE ARTIFACT ./bin/* AS LOCAL bin/$LINK_TYPE/$TARGETPLATFORM/
    ELSE
        RUN exit 1
    END