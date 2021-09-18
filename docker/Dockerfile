# syntax = docker/dockerfile:1.1.1-experimental
# [Choice] Ubuntu version: bionic, focal
ARG VARIANT=ubuntu-18.04
FROM mcr.microsoft.com/vscode/devcontainers/base:${VARIANT} AS base

# ARG PACKAGE_NAME
# LABEL jp.shiguredo.momo=$PACKAGE_NAME
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
  binutils-aarch64-linux-gnu \
  binutils-arm-linux-gnueabi \
  binutils-arm-linux-gnueabihf \
  build-essential \
  clang-format \
  curl \
  git \
  lbzip2 \
  libasound2-dev \
  libc6-dev \
  libexpat1-dev \
  libgtk-3-dev \
  libtinfo5 \
  libnspr4-dev \
  libnss3-dev \
  libpulse-dev \
  libudev-dev \
  libxrandr-dev \
  lsb-release \
  multistrap \
  python \
  python-dev \
  software-properties-common \
  vim-nox \
  wget \
  xz-utils


# WebRTC の取得（hostだけCMakeのバージョン取得のために先にやる）
ARG WEBRTC_BUILD_VERSION
ARG VARIANT

WORKDIR /tool/webrtc/host
RUN curl -fsqSL https://github.com/shiguredo-webrtc-build/webrtc-build/releases/download/m${WEBRTC_BUILD_VERSION}/webrtc.${VARIANT}_x86_64.tar.gz | \
  tar zxf - --strip-components 1

COPY script/get_llvm.sh /tool
RUN /tool/get_llvm.sh /tool/webrtc/host /tool

# CMake のインストール
ARG CMAKE_VERSION
WORKDIR /tool/cmake
RUN curl -fsqSL https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz | \
  tar zxf - --strip-components 1
ENV PATH "/tool/cmake/bin:$PATH"

# rootfs の構築
FROM base AS rootfs
# Ubuntu 18.04 で multistrap が動かない問題の修正。
# https://github.com/volumio/Build/issues/348#issuecomment-462271607 を参照
RUN sed -e 's/Apt::Get::AllowUnauthenticated=true/Apt::Get::AllowUnauthenticated=true";\n$config_str .= " -o Acquire::AllowInsecureRepositories=true/' -i /usr/sbin/multistrap

# ARM FS
# RUN wget http://archive.raspbian.org/raspbian.public.key -O - | apt-key add -
COPY script/init_rootfs_arm*.sh /tool/
COPY rpi-os-armv6.conf /tool/
RUN /tool/init_rootfs_armhf.sh /tool/rootfs/raspberry-pi-os_armv6 /tool/rpi-os-armv6.conf
COPY rpi-os-armv7.conf /tool/
RUN /tool/init_rootfs_armhf.sh /tool/rootfs/raspberry-pi-os_armv7 /tool/rpi-os-armv7.conf
COPY rpi-os-armv8.conf /tool/
RUN /tool/init_rootfs_arm64.sh /tool/rootfs/raspberry-pi-os_armv8 /tool/rpi-os-armv8.conf

COPY jetson_*.conf /tool/
COPY script/jetson_*.sh /tool/
RUN /tool/init_rootfs_arm64.sh /tool/rootfs/ubuntu-18.04_armv8_jetson_nano /tool/jetson_nano.conf
RUN /tool/jetson_nano.sh
RUN /tool/init_rootfs_arm64.sh /tool/rootfs/ubuntu-18.04_armv8_jetson_xavier /tool/jetson_xavier.conf
RUN /tool/jetson_xavier.sh


# WebRTC の取得（hostだけCMakeのバージョン取得のために先にやってある）
FROM base AS webrtc_builder
ARG WEBRTC_BUILD_VERSION

WORKDIR /tool/webrtc/raspberry-pi-os_armv6
RUN curl -fsqSL https://github.com/shiguredo-webrtc-build/webrtc-build/releases/download/m${WEBRTC_BUILD_VERSION}/webrtc.raspberry-pi-os_armv6.tar.gz | \
  tar zxf - --strip-components 1
