FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive
ARG UID=1000
ARG GID=1000

RUN apt-get update && apt-get install -y --no-install-recommends \
        git curl ca-certificates zip unzip tar pkg-config python3 libstdc++-15-dev \
    && rm -rf /var/lib/apt/lists/*

# Dev user matching the host UID/GID (drop any stock user/group already holding them).
RUN (getent passwd "${UID}" | cut -d: -f1 | xargs -r userdel -r || true) \
    && (getent group "${GID}" | cut -d: -f1 | xargs -r groupdel || true) \
    && groupadd -g "${GID}" dev \
    && useradd -m -u "${UID}" -g "${GID}" -s /bin/bash dev \
    && mkdir -p /home/dev/.cache/vcpkg /home/dev/.bun/install/cache \
    && chown -R dev:dev /home/dev

# mise + pinned toolchain (installed system-wide so any UID can use it).
ENV MISE_DATA_DIR=/opt/mise \
    MISE_CONFIG_DIR=/opt/mise/config \
    MISE_YES=1 \
    PATH=/opt/mise/shims:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
RUN curl -fsSL https://mise.run | MISE_INSTALL_PATH=/usr/local/bin/mise sh
COPY mise.toml /opt/mise/config/config.toml
RUN mise install
# conda-forge clang ships neither a clang++ driver nor a configuration that finds Ubuntu's
# libstdc++: add the driver name (mise does not shim it, hence the /usr/local/bin link) and point its config at the system GCC 15 install and sysroot.
ARG CLANG_VERSION=23.1.4
RUN B=/opt/mise/installs/clang/${CLANG_VERSION}/bin \
    && ln -s clang-23 "$B/clang++" \
    && printf -- '--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/15\n--sysroot=/\n' \
        > "$B/x86_64-conda-linux-gnu-clang.cfg" \
    && cp "$B/x86_64-conda-linux-gnu-clang.cfg" "$B/x86_64-conda-linux-gnu-clang++.cfg" \
    && ln -s "$B/clang++" /usr/local/bin/clang++ \
    && mise reshim && chmod -R a+rX /opt/mise

ENV CC=clang CXX=clang++ VCPKG_ROOT=/opt/vcpkg

RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics \
    && chown -R dev:dev /opt/vcpkg

# Playwright Chromium (+ system deps) shared by all users.
ENV PLAYWRIGHT_BROWSERS_PATH=/opt/playwright
RUN mkdir -p /opt/playwright-tmp && cd /opt/playwright-tmp \
    && bunx playwright install --with-deps chromium \
    && cd / && rm -rf /opt/playwright-tmp && chmod -R a+rX /opt/playwright

USER dev
WORKDIR /src
