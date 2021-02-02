#!/bin/bash
set -eu -o pipefail
set -x

docker run -it --rm \
    -e REPO_DIR=`pwd` \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -w /fluentd \
    -v 'pwd':/fluentd \
    fluentd-plugin-mdsd-dev