WORKDIR /tool/webrtc/raspberry-pi-os_armv7
RUN curl -fsqSL https://github.com/shiguredo-webrtc-build/webrtc-build/releases/download/m${WEBRTC_BUILD_VERSION}/webrtc.raspberry-pi-os_armv7.tar.gz | \
  tar zxf - --strip-components 1
WORKDIR /tool/webrtc/raspberry-pi-os_armv8
RUN curl -fsqSL https://github.com/shiguredo-webrtc-build/webrtc-build/releases/download/m${WEBRTC_BUILD_VERSION}/webrtc.raspberry-pi-os_armv8.tar.gz | \
  tar zxf - --strip-components 1
WORKDIR /tool/webrtc/ubuntu-18.04_armv8
RUN curl -fsqSL https://github.com/shiguredo-webrtc-build/webrtc-build/releases/download/m${WEBRTC_BUILD_VERSION}/webrtc.ubuntu-18.04_armv8.tar.gz | \
  tar zxf - --strip-components 1

# Boost のビルド
FROM base AS host_boost_builder
ARG BOOST_VERSION
ENV BOOST_VERSION_UNDERSCORE=

WORKDIR /tool/boost_source
SHELL [ "/bin/bash", "-c" ]
RUN curl -fsqSL https://boostorg.jfrog.io/artifactory/main/release/$BOOST_VERSION/source/boost_${BOOST_VERSION//./_}.tar.gz | \
  tar zxf - --strip-components 1

## host
WORKDIR /tool/boost_source
RUN ./bootstrap.sh
RUN echo 'using clang : : /tool/llvm/clang/bin/clang++ : ;' > project-config.jam
RUN ./b2 \
  cxxflags=' \
  -D_LIBCPP_ABI_UNSTABLE \
  -D_LIBCPP_DISABLE_AVAILABILITY \
  -nostdinc++ \
  -isystem/tool/llvm/libcxx/include \
  ' \
  linkflags=' \
  ' \
  toolset=clang \
  visibility=global \
  target-os=linux \
  address-model=64 \
  link=static \
  variant=release \
  install \
  -j`nproc` \
  --ignore-site-config \
  --prefix=/tool/boost/host \
  --build-dir=/tool/build/boost/host \
  --with-filesystem \
  --with-json

FROM host_boost_builder AS cross_boost_builder
COPY --from=rootfs /tool/rootfs /tool/rootfs

## raspberry-pi-os_armv6
RUN ./bootstrap.sh
RUN echo 'using clang : : /tool/llvm/clang/bin/clang++ : ;' > project-config.jam
RUN ./b2 \
  cxxflags=' \
  -D_LIBCPP_ABI_UNSTABLE \
  -D_LIBCPP_DISABLE_AVAILABILITY \
  -nostdinc++ \
  -isystem/tool/llvm/libcxx/include \
  --target=arm-linux-gnueabihf \
  --sysroot=/tool/rootfs/raspberry-pi-os_armv6 \
  -I/tool/rootfs/raspberry-pi-os_armv6/usr/include/arm-linux-gnueabihf \
  ' \
  linkflags=' \
  -L/tool/rootfs/raspberry-pi-os_armv6/usr/lib/arm-linux-gnueabihf \
  -B/tool/rootfs/raspberry-pi-os_armv6/usr/lib/arm-linux-gnueabihf \
  ' \
  toolset=clang \
  visibility=global \
  target-os=linux \
  architecture=arm \
  address-model=32 \
  link=static \
  variant=release \
  install \
  -j`nproc` \
  --ignore-site-config \
  --prefix=/tool/boost/raspberry-pi-os_armv6 \
  --build-dir=/tool/build/boost/raspberry-pi-os_armv6 \
  --with-filesystem \
  --with-json

