# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.145.1/containers/cpp/.devcontainer/base.Dockerfile

# [Choice] Debian / Ubuntu version: debian-10, debian-9
ARG VARIANT="debian-11"
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

# [Optional] Uncomment this section to install additional packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends git wget \
    flex bison gperf python3 python3-pip python3-setuptools \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util \
    libusb-1.0-0 python-is-python3

# Install ESP-IDF.
RUN mkdir -p ~/esp \
    && cd ~/esp \
    && git clone --recursive https://github.com/espressif/esp-idf.git \
    && cd ~/esp/esp-idf \
    && ./install.sh

# Set up canonical compiler path independent of installed SDK version.
RUN ln -s $(find $HOME -name xtensa-esp32-elf-gcc) /usr/bin/xtensa-esp32-elf-gcc

# Set up alias to make ESP-IDF tool available in PATH.
RUN echo "alias idf='. $HOME/esp/esp-idf/export.sh'" >> ~/.bashrc
