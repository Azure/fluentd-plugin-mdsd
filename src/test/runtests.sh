#!/bin/bash

# This script will run all tests.

TotalErrors=0

RunOutmdsdTest()
{
    echo Start ut_outmdsd unittest ...
    ./ut_outmdsd --log_level=all --report_format=HRF --report_level=detailed --report_sink=outmdsd_report.log > outmdsd_test.log 2>&1
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: ut_outmdsd unittest failed
    fi
}

RunRubyTest()
{
    echo Start RunRubyTest mdsdtest.rb ...
    ./mdsdtest.rb > rubytest.log 2>&1
    if [ $? != 0 ]; then
        let TotalErrors+=1
        echo Error: RunRubyTest mdsdtest.rb failed
    fi
}

# zip all related testlog files for debugging
ArchiveTestLogs()
{
    echo ArchiveTestLogs ...
    rm -f testresults.tar.gz
    tar --ignore-failed-read -czf testresults.tar.gz *.log *.txt
}

RunOutmdsdTest
RunRubyTest
ArchiveTestLogs

echo Finished all tests at `date`. Total errors = ${TotalErrors}
exit ${TotalErrors}