## raspberry-pi-os_armv7
RUN ./bootstrap.sh
RUN echo 'using clang : : /tool/llvm/clang/bin/clang++ : ;' > project-config.jam
RUN ./b2 \
  cxxflags=' \
  -D_LIBCPP_ABI_UNSTABLE \
  -D_LIBCPP_DISABLE_AVAILABILITY \
  -nostdinc++ \
  -isystem/tool/llvm/libcxx/include \
  --target=arm-linux-gnueabihf \
  --sysroot=/tool/rootfs/raspberry-pi-os_armv7 \
  -I/tool/rootfs/raspberry-pi-os_armv7/usr/include/arm-linux-gnueabihf \
  ' \
  linkflags=' \
  -L/tool/rootfs/raspberry-pi-os_armv7/usr/lib/arm-linux-gnueabihf \
  -B/tool/rootfs/raspberry-pi-os_armv7/usr/lib/arm-linux-gnueabihf \
  ' \
  toolset=clang \
  visibility=global \
  target-os=linux \
  architecture=arm \
  address-model=32 \
  link=static \
  variant=release \
  install \
  -j`nproc` \
  --ignore-site-config \
  --prefix=/tool/boost/raspberry-pi-os_armv7 \
  --build-dir=/tool/build/boost/raspberry-pi-os_armv7 \
  --with-filesystem \
  --with-json

## raspberry-pi-os_armv8
RUN ./bootstrap.sh
RUN echo 'using clang : : /tool/llvm/clang/bin/clang++ : ;' > project-config.jam
RUN ./b2 \
  cxxflags=' \
  -D_LIBCPP_ABI_UNSTABLE \
  -D_LIBCPP_DISABLE_AVAILABILITY \
  -nostdinc++ \
  -isystem/tool/llvm/libcxx/include \
  --target=aarch64-linux-gnu \
  --sysroot=/tool/rootfs/raspberry-pi-os_armv8 \
  -I/tool/rootfs/raspberry-pi-os_armv8/usr/include/aarch64-linux-gnu \
  ' \
  linkflags=' \
  -L/tool/rootfs/raspberry-pi-os_armv8/usr/lib/aarch64-linux-gnu \
  -B/tool/rootfs/raspberry-pi-os_armv8/usr/lib/aarch64-linux-gnu \
  ' \
  toolset=clang \
  visibility=global \
  target-os=linux \
  architecture=arm \
  address-model=64 \
  link=static \
  variant=release \
  install \
  -j`nproc` \
  --ignore-site-config \
  --prefix=/tool/boost/raspberry-pi-os_armv8 \
  --build-dir=/tool/build/boost/raspberry-pi-os_armv8 \
  --with-filesystem \
  --with-json

## ubuntu-18.04_armv8_jetson_nano
RUN ./bootstrap.sh
RUN echo 'using clang : : /tool/llvm/clang/bin/clang++ : ;' > project-config.jam
RUN ./b2 \
  cxxflags=' \
  -D_LIBCPP_ABI_UNSTABLE \
  -D_LIBCPP_DISABLE_AVAILABILITY \
  -nostdinc++ \
  -isystem/tool/llvm/libcxx/include \
  --target=aarch64-linux-gnu \
  --sysroot=/tool/rootfs/ubuntu-18.04_armv8_jetson_nano \
  -I/tool/rootfs/ubuntu-18.04_armv8_jetson_nano/usr/include/aarch64-linux-gnu \
  ' \
  linkflags=' \
  -L/tool/rootfs/ubuntu-18.04_armv8_jetson_nano/usr/lib/aarch64-linux-gnu \
  -B/tool/rootfs/ubuntu-18.04_armv8_jetson_nano/usr/lib/aarch64-linux-gnu \
  ' \
  toolset=clang \
  visibility=global \
  target-os=linux \
  architecture=arm \
  address-model=64 \
  link=static \
  variant=release \
  install \
  -j`nproc` \
  --ignore-site-config \
  --prefix=/tool/boost/ubuntu-18.04_armv8_jetson_nano \
  --build-dir=/tool/build/boost/ubuntu-18.04_armv8_jetson_nano \
  --with-filesystem \
  --with-json

