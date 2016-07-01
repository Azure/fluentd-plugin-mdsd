# This is MDM output plugin: data will be sent to MDM through IFX API.
# Only OMI data are supported.
module Fluent
    class OutputMDM < BufferedOutput
        Plugin.register_output('out_mdm', self)

        def initialize
            super
            require_relative 'omslog'
            require_relative 'oms_configuration'
            require_relative 'oms_common'
            require_relative 'libifxext'
            require 'time'
            ENV["RTCPAL_LOG_LEVEL"] = "2"
            ENV["RTCPAL_LOG_IDENT"] = "ruby"
            @@mdm2dtable = {}
        end

        config_param :omsadmin_conf_path, :string, :default => '/etc/opt/microsoft/omsagent/conf/omsadmin.conf'
        config_param :cert_path, :string, :default => '/etc/opt/microsoft/omsagent/certs/oms.crt'
        config_param :key_path, :string, :default => '/etc/opt/microsoft/omsagent/certs/oms.key'
        config_param :mdm_account, :string

        def configure(conf)
            super
        end

        def start
            super
        end

        def shutdown
            super
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
            # Quick exit if we are missing something
            #if !OMS::Configuration.load_configuration(omsadmin_conf_path, cert_path, key_path)
            #    raise 'Missing configuration. Make sure to onboard. Will continue to buffer data.'
            #end

            # handle OMI datatype only
            chunk.msgpack_each {|(tag, record)|
                if tag == 'oms.omi'
                    @log.debug "Start to process #{tag} data ..."
                    handle_record(record)
                else
                    @log.error "Error: unsupported tag #{tag}. Ignore data."
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

        # examples:
        # computer1, Memory, Memory, "Available MBytes Memory", 1023
        # computer2, Processor, 1, "% Processor Time", 3
        def handle_metrix(timeticks, hostname, objname, instname, itemname, itemvalue)
            @log.debug "Metrix: Name: #{itemname}; Value: #{itemvalue}"
            dimval2 = objname
            if objname != instname
                dimval2 += instname
            end
            mdm2dkey = objname + "::" + itemname
            m = @@mdm2dtable[mdm2dkey]
            if m == nil
                @@mdm2dtable[mdm2dkey] = Libifxext::Mdm2D.new(mdm_account, objname, itemname, "Hostname", "InstanceName")
                m = @@mdm2dtable[mdm2dkey]
            end
            m.LogValueAtTime(timeticks, itemvalue.to_i(), hostname, dimval2)
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
