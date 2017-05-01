#!/bin/bash

# This script will run all tests.

TotalErrors=0

# Valid ${Target} values are: td (for treasure data), oms (for OMSAgent)
Target=td

# The bin directory where ruby locates.
RubyDir=

Usage()
{
    echo "Usage: $0 [-t target] [-h]"
    echo "    -h: help."
    echo "    -t: fluentd target ('td' or 'oms')"
}

FindRubyPath()
{
    if [ ${Target} == "td" ]; then
        RubyDir=/opt/td-agent/embedded/bin
    elif [ ${Target} == "oms" ]; then
        RubyDir=/opt/microsoft/omsagent/ruby/bin
    fi
}

# Syslog is required for some ut_outmdsd tests.
# In docker image, rsyslogd is not started and may not
# configured properly. So handle them in this function.
#
# In non-docker machine, assume the proper syslog is
# configured. If not, user may get test error, and test
# error should contain details on what's wrong.
ConfigSyslog()
{
    grep docker /proc/self/cgroup > /dev/null
    if [ $? == 0 ]; then
        echo Configure syslog ...
        sudo chown syslog.syslog /var/log/
        sudo touch /var/log/syslog
        sudo chmod a+r /var/log/syslog
        sudo chown syslog.adm /var/log/syslog
        sudo rsyslogd > /dev/null 2>&1
    fi
}

RunOutmdsdTest()
{
    ConfigSyslog

    echo Start ut_outmdsd unittest ...
    ./ut_outmdsd --log_level=all --report_format=HRF --report_level=detailed --report_sink=outmdsd_report.log > outmdsd_test.log 2>&1
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: ut_outmdsd unittest failed
    fi
}

# The ruby code part of the plugin
RunPluginRubyTest()
{
    ${RubyDir}/gem unpack ../gem/fluent-plugin-mdsd*.gem
    pushd fluent-plugin-mdsd*

    echo RunPluginRubyTest: ${RubyDir}/ruby ${RubyDir}/rake test
    ${RubyDir}/ruby ${RubyDir}/rake test
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: RunPluginRubyTest failed
    fi

    popd
}

RunGemInstallTest()
{
    echo RunGemInstallTest ...
    sudo ${RubyDir}/fluent-gem install ../gem/fluent-plugin-mdsd*.gem
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: RunGemInstallTest failed
    fi
}

# zip all related testlog files for debugging
ArchiveTestLogs()
{
    echo ArchiveTestLogs ...
    rm -f testresults.tar.gz
    tar --ignore-failed-read -czf testresults.tar.gz *.log
}


if [ "$#" == "0" ]; then
    Usage
    exit 1
fi

args=`getopt ht: $*`

if [ $? != 0 ]; then
    Usage
    exit 1
fi
set -- $args

for i; do
    case "$i" in
        -h)
            Usage
            exit 0
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

FindRubyPath

RunOutmdsdTest
RunPluginRubyTest
RunGemInstallTest

ArchiveTestLogs

echo Finished all tests at `date`. Total failed test suites = ${TotalErrors}

if [ ${TotalErrors} != 0 ]; then
    exit 1
else
    exit 0
fi
