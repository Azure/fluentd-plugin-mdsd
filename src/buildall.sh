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

Usage()
{
    echo "Usage: $0 [-b buildname] <-d|-o> [-h]"
    echo "    -b: use buildname. Default: dev."
    echo "    -d: build debug build."
    echo "    -h: help."
    echo "    -o: build optimized(release) build."
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

args=`getopt b:dho $*`
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
        --) shift; break ;;
    esac
done

if [ -z "${BuildType}" ]; then
    echo "Error: missing build type"
    exit 1
fi

BuildWithCMake()
{
    echo
    echo Start to build source code. BuildType=${BuildType} ...
    BinDropDir=${BUILDDIR}.${BuildType}.${CCompiler}
    rm -rf ${BUILDDIR} ${BinDropDir}
    mkdir ${BinDropDir}
    ln -s ${BinDropDir} ${BUILDDIR}

    pushd ${BinDropDir}

    cmake -DCMAKE_C_COMPILER=${CCompiler} -DCMAKE_CXX_COMPILER=${CXXCompiler} \
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

# usage: BuildWithMake <dir>
BuildWithMake()
{
    echo
    echo Start to build: directory=$1 ...
    make -C $1 clean
    make LABEL=build.${BuildName} -C $1

    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: build $1 failed
        exit ${TotalErrors}
    else
        echo Finished built successfully: directory=$1
    fi
}

ParseGlibcVer()
{
    glibcver=2.9  # max GLIBC version to support
    dirname=./builddir/release/lib
    echo
    echo python ./test/parseglibc.py -d ${dirname} -v ${glibcver}
    python ./test/parseglibc.py -d ${dirname} -v ${glibcver}
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: ParseGlibcVer failed: maximum supported GLIBC version is ${glibcver}.
        exit ${TotalErrors}
    fi
}

echo Start build at `date`. BuildType=${BuildType} CC=${CCompiler} ...

BuildWithCMake
ParseGlibcVer
BuildWithMake fluent-plugin-mdsd
BuildWithMake debpkg

echo
echo Finished all builds at `date`. error = ${TotalErrors}
exit ${TotalErrors}
