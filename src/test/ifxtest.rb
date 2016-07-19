#!/opt/microsoft/omsagent/ruby/bin/ruby

# Set tracing information for IFX API and RtcPal
ENV["RTCPAL_LOG_LEVEL"] = "2"
ENV["RTCPAL_LOG_IDENT"] = "ruby"

require 'socket'
require '../lib/Libifxext'

testAccount = "AzLinux"
testNameSpace = "RubyIfxExt"
testMetric = "% Idle Time"
dim1 = "Hostname"
dim2 = "ProcessorName"

myComputename = Socket.gethostname()

Libifxext::MdmStartup()

x = Libifxext::Mdm2D.new(testAccount, testNameSpace, testMetric, dim1, dim2)
mdmmethods = (x.methods - Object.methods)
puts "Mdm2D methods: #{mdmmethods}"

(1..4).each {|i|
    r = x.LogValue(200+i, myComputename, "CPU" + (i%4).to_s)
    puts "LogValue #{i} returns #{r}"
    sleep(0.1)

    r2 = x.LogValueAtTime(131116247046770000, 200+i, myComputename, "CPU" + (i%4).to_s)
    puts "LogValueAtTime #{i} returns #{r}"
    sleep(0.1)
}

Libifxext::MdmCleanup()
