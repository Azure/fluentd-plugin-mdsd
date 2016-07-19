# This is MDM output plugin: data will be sent to MDM through IFX API.
# Only OMI data are supported.
module Fluent
    class OutputMDM < BufferedOutput
        Plugin.register_output('mdm', self)

        def initialize
            super
            require_relative 'Libifxext'
            require 'time'
            # For debug purpose only
            ENV["RTCPAL_LOG_LEVEL"] = "2"
            ENV["RTCPAL_LOG_IDENT"] = "ruby"
            @@mdm2dtable = {}
        end

        config_param :mdm_account, :string

        def configure(conf)
            super
        end

        def start
            super
            Libifxext::MdmStartup()
        end

        def shutdown
            super
            Libifxext::MdmCleanup()
        end

        # This method is called when an event reaches to Fluentd.
        # Convert the event to a raw string.
        def format(tag, time, record)
            @log.trace "Buffering #{tag}"
            [tag, record].to_msgpack
        end


        # This method is called every flush interval. Send the buffer chunk to OMS. 
        # 'chunk' is a buffer chunk that includes multiple formatted events.
        def write(chunk)
            # handle OMI datatype only
            chunk.msgpack_each {|(tag, record)|
                datatype = record["DataType"]
                if datatype != "LINUX_PERF_BLOB"
                    @log.error "Error: unsupported data type #{datatype} for tag #{tag}. Ignore data."
                else
                    handle_record(record)
                end
            }
        end

        # each record has an array called DataItems
        # each DataItem has an array called Collections
        # each Collection is {CounterName, Value}
        def handle_record(record)
            dataitems = record["DataItems"]
            dataitems.each { |item|
                timeticks = GetTimeStampTicks(item["Timestamp"])
                hostname = item["Host"]
                objname = item["ObjectName"]
                instname = item["InstanceName"]
                collections = item["Collections"]

                collections.each { |collection|
                    handle_metrix(timeticks, hostname, objname, instname,
                                  collection["CounterName"], collection["Value"])
                }
            }
        end

        # Create a new MDM value with objname as Mdm namespace, counter name as metrics Name,
        # Counter value as metrics raw value.
        # examples:
        # ticks, computer1, Memory, Memory, "Available MBytes Memory", 1023
        # ticks, computer2, Processor, 1, "% Processor Time", 3
        def handle_metrix(timeticks, hostname, objname, instname, countername, countervalue)
            dimval2 = objname
            if objname != instname
                dimval2 += instname
            end
            @log.debug "Instance: #{dimval2}; Metrix: #{countername}; V: #{countervalue}"
            mdm2dkey = objname + "::" + countername
            m = @@mdm2dtable[mdm2dkey]
            if m == nil
                @@mdm2dtable[mdm2dkey] = Libifxext::Mdm2D.new(mdm_account, objname, countername, "Hostname", "InstanceName")
                m = @@mdm2dtable[mdm2dkey]
            end
            m.LogValueAtTime(timeticks, countervalue.to_i(), hostname, dimval2)
        end

        # Get number of ticks for a given UTC time since 1601/01/01 00:00:00.
        # example input: "2016-06-28T21:58:24.677Z", returns: 131116247046770000
        def GetTimeStampTicks(timestampStr)
            baseticks= Time.utc(1601,1,1).to_i() * 10000000
            t1 = Time.parse(timestampStr)
            ticks = t1.to_i() * 10000000 + t1.nsec/100 - baseticks
            return ticks
        end


    end # class OutputMDM
end  # module Fluent
