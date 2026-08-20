FROM ubuntu:22.04

ARG UID=1000
ARG GID=1000

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    git \
    python3 \
    python3-venv \
    python3-pip \
    curl \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/15.3.rel1/arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-eabi.tar.xz" \
    -o /tmp/gcc-arm-none-eabi.tar.xz \
    && tar -xf /tmp/gcc-arm-none-eabi.tar.xz -C /opt \
    && mv /opt/arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-eabi /opt/gcc-arm-none-eabi \
    && rm /tmp/gcc-arm-none-eabi.tar.xz

ENV PATH="/opt/gcc-arm-none-eabi/bin:${PATH}"

RUN git clone --branch 2.2.0 --depth 1 https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk \
    && cd /opt/pico-sdk && git submodule update --init --depth 1

ENV PICO_SDK_PATH=/opt/pico-sdk

RUN groupadd -g $GID builder && \
    useradd -m -u $UID -g $GID builder

USER builder
RUN git config --global safe.directory '*'

WORKDIR /build