## ubuntu-18.04_armv8_jetson_xavier
RUN ./bootstrap.sh
RUN echo 'using clang : : /tool/llvm/clang/bin/clang++ : ;' > project-config.jam
RUN ./b2 \
  cxxflags=' \
  -D_LIBCPP_ABI_UNSTABLE \
  -D_LIBCPP_DISABLE_AVAILABILITY \
  -nostdinc++ \
  -isystem/tool/llvm/libcxx/include \
  --target=aarch64-linux-gnu \
  --sysroot=/tool/rootfs/ubuntu-18.04_armv8_jetson_xavier \
  -I/tool/rootfs/ubuntu-18.04_armv8_jetson_xavier/usr/include/aarch64-linux-gnu \
  ' \
  linkflags=' \
  -L/tool/rootfs/ubuntu-18.04_armv8_jetson_xavier/usr/lib/aarch64-linux-gnu \
  -B/tool/rootfs/ubuntu-18.04_armv8_jetson_xavier/usr/lib/aarch64-linux-gnu \
  ' \
  toolset=clang \
  visibility=global \
  target-os=linux \
  architecture=arm \
  address-model=64 \
  link=static \
  variant=release \
  install \
  -j`nproc` \
  --ignore-site-config \
  --prefix=/tool/boost/ubuntu-18.04_armv8_jetson_xavier \
  --build-dir=/tool/build/boost/ubuntu-18.04_armv8_jetson_xavier \
  --with-filesystem \
  --with-json


# CLI11 の取得
FROM base AS cli11_builder

ARG CLI11_VERSION
RUN git clone --branch v$CLI11_VERSION --depth 1 https://github.com/CLIUtils/CLI11.git /tool/CLI11

# SDL2 のビルド
FROM base AS host_sdl2_builder
ARG SDL2_VERSION

## ソースの取得
WORKDIR /tool/source/sdl2
RUN curl -fsqSL http://www.libsdl.org/release/SDL2-$SDL2_VERSION.tar.gz | \
  tar zxf - --strip-components 1

## host
WORKDIR /tool/source/sdl2/build/host
RUN cmake ../.. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tool/SDL2/host \
  -DCMAKE_C_COMPILER=/tool/llvm/clang/bin/clang \
  -DCMAKE_CXX_COMPILER=/tool/llvm/clang/bin/clang++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DSDL_ATOMIC=OFF \
  -DSDL_AUDIO=OFF \
  -DSDL_VIDEO=ON \
  -DSDL_RENDER=ON \
  -DSDL_EVENTS=ON \
  -DSDL_JOYSTICK=ON \
  -DSDL_HAPTIC=ON \
  -DSDL_POWER=ON \
  -DSDL_THREADS=ON \
  -DSDL_TIMERS=OFF \
  -DSDL_FILE=OFF \
  -DSDL_LOADSO=ON \
  -DSDL_CPUINFO=OFF \
  -DSDL_FILESYSTEM=OFF \
  -DSDL_DLOPEN=ON \
  -DSDL_SENSOR=ON \
  -DVIDEO_OPENGL=ON \
  -DVIDEO_OPENGLES=ON \
  -DVIDEO_RPI=OFF \
  -DVIDEO_WAYLAND=OFF \
  -DVIDEO_X11=ON \
  -DX11_SHARED=OFF \
  -DVIDEO_X11_XCURSOR=OFF \
  -DVIDEO_X11_XINERAMA=OFF \
  -DVIDEO_X11_XINPUT=OFF \
  -DVIDEO_X11_XRANDR=OFF \
  -DVIDEO_X11_XSCRNSAVER=OFF \
  -DVIDEO_X11_XSHAPE=OFF \
  -DVIDEO_X11_XVM=OFF \
  -DVIDEO_VULKAN=OFF \
  -DVIDEO_VIVANTE=OFF \
  -DVIDEO_COCOA=OFF \
  -DVIDEO_METAL=OFF \
  -DVIDEO_KMSDRM=OFF
RUN cmake --build . -j$(nproc)
RUN cmake --install .

FROM host_sdl2_builder AS cross_sdl2_builder
COPY --from=rootfs /tool/rootfs /tool/rootfs

