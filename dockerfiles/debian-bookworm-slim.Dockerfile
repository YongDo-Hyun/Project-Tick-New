# syntax=docker/dockerfile:1.7
FROM debian:bookworm-slim

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     apt-get update; apt-get install -y --no-install-recommends ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     rm -rf /var/lib/apt/lists/*;   fi;   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1; then     true;   else     echo "Qt toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
