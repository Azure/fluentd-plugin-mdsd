require "digest"
require "fileutils"
require "json"
require "thread"
require "tmpdir"

module Fluent
    class MdsdChunkQuarantine
        class PersistenceError < StandardError; end

        def initialize(path, plugin_id, worker_id)
            @plugin_id = plugin_id
            @worker_id = worker_id
            @directory = File.join(path, "worker#{worker_id}", Digest::SHA256.hexdigest(plugin_id))
            @mutex = Mutex.new
        end

        def persist(chunk_id, plan)
            unless chunk_id.is_a?(String) && chunk_id.bytesize == 16
                raise PersistenceError, "MDSD quarantine requires a 16-byte chunk unique_id"
            end
            identity = chunk_id.unpack("H*").first
            manifest = {
                "version" => 1,
                "chunk_id" => identity,
                "plugin_id" => @plugin_id,
                "worker_id" => @worker_id,
                "bytes" => plan.data.bytesize,
                "sha256" => Digest::SHA256.hexdigest(plan.data),
                "plan" => plan.manifest
            }
            encoded = JSON.generate(manifest) + "\n"
            destination = File.join(@directory, identity)
            @mutex.synchronize do
                ensure_directory(@directory)
                unless File.exist?(destination)
                    temporary = Dir.mktmpdir(".#{identity}-", @directory)
                    begin
                        write_synced(File.join(temporary, "chunk.msgpack"), plan.data)
                        write_synced(File.join(temporary, "manifest.json"), encoded)
                        sync_directory(temporary)
                        begin
                            File.rename(temporary, destination)
                        rescue Errno::EEXIST, Errno::ENOTEMPTY
                            # A concurrent retry must verify the already published entry below.
                        end
                    ensure
                        FileUtils.remove_entry_secure(temporary) if File.directory?(temporary)
                    end
                end
                verify_entry(destination, encoded, manifest)
                sync_directory(@directory)
            end
            destination
        rescue SystemCallError, IOError => error
            raise PersistenceError, "MDSD quarantine persistence failed (#{error.class}): #{error.message}"
        end

        private

        def ensure_directory(path)
            parent = File.dirname(path)
            ensure_directory(parent) unless parent == path
            if File.exist?(path)
                unless File.directory?(path) && !File.symlink?(path)
                    raise PersistenceError, "MDSD quarantine path is not a real directory: #{path}"
                end
                sync_directory(path)
                return
            end
            begin
                Dir.mkdir(path, 0700)
            rescue Errno::EEXIST
                unless File.directory?(path) && !File.symlink?(path)
                    raise PersistenceError, "MDSD quarantine directory changed during creation: #{path}"
                end
            end
            sync_directory(path)
            sync_directory(parent)
        end

        def write_synced(path, data)
            File.open(path, File::WRONLY | File::CREAT | File::EXCL, 0600) do |file|
                file.write(data)
                file.flush
                sync_file(file)
            end
        end

        def sync_file(file)
            file.fsync
        end

        def sync_directory(path)
            File.open(path, File::RDONLY) { |directory| directory.fsync }
        end

        def verify_entry(path, encoded, manifest)
            raw_path = File.join(path, "chunk.msgpack")
            manifest_path = File.join(path, "manifest.json")
            unless File.directory?(path) && !File.symlink?(path) &&
                    [raw_path, manifest_path].all? { |file| File.file?(file) && !File.symlink?(file) }
                raise PersistenceError, "MDSD quarantine entry is incomplete or unsafe: #{path}"
            end
            unless File.size(manifest_path) == encoded.bytesize &&
                    File.binread(manifest_path) == encoded &&
                    File.size(raw_path) == manifest["bytes"] &&
                    Digest::SHA256.file(raw_path).hexdigest == manifest["sha256"]
                raise PersistenceError, "MDSD quarantine entry conflicts with the buffered chunk: #{path}"
            end
            [raw_path, manifest_path].each do |file|
                File.open(file, File::RDONLY) { |io| sync_file(io) }
            end
            sync_directory(path)
        end
    end
end
