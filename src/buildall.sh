#!/bin/bash

# This will build all source code
# Usage: see Usage()
#

TotalErrors=0

BuildType=
CCompiler=
CXXCompiler=
BuildName=
BUILDDIR=builddir

Usage()
{
    echo "Usage: $0 <-d|-o> <-c|-g> [-b buildname] [-h]"
    echo "    -b: use buildname. Default: timestamp."
    echo "    -c: use clang compiler."
    echo "    -d: build debug build."
    echo "    -g: use gcc/g++ compiler."
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

SetCompilers()
{
    if [ -z "${CCompiler}" ]; then
        CCompiler=$1
        CXXCompiler=$2
    else
        echo "Error: compiler is already set to ${CCompiler}."
        exit 1
    fi
}

args=`getopt b:cdgho $*`
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
        -c)
            SetCompilers clang clang++
            shift ;;
        -d)
            SetBuildType Debug
            shift ;;
        -g)
            SetCompilers gcc g++
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

if [ -z "${CCompiler}" ]; then
    echo "Error: missing compiler type."
    exit 1
fi

# Install any required packages that docker image don't have yet
# After the docker image has this, this should be removed.
InstallPkgs()
{
    echo "Search required pkg. If not found, install it ..."
#    dpkg -L <pkgname>
#    if [ $? != 0 ]; then
#        sudo apt-get update
#        sudo apt-get install -y <pkgname>
#    fi
}

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
    echo Start to build: directory=$1 compiler=${CCompiler} ...
    make -C $1 clean CC=${CCompiler}
    make -C $1 CC=${CCompiler} LABEL=build.${BuildName}

    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: build $1 failed
        exit ${TotalErrors}
    else
        echo Finished built successfully: directory=$1
    fi
}

echo Start build at `date`. BuildType=${BuildType} CC=${CCompiler} ...

InstallPkgs
BuildWithCMake
BuildWithMake package

echo
echo Finished all builds at `date`. error = ${TotalErrors}
exit ${TotalErrors}
