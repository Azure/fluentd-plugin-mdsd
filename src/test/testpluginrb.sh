#!/bin/bash -xe

sudo gem install bundler
pushd ../../../fluent-plugin-mdsd/
bundle install
bundle exec rake test
popd
