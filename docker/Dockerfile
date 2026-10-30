# xq3osp build image: id Software 1.32b q3lcc/q3cpp/q3rcc/q3asm and the __DATE__ shim.
#
# Build from the repository root (the build context must be the repo root):
#   docker build -f docker/Dockerfile -t xq3osp-build .
FROM debian:bookworm-slim

# gcc-multilib: the id-era lcc/q3asm sources assume a 32-bit host (ILP32), so they are
# built with -m32 (see docker/build-tools.sh).
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential gcc-multilib \
 && rm -rf /var/lib/apt/lists/*

# Only the tool sources are copied in (see /.dockerignore); the repository is bind-mounted
# at /work at run time.
COPY src/tools /opt/xq3osp-src/src/tools
COPY docker/build-tools.sh /opt/xq3osp-src/docker/build-tools.sh
COPY docker/cppdate.c /opt/xq3osp-src/docker/cppdate.c

RUN sh /opt/xq3osp-src/docker/build-tools.sh /opt/xq3osp-src /opt/q3tools

ENV PATH=/opt/q3tools/bin:$PATH
WORKDIR /work
