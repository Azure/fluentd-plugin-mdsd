#!/bin/bash

# This script will run all tests.

TotalErrors=0


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
    echo RunPluginRubyTest ./testpluginrb.sh ...
    ./testpluginrb.sh > rubytest.log 2>&1
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: RunPluginRubyTest failed
    fi
}

# zip all related testlog files for debugging
ArchiveTestLogs()
{
    echo ArchiveTestLogs ...
    rm -f testresults.tar.gz
    tar --ignore-failed-read -czf testresults.tar.gz *.log
}

RunOutmdsdTest
RunPluginRubyTest
ArchiveTestLogs

echo Finished all tests at `date`. Total failed test suites = ${TotalErrors}

if [ ${TotalErrors} != 0 ]; then
    exit 1
else
    exit 0
fi