## raspberry-pi-os_armv7
WORKDIR /tool/source/sdl2/build/raspberry-pi-os_armv7
RUN cmake ../.. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tool/SDL2/raspberry-pi-os_armv7 \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER=/tool/llvm/clang/bin/clang \
  -DCMAKE_C_COMPILER_TARGET=arm-linux-gnueabihf \
  -DCMAKE_CXX_COMPILER=/tool/llvm/clang/bin/clang++ \
  -DCMAKE_CXX_COMPILER_TARGET=arm-linux-gnueabihf \
  -DCMAKE_FIND_ROOT_PATH=/tool/rootfs/raspberry-pi-os_armv7 \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DCMAKE_SYSROOT=/tool/rootfs/raspberry-pi-os_armv7 \
  -DBUILD_SHARED_LIBS=OFF \
  -DSDL_ATOMIC=OFF \
  -DSDL_AUDIO=OFF \
  -DSDL_VIDEO=ON \
  -DSDL_RENDER=ON \
  -DSDL_EVENTS=ON \
  -DSDL_JOYSTICK=ON \
  -DSDL_HAPTIC=ON \
  -DSDL_POWER=ON \
  -DSDL_THREADS=ON \
  -DSDL_TIMERS=OFF \
  -DSDL_FILE=OFF \
  -DSDL_LOADSO=ON \
  -DSDL_CPUINFO=OFF \
  -DSDL_FILESYSTEM=OFF \
  -DSDL_DLOPEN=ON \
  -DSDL_SENSOR=ON \
  -DVIDEO_OPENGL=ON \
  -DVIDEO_OPENGLES=ON \
  -DVIDEO_RPI=OFF \
  -DVIDEO_WAYLAND=OFF \
  -DVIDEO_X11=ON \
  -DX11_SHARED=OFF \
  -DVIDEO_X11_XCURSOR=OFF \
  -DVIDEO_X11_XINERAMA=OFF \
  -DVIDEO_X11_XINPUT=OFF \
  -DVIDEO_X11_XRANDR=OFF \
  -DVIDEO_X11_XSCRNSAVER=OFF \
  -DVIDEO_X11_XSHAPE=OFF \
  -DVIDEO_X11_XVM=OFF \
  -DVIDEO_VULKAN=OFF \
  -DVIDEO_VIVANTE=OFF \
  -DVIDEO_COCOA=OFF \
  -DVIDEO_METAL=OFF \
  -DVIDEO_KMSDRM=OFF
RUN cmake --build . -j$(nproc)
RUN cmake --install .

## ubuntu-18.04_armv8_jetson_nano
WORKDIR /tool/source/sdl2/build/ubuntu-18.04_armv8_jetson_nano
RUN cmake ../.. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tool/SDL2/ubuntu-18.04_armv8_jetson_nano \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=/tool/llvm/clang/bin/clang \
  -DCMAKE_C_COMPILER_TARGET=aarch64-linux-gnu \
  -DCMAKE_CXX_COMPILER=/tool/llvm/clang/bin/clang++ \
  -DCMAKE_CXX_COMPILER_TARGET=aarch64-linux-gnu \
  -DCMAKE_FIND_ROOT_PATH=/tool/rootfs/ubuntu-18.04_armv8_jetson_nano \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DCMAKE_SYSROOT=/tool/rootfs/ubuntu-18.04_armv8_jetson_nano \
  -DBUILD_SHARED_LIBS=OFF \
  -DSDL_ATOMIC=OFF \
  -DSDL_AUDIO=OFF \
  -DSDL_VIDEO=ON \
  -DSDL_RENDER=ON \
  -DSDL_EVENTS=ON \
  -DSDL_JOYSTICK=ON \
  -DSDL_HAPTIC=ON \
  -DSDL_POWER=ON \
  -DSDL_THREADS=ON \
  -DSDL_TIMERS=OFF \
  -DSDL_FILE=OFF \
  -DSDL_LOADSO=ON \
  -DSDL_CPUINFO=OFF \
  -DSDL_FILESYSTEM=OFF \
  -DSDL_DLOPEN=ON \
  -DSDL_SENSOR=ON \
  -DVIDEO_OPENGL=ON \
  -DVIDEO_OPENGLES=ON \
  -DVIDEO_RPI=OFF \
  -DVIDEO_WAYLAND=OFF \
  -DVIDEO_X11=ON \
  -DX11_SHARED=OFF \
  -DVIDEO_X11_XCURSOR=OFF \
  -DVIDEO_X11_XINERAMA=OFF \
  -DVIDEO_X11_XINPUT=OFF \
  -DVIDEO_X11_XRANDR=OFF \
  -DVIDEO_X11_XSCRNSAVER=OFF \
  -DVIDEO_X11_XSHAPE=OFF \
  -DVIDEO_X11_XVM=OFF \
  -DVIDEO_VULKAN=OFF \
  -DVIDEO_VIVANTE=OFF \
  -DVIDEO_COCOA=OFF \
  -DVIDEO_METAL=OFF \
  -DVIDEO_KMSDRM=OFF
