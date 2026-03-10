FROM python:3.14

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    locales \
    build-essential \
    lcov \
    gcovr \
    pkg-config \
    libgtest-dev \
    libgmock-dev \
    libexpat-dev \
    gdb \
    git \
    xmldiff \
    less \
    vim \
    sudo  && \
    echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/\* /tmp/\* /var/tmp/* && \
    useradd -m docker

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en

WORKDIR /workspace

SHELL ["/bin/bash", "-c"]
