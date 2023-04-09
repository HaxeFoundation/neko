VERSION 0.6
FROM ubuntu:jammy

ARG DEVCONTAINER_IMAGE_NAME_DEFAULT=haxe/neko_devcontainer

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

ARG LINK_TYPE_DEFAULT=static # static or dynamic
ARG LINK_DYNAMIC_PACKAGES="libgc-dev libpcre2-dev zlib1g-dev apache2-dev libmysqlclient-dev libsqlite3-dev libmbedtls-dev"

vscode-dev-containers-scripts:
    FROM curlimages/curl:7.80.0
    WORKDIR /tmp
    RUN curl -fsSLO https://raw.githubusercontent.com/microsoft/vscode-dev-containers/main/script-library/common-debian.sh
    RUN curl -fsSLO https://raw.githubusercontent.com/microsoft/vscode-dev-containers/main/script-library/docker-debian.sh
    SAVE ARTIFACT --keep-ts *.sh AS LOCAL .devcontainer/library-scripts/

devcontainer-base:
    FROM mcr.microsoft.com/vscode/devcontainers/base:0-jammy
    ARG --required TARGETARCH

    # Avoid warnings by switching to noninteractive
    ENV DEBIAN_FRONTEND=noninteractive

    ARG INSTALL_ZSH="false"
    ARG UPGRADE_PACKAGES="true"
    ARG ENABLE_NONROOT_DOCKER="true"
    ARG USE_MOBY="true"
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
            libgtk-3-dev \
            $LINK_DYNAMIC_PACKAGES \
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
    RUN curl -fsSL https://github.com/earthly/earthly/releases/download/v0.6.30/earthly-linux-${TARGETARCH} -o /usr/local/bin/earthly \
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

build-env:
    # We specifically use an old distro to build against an old glibc.
    # https://repology.org/project/glibc/versions
    FROM ubuntu:xenial
    RUN apt-get update \
        && apt-get install -qqy --no-install-recommends \
            software-properties-common \
            curl \
            build-essential \
            autoconf \
            automake \
            libtool \
            file \
            git \
            ninja-build \
            pkg-config \
            libgtk-3-dev \
        #
        # Clean up
        && apt-get autoremove -y \
        && apt-get clean -y \
        && rm -rf /var/lib/apt/lists/*
    # install a recent CMake
    ARG --required TARGETARCH
    ARG CMAKE_VERSION=3.25.0
    RUN case "$TARGETARCH" in \
            amd64) curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh" -o cmake-install.sh;; \
            arm64) curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-aarch64.sh" -o cmake-install.sh;; \
            *) exit 1;; \
        esac \
        && sh cmake-install.sh --skip-license --prefix=/usr/local \
        && rm cmake-install.sh \
        && cmake --version | grep -q "$CMAKE_VERSION"
    ARG LINK_TYPE="$LINK_TYPE_DEFAULT" # static or dynamic
    RUN if [ "$LINK_TYPE" = "dynamic" ]; then \
        apt-get update && apt-get install -qqy --no-install-recommends $LINK_DYNAMIC_PACKAGES \
        #
        # Clean up
        && apt-get autoremove -y \
        && apt-get clean -y \
        && rm -rf /var/lib/apt/lists/* \
    ; fi

build:
    ARG LINK_TYPE="$LINK_TYPE_DEFAULT" # static or dynamic
    FROM +build-env --LINK_TYPE="$LINK_TYPE"
    WORKDIR /src
    COPY --dir boot cmake extra libs src vm .
    COPY CMakeLists.txt CHANGES LICENSE README.md .
    WORKDIR /src/build
    RUN case "$LINK_TYPE" in \
            static)  cmake .. -DSTATIC_DEPS=all  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo;; \
            dynamic) cmake .. -DSTATIC_DEPS=none -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo;; \
            *) exit 1;; \
        esac
    RUN if [ "$LINK_TYPE" = "static" ]; then ninja download_deps; fi
    RUN ninja
    RUN ninja test
    SAVE ARTIFACT bin/*

package:
    ARG LINK_TYPE="$LINK_TYPE_DEFAULT" # static or dynamic
    FROM +build --LINK_TYPE="$LINK_TYPE"
    RUN ninja package
    # ARG --required TARGETOS
    # ARG --required TARGETARCH
    # RUN mv bin/neko-*.tar.gz "bin/neko-$(cmake -L -N -B . | awk -F '=' '/NEKO_VERSION/ {print $2}')-${TARGETOS}-${TARGETARCH}.tar.gz"
    ARG --required TARGETPLATFORM
    SAVE ARTIFACT --keep-ts bin/neko-*.tar.gz AS LOCAL bin/$LINK_TYPE/$TARGETPLATFORM/

package-all-platforms:
    BUILD --platform=linux/amd64 --platform=linux/arm64 +package

extract-package:
    ARG LINK_TYPE="$LINK_TYPE_DEFAULT" # static or dynamic
    FROM +package --LINK_TYPE="$LINK_TYPE"
    WORKDIR bin
    RUN mkdir /tmp/neko && tar xf neko-*.tar.gz --strip-components 1 -C /tmp/neko
    SAVE ARTIFACT /tmp/neko neko

test-static-package:
    ARG IMAGE=ubuntu:xenial
    FROM $IMAGE
    WORKDIR /tmp/neko
    COPY +extract-package/neko .
    ARG PREFIX=/usr/local
    RUN mkdir -p $PREFIX/bin \
        && mv neko nekotools nekoc nekoml $PREFIX/bin \
        && mkdir -p $PREFIX/lib/neko \
        && mv *.ndll nekoml.std $PREFIX/lib/neko \
        && mv *.so* $PREFIX/lib \
        && rm -rf * \
        && ldconfig
    RUN neko -version
    RUN nekoc
    RUN nekoml
    RUN nekotools

test-static-package-all-platforms:
    ARG IMAGE=ubuntu:xenial
    BUILD --platform=linux/amd64 --platform=linux/arm64 +test-static-package --IMAGE="$IMAGE"
