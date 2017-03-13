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

RUBY_INC_PATH=

Usage()
{
    echo "Usage: $0 [-b buildname] <-d|-o> [-h]"
    echo "    -b: use buildname. Default: dev."
    echo "    -d: build debug build."
    echo "    -h: help."
    echo "    -o: build optimized(release) build."
    echo "    -t: fluentd target ('td' or 'oms')"
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

if [ "${Target}" != "td" ] && [ "${Target}" != "oms" ]; then
    echo "Error: invalid -t value. Expected: 'td' or 'oms'"
    exit 1
fi

if [ -z "${BuildType}" ]; then
    echo "Error: missing build type"
    exit 1
fi

FindRubyPath()
{
    if [ "${Target}" == "td" ]; then
        RubyBaseDir="/opt/td-agent/embedded/include/ruby-"
    elif [ "${Target}" == "oms" ]; then
        RubyBaseDir="/opt/microsoft/omsagent/ruby/include/ruby-"
    else
        echo "FindRubyPath() error: unexpected target ${Target}."
        exit 1
    fi

    for diritem in "${RubyBaseDir}"*; do
        [ -d "${diritem}" ] && RUBY_INC_PATH="${diritem}" && break
    done

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

ReleaseGemFile()
{
    GemReleaseDir=${BUILDDIR}/release/gem
    Target=$1

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
ParseGlibcVer

BuildWithMake fluent-plugin-mdsd Target=${Target}
ReleaseGemFile ${Target}

BuildWithMake debpkg

echo
echo Finished all builds at `date`. error = ${TotalErrors}
exit ${TotalErrors}
