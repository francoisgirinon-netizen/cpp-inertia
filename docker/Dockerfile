# syntax=docker/dockerfile:1
# Staged, BuildKit-cached dev image. Stages go from the least to the most frequently changing input:
#   base       Ubuntu + apt packages + dev user (changes with the package list / UID / GID)
#   toolchain  mise + the tools pinned in mise.toml (changes with mise.toml)
#   vcpkg      vcpkg at the builtin-baseline of vcpkg.json (independent of toolchain)
#   playwright Chromium for the Playwright version pinned in e2e/package.json
#   dev        final image: playwright + vcpkg + runtime environment

# ---------------------------------------------------------------- base
FROM ubuntu:26.04 AS base

# A pipeline fails when any of its commands fails, not only the last one.
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG DEBIAN_FRONTEND=noninteractive
ARG UID=1000
ARG GID=1000

# Dated Ubuntu snapshot every apt source is pointed at, so the package versions pinned below are
# reproducible. To bump: choose a new timestamp, look up the candidate of each package in that
# snapshot (`apt-cache policy <pkg>`), update the pins below, and change this value in the same commit.
ARG UBUNTU_SNAPSHOT=20261024T000000Z
# Version of ca-certificates the snapshot holds (see the base RUN below).
ARG CA_CERTIFICATES_VERSION=20260816ubuntu0.26.04.1

# Keep downloaded .debs in the cache mount: the stock docker-clean hook would delete them.
RUN rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

# The base image has no CA bundle and the snapshot only answers over HTTPS, so ca-certificates is first
# installed from the plain-HTTP archive, at the version the snapshot holds (the archive keeps only the
# current version: if this pin fails, the snapshot and the archive have diverged, bump both).
# The snapshot's Release files outlive their Valid-Until date, hence Check-Valid-Until is off, but only
# for the apt-get update calls that read the snapshot (-o on the command line, no global config file);
# their signatures are still verified.
#
# git/curl/...: fetching; binutils: CMake finds ar/ranlib/nm next to the system toolchain, not next
# to the conda clang; make/perl/autotools/...: what vcpkg ports (openssl, libuuid, ...) need to
# build from source; libstdc++-15-dev: the C++ standard library clang compiles against.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates=${CA_CERTIFICATES_VERSION} \
    && sed -i "s|^URIs: .*|URIs: https://snapshot.ubuntu.com/ubuntu/${UBUNTU_SNAPSHOT}/|" \
        /etc/apt/sources.list.d/ubuntu.sources \
    && grep -q "^URIs: https://snapshot.ubuntu.com/ubuntu/${UBUNTU_SNAPSHOT}/\$" /etc/apt/sources.list.d/ubuntu.sources \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends \
        ca-certificates=${CA_CERTIFICATES_VERSION} \
        git=1:2.53.0-1ubuntu1.1 \
        curl=8.18.0-1ubuntu2.8 \
        zip=3.0-15ubuntu3 \
        unzip=6.0-29ubuntu1 \
        tar=1.35+dfsg-4ubuntu0.4 \
        pkg-config=2.5.1-4 \
        python3=3.14.3-0ubuntu2 \
        libstdc++-15-dev=15.2.0-16ubuntu1 \
        binutils=2.46-3ubuntu2 \
        make=4.4.1-3 \
        perl=5.40.1-7ubuntu0.3 \
        linux-libc-dev=7.0.0-38.38 \
        autoconf=2.72-3.1ubuntu2 \
        automake=1:1.18.1-3build1 \
        libtool=2.5.4-9

# Dev user matching the host UID/GID (drop any stock user/group already holding them).
RUN (getent passwd "${UID}" | cut -d: -f1 | xargs -r userdel -r || true) \
    && (getent group "${GID}" | cut -d: -f1 | xargs -r groupdel || true) \
    && groupadd -g "${GID}" dev \
    && useradd -l -m -u "${UID}" -g "${GID}" -s /bin/bash dev \
    && mkdir -p /home/dev/.cache/vcpkg/archives /home/dev/.bun/install/cache \
    && chown -R dev:dev /home/dev

# ---------------------------------------------------------------- toolchain
FROM base AS toolchain

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# mise, installed system-wide so any UID can use it. Pinned release binary, checksum verified
# (SHASUMS256.txt of the release).
ARG MISE_VERSION=v2026.10.17
ARG MISE_SHA256=fe6dd17e68e09682b31d2e7bb7bb755d22636ac1af5d94621e300f2ca9e9fe8a
RUN curl -fsSL -o /usr/local/bin/mise \
        "https://github.com/jdx/mise/releases/download/${MISE_VERSION}/mise-${MISE_VERSION}-linux-x64" \
    && echo "${MISE_SHA256}  /usr/local/bin/mise" | sha256sum -c - \
    && chmod +x /usr/local/bin/mise

