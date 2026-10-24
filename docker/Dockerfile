# syntax=docker/dockerfile:1
# Staged, BuildKit-cached dev image. Stages go from the least to the most frequently changing input:
#   base       Ubuntu + apt packages + dev user (changes with the package list / UID / GID)
#   toolchain  mise + the tools pinned in mise.toml (changes with mise.toml)
#   vcpkg      vcpkg at the builtin-baseline of vcpkg.json (independent of toolchain)
#   playwright Chromium for the Playwright version pinned in e2e/package.json
#   dev        final image: playwright + vcpkg + runtime environment

# ---------------------------------------------------------------- base
FROM ubuntu:26.04 AS base

ARG DEBIAN_FRONTEND=noninteractive
ARG UID=1000
ARG GID=1000

# Keep downloaded .debs in the cache mount: the stock docker-clean hook would delete them.
RUN rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache

# git/curl/...: fetching; binutils: CMake finds ar/ranlib/nm next to the system toolchain, not next
# to the conda clang; make/perl/autotools/...: what vcpkg ports (openssl, libuuid, ...) need to
# build from source; libstdc++-15-dev: the C++ standard library clang compiles against.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        git curl ca-certificates zip unzip tar pkg-config python3 libstdc++-15-dev \
        binutils make perl linux-libc-dev autoconf automake libtool

# Dev user matching the host UID/GID (drop any stock user/group already holding them).
RUN (getent passwd "${UID}" | cut -d: -f1 | xargs -r userdel -r || true) \
    && (getent group "${GID}" | cut -d: -f1 | xargs -r groupdel || true) \
    && groupadd -g "${GID}" dev \
    && useradd -m -u "${UID}" -g "${GID}" -s /bin/bash dev \
    && mkdir -p /home/dev/.cache/vcpkg/archives /home/dev/.bun/install/cache \
    && chown -R dev:dev /home/dev

# ---------------------------------------------------------------- toolchain
FROM base AS toolchain

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
COPY e2e/package.json /tmp/e2e/package.json
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    --mount=type=cache,target=/root/.bun/install/cache \
    V="$(python3 -c 'import json; print(json.load(open("/tmp/e2e/package.json"))["devDependencies"]["@playwright/test"])')" \
    && case "$V" in [0-9]*) ;; *) echo "pin an exact @playwright/test version" >&2; exit 1;; esac \
    && apt-get update \
    && mkdir -p /opt/playwright-tmp && cd /opt/playwright-tmp \
    && bunx playwright@"$V" install --with-deps chromium \
    && cd / && rm -rf /opt/playwright-tmp /tmp/e2e && chmod -R a+rX /opt/playwright

# ---------------------------------------------------------------- dev
FROM playwright AS dev

COPY --from=vcpkg --chown=dev:dev /opt/vcpkg /opt/vcpkg

ENV CC=clang CXX=clang++ VCPKG_ROOT=/opt/vcpkg

USER dev
WORKDIR /src