RUN cmake --build . -j$(nproc)
RUN cmake --install .

## jetson_xavier
WORKDIR /tool/source/sdl2/build/ubuntu-18.04_armv8_jetson_xavier
RUN cmake ../.. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tool/SDL2/ubuntu-18.04_armv8_jetson_xavier \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=/tool/llvm/clang/bin/clang \
  -DCMAKE_C_COMPILER_TARGET=aarch64-linux-gnu \
  -DCMAKE_CXX_COMPILER=/tool/llvm/clang/bin/clang++ \
  -DCMAKE_CXX_COMPILER_TARGET=aarch64-linux-gnu \
  -DCMAKE_FIND_ROOT_PATH=/tool/rootfs/ubuntu-18.04_armv8_jetson_xavier \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
  -DCMAKE_SYSROOT=/tool/rootfs/ubuntu-18.04_armv8_jetson_xavier \
  -DBUILD_SHARED_LIBS=OFF \
  -DSDL_ATOMIC=OFF \
  -DSDL_AUDIO=OFF \
  -DSDL_VIDEO=ON \
  -DSDL_RENDER=ON \
  -DSDL_EVENTS=ON \
  -DSDL_JOYSTICK=ON \
  -DSDL_HAPTIC=ON \
  -DSDL_POWER=ON \
  -DSDL_THREADS=ON \
  -DSDL_TIMERS=OFF \
  -DSDL_FILE=OFF \
  -DSDL_LOADSO=ON \
  -DSDL_CPUINFO=OFF \
  -DSDL_FILESYSTEM=OFF \
  -DSDL_DLOPEN=ON \
  -DSDL_SENSOR=ON \
  -DVIDEO_OPENGL=ON \
  -DVIDEO_OPENGLES=ON \
  -DVIDEO_RPI=OFF \
  -DVIDEO_WAYLAND=OFF \
  -DVIDEO_X11=ON \
  -DX11_SHARED=OFF \
  -DVIDEO_X11_XCURSOR=OFF \
  -DVIDEO_X11_XINERAMA=OFF \
  -DVIDEO_X11_XINPUT=OFF \
  -DVIDEO_X11_XRANDR=OFF \
  -DVIDEO_X11_XSCRNSAVER=OFF \
  -DVIDEO_X11_XSHAPE=OFF \
  -DVIDEO_X11_XVM=OFF \
  -DVIDEO_VULKAN=OFF \
  -DVIDEO_VIVANTE=OFF \
  -DVIDEO_COCOA=OFF \
  -DVIDEO_METAL=OFF \
  -DVIDEO_KMSDRM=OFF
RUN cmake --build . -j$(nproc)
RUN cmake --install .

# CUDA 周りのインストール
FROM base AS cuda_installer
ARG CUDA_VERSION
ARG VARIANT
SHELL [ "/bin/bash", "-c" ]
RUN wget -O /etc/apt/preferences.d/cuda-repository-pin-600 https://developer.download.nvidia.com/compute/cuda/repos/${VARIANT//[-.]/}/x86_64/cuda-${VARIANT//[-.]/}.pin \
  && apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/${VARIANT//[-.]/}/x86_64/7fa2af80.pub \
  && add-apt-repository "deb http://developer.download.nvidia.com/compute/cuda/repos/${VARIANT//[-.]/}/x86_64/ /" \
  && apt-get update \
  && apt-get -y install cuda=$CUDA_VERSION clang-10
RUN apt-get install -y clangd-10
RUN update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-10 100

FROM cuda_installer AS host_momo_builder
COPY --from=host_boost_builder /tool/boost /tool/boost
COPY --from=cli11_builder /tool/CLI11 /tool/CLI11
COPY --from=host_sdl2_builder /tool/SDL2 /tool/SDL2


FROM cuda_installer AS cross_momo_builder
COPY --from=rootfs /tool/rootfs /tool/rootfs
COPY --from=webrtc_builder /tool/webrtc /tool/webrtc
COPY --from=cross_boost_builder /tool/boost /tool/boost
COPY --from=cli11_builder /tool/CLI11 /tool/CLI11
COPY --from=cross_sdl2_builder /tool/SDL2 /tool/SDL2