ENV MISE_DATA_DIR=/opt/mise \
    MISE_CONFIG_DIR=/opt/mise/config \
    MISE_YES=1 \
    PATH=/opt/mise/shims:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin

# Only mise.toml invalidates this stage.
COPY mise.toml /opt/mise/config/config.toml

# conda-forge clang ships neither a clang++ driver nor a configuration that finds Ubuntu's
# libstdc++: add the driver name (mise does not shim it, hence the /usr/local/bin link) and point
# its config at the system GCC 15 install and sysroot. clang-tidy (conda:clang-tools) also needs the
# compiler's builtin headers (stddef.h, ...).
ARG CLANG_VERSION=23.1.4
RUN --mount=type=cache,target=/root/.cache/mise \
    --mount=type=cache,target=/opt/mise/conda-packages \
    mise install \
    && B=/opt/mise/installs/clang/${CLANG_VERSION}/bin \
    && ln -s clang-23 "$B/clang++" \
    && printf -- '--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/15\n--sysroot=/\n' \
        > "$B/x86_64-conda-linux-gnu-clang.cfg" \
    && cp "$B/x86_64-conda-linux-gnu-clang.cfg" "$B/x86_64-conda-linux-gnu-clang++.cfg" \
    && ln -s "$B/clang++" /usr/local/bin/clang++ \
    && ln -s /opt/mise/installs/clang/${CLANG_VERSION}/lib/clang \
        /opt/mise/installs/conda-clang-tools/${CLANG_VERSION}/lib/clang \
    && mise reshim && chmod -R a+rX /opt/mise

# ---------------------------------------------------------------- vcpkg
FROM base AS vcpkg

# Check out the builtin-baseline pinned in vcpkg.json (manifests resolve against the image's vcpkg,
# so a plain clone of HEAD would drift from it and installs would fail). vcpkg verifies the SHA-512
# of the vcpkg tool it downloads while bootstrapping.
# Only vcpkg.json invalidates the clone: the SHA is read from it, never duplicated here.
COPY vcpkg.json /tmp/vcpkg.json
RUN BASELINE="$(python3 -c 'import json; print(json.load(open("/tmp/vcpkg.json"))["builtin-baseline"])')" \
    && test -n "$BASELINE" \
    && git init -q /opt/vcpkg \
    && git -C /opt/vcpkg remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C /opt/vcpkg fetch -q --depth 1 origin "$BASELINE" \
    && git -C /opt/vcpkg checkout -q FETCH_HEAD
RUN --mount=type=cache,target=/root/.cache/vcpkg-downloads \
    VCPKG_DOWNLOADS=/root/.cache/vcpkg-downloads /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# ---------------------------------------------------------------- playwright
FROM toolchain AS playwright

# Chromium + its system libraries, shared by all users. The Playwright version is read from
# e2e/package.json (@playwright/test must be an exact version), the single source of truth.
ENV PLAYWRIGHT_BROWSERS_PATH=/opt/playwright
# --with-deps installs Chromium's system libraries; they resolve from the pinned Ubuntu snapshot
# (UBUNTU_SNAPSHOT, set in base), not the live archive.
COPY e2e/package.json /tmp/e2e/package.json
WORKDIR /opt/playwright-tmp
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    --mount=type=cache,target=/root/.bun/install/cache \
    V="$(python3 -c 'import json; print(json.load(open("/tmp/e2e/package.json"))["devDependencies"]["@playwright/test"])')" \
    && case "$V" in [0-9]*) ;; *) echo "pin an exact @playwright/test version" >&2; exit 1;; esac \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && bunx playwright@"$V" install --with-deps chromium \
    && rm -rf /opt/playwright-tmp /tmp/e2e && chmod -R a+rX /opt/playwright
WORKDIR /

# ---------------------------------------------------------------- dev
FROM playwright AS dev

ARG UID=1000
ARG GID=1000

COPY --from=vcpkg --chown=dev:dev /opt/vcpkg /opt/vcpkg

ENV CC=clang CXX=clang++ VCPKG_ROOT=/opt/vcpkg

# Mount point of the compiler cache volume (a named volume copies the owner of this directory).
RUN mkdir -p /home/dev/.cache/sccache && chown dev:dev /home/dev/.cache/sccache

# Numeric on purpose: UID/GID come from the build args, so bind-mounted files keep the host owner.
USER ${UID}:${GID}
WORKDIR /src
