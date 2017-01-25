#!/opt/microsoft/omsagent/ruby/bin/ruby

require '../lib/Liboutmdsdrb'

def Usage()
  progname = __FILE__
  puts "#{progname} <nmsgs>"
end

def RemoveFile(filename)
  if File.exists?(filename)
    File.delete(filename)
  end
end

def GetSchemaVal(n)
  str = '1,[["timestr","FT_STRING"],["intnum","FT_INT64"],["string","FT_STRING"],["floatnum","FT_DOUBLE"]],["2016-11-04 09:17:01 -0700",'
  str << n.to_s << ',"costco",1.234]'
  return str
end

def GetLogger(logfile, socketFile)
  RemoveFile(logfile)

  Liboutmdsdrb::InitLogger(logfile, true)
  Liboutmdsdrb::SetLogLevel("trace")
  eplog = Liboutmdsdrb::SocketLogger.new(socketFile, 100, 1000)
  return eplog
end

def RunBasicTest(nmsgs)
  eplog = GetLogger("/tmp/outmdsd.log", "/tmp/default_djson.socket")

  nmsgs.times { |k|
    eplog.SendDjson("testsource", "test schemadata " + (k+1).to_s)
    sleep(0.01)
  }
  nTags = eplog.GetNumTagsRead()
  puts "Total ack tags read: #{nTags}"
end

def RunMdsdTest(nmsgs)
  eplog = GetLogger("/tmp/outmdsd.log", "/var/run/mdsd/default_djson.socket")
  nmsgs.times { |k|
    msg = GetSchemaVal(k+1)
    puts msg
    eplog.SendDjson("fluentdtest", msg)
    sleep(0.01)
  }
  nTags = eplog.GetNumTagsRead()
  puts "Total ack tags read: #{nTags}"
end

if __FILE__ == $0
  unless ARGV.length == 1
    Usage()
    exit
  end
  
  nmsgs = ARGV[0].to_i
  #RunBasicTest(nmsgs)
  RunMdsdTest(nmsgs)
end
