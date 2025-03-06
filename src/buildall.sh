#!/bin/bash

# This will build all source code
# Usage: see Usage()
#

TotalErrors=0

BuildType=
CCompiler=gcc
CXXCompiler=g++
BUILDDIR=builddir
BuildName=dev
# Valid ${Target} values are: td (for treasure data), oms (for OMSAgent)
Target=td

# The full path directory containing ruby headers (e.g. 'ruby.h')
RUBY_INC_PATH=
# The full path directory containing ruby bin (e.g. 'ruby', 'gem', etc)
RUBY_BIN_PATH=

Usage()
{
    echo "Usage: $0 [-b buildname] <-d|-o> [-h]"
    echo "    -b: use buildname. Default: dev."
    echo "    -d: build debug build."
    echo "    -h: help."
    echo "    -o: build optimized(release) build."
    echo "    -t: fluentd target ('system', 'td' or 'oms')"
}

if [ "$#" == "0" ]; then
    Usage
    exit 1
fi

SetBuildType()
{
    if [ -z "${BuildType}" ]; then
        BuildType=$1
    else
        echo "Error: build type is already set to be ${BuildType}."
        exit 1
    fi
}

args=`getopt b:dhot: $*`
if [ $? != 0 ]; then
    Usage
    exit 1
fi
set -- $args

for i; do
    case "$i" in
        -b)
            BuildName=$2
            shift ; shift ;;
        -d)
            SetBuildType Debug
            shift ;;
        -h)
            Usage
            exit 0
            shift ;;
        -o)
            SetBuildType Release
            shift ;;
        -t)
            Target=$2
            shift; shift ;;
        --) shift; break ;;
    esac
done

if [ "${Target}" != "system" ] && [ "${Target}" != "td" ] && [ "${Target}" != "oms" ]; then
    echo "Error: invalid -t value. Expected: 'system', 'td' or 'oms'"
    exit 1
fi

if [ -z "${BuildType}" ]; then
    echo "Error: missing build type"
    exit 1
fi

FindRubyPath()
{
    # The full path directory containing ruby bin (e.g. 'ruby', 'gem', etc)
    if [ "${Target}" == "td" ]; then
        if [ -d "/opt/td-agent/embedded/bin" ]; then
            RubyBaseDir="/opt/td-agent/embedded/include/ruby-"
            RUBY_BIN_PATH=/opt/td-agent/embedded/bin
        else
            RubyBaseDir="/opt/td-agent/include/ruby-"
            RUBY_BIN_PATH=/opt/td-agent/bin
        fi
    elif [ "${Target}" == "oms" ]; then
        RubyBaseDir="/opt/microsoft/omsagent/ruby/include/ruby-"
        RUBY_BIN_PATH=/opt/microsoft/omsagent/ruby/bin
    elif [ "${Target}" == "system" ]; then
        RUBY_BIN_PATH=/usr/bin
        RUBY_INC_PATH=/usr/include
    else
        echo "FindRubyPath() error: unexpected target ${Target}."
        exit 1
    fi

    if [ -z "${RUBY_INC_PATH}" ]; then
        for diritem in "${RubyBaseDir}"*; do
            [ -d "${diritem}" ] && RUBY_INC_PATH="${diritem}" && break
        done
    fi

    if [ -z "${RUBY_INC_PATH}" ]; then
        echo "Error: failed to get value for RUBY_INC_PATH."
    else
        echo "Ruby include dir: ${RUBY_INC_PATH}."
    fi
}

BuildWithCMake()
{
    echo
    echo Start to build source code. BuildType=${BuildType} ...
    BinDropDir=${BUILDDIR}.${BuildType}.${Target}
    rm -rf ${BUILDDIR} ${BinDropDir}
    mkdir ${BinDropDir}
    ln -s ${BinDropDir} ${BUILDDIR}

    pushd ${BinDropDir}

    cmake -DCMAKE_C_COMPILER=${CCompiler} -DCMAKE_CXX_COMPILER=${CXXCompiler} \
          -DRUBY_INC_PATH=${RUBY_INC_PATH} \
          -DFLUENTD_TARGET=${Target} \
          -DCMAKE_BUILD_TYPE=${BuildType} ../

    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: cmake failed.
        exit ${TotalErrors}
    fi

    make -j4
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: make failed.
        exit ${TotalErrors}
    fi
    make install
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: \'make install\' failed.
        exit ${TotalErrors}       
    fi
    tar czf release.tar.gz release
    popd
}

# usage: BuildWithMake <dir> <options>
# <options> are optional
BuildWithMake()
{
    echo
    echo Start to build: directory=$1 $2 ...
    make -C $1 clean
    make LABEL=build.${BuildName} -C $1 $2

    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: build $1 $2 failed
        exit ${TotalErrors}
    else
        echo Finished built successfully: directory=$1
    fi
}

ParseGlibcVer()
{
    glibcver=2.9  # max GLIBC version to support
    dirname=./${BUILDDIR}/release/lib
    echo
    echo python ./test/parseglibc.py -d ${dirname} -v ${glibcver}
    python ./test/parseglibc.py -d ${dirname} -v ${glibcver}
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: ParseGlibcVer failed: maximum supported GLIBC version is ${glibcver}.
        exit ${TotalErrors}
    fi
}

CreateGemFile()
{
    echo CreateGemFile ...
    Label=build.${BuildName}
    Version=$(cat ./Version.txt)-${Label}

    pushd fluent-plugin-mdsd

    cp -f ../builddir/release/lib/Liboutmdsdrb.so ./lib/fluent/plugin/Liboutmdsdrb.so
    cp -f ../../LICENSE.txt ../../README.md .

    sed "s/GEMVERSION/${Version}/g" gemspec-template > fluent-plugin-mdsd.gemspec

    # If Target is 'system', then use gem to build the gem file. Otherwise, use fluent-gem.
    GEM_BIN=${RUBY_BIN_PATH}/fluent-gem
    if [ "${Target}" == "system" ]; then
        GEM_BIN=gem
    fi

    echo ${GEM_BIN} build fluent-plugin-mdsd.gemspec
    ${GEM_BIN} build fluent-plugin-mdsd.gemspec
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: CreateGemFile failed
        exit ${TotalErrors}
    fi

    popd
}

ReleaseGemFile()
{
    GemReleaseDir=${BUILDDIR}/release/gem

    pushd fluent-plugin-mdsd
    for f in $(ls *.gem); do
        mv $f ${f%.gem}-${Target}.amd64.gem
    done
    popd
    mkdir -p ${GemReleaseDir}
    mv fluent-plugin-mdsd/*.gem ${GemReleaseDir}
}

echo Start build at `date`. BuildType=${BuildType} CC=${CCompiler} Target=${Target} ...

FindRubyPath
BuildWithCMake
if [ "${Target}" != "system" ]; then
    ParseGlibcVer
fi
CreateGemFile
ReleaseGemFile

BuildWithMake debpkg

echo
echo Finished all builds at `date`. error = ${TotalErrors}
exit ${TotalErrors}
