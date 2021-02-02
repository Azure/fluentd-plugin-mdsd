FROM ubuntu:16.04

# install wget, curl, sudo, ruby, make, git, gcc, clang, and rake
RUN apt-get update \
    && apt-get install -y \
        wget \
        curl \
        sudo
RUN apt-get update \
    && apt-get install -y \
        make \
        git \
        ruby 
RUN apt-get install -y \
        gcc-4.8 g++ gcc-4.8-base \
        clang-3.5 lldb-3.5 
RUN gem install rake 

# build cmake
RUN apt-get install -y cmake

# install swig 3.0
RUN apt-get update
RUN apt-get install swig3.0

# install boost
RUN apt-get install libboost-system1.58-dev libboost-test1.58-dev -y

# install td-agent
run curl -L https://toolbelt.treasuredata.com/sh/install-ubuntu-xenial-td-agent3.sh | sh

# install lintian
run apt-get install lintian -y