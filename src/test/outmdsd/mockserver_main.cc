#include <iostream>
#include <future>

extern "C" {
#include <unistd.h>
}

#include "MockServer.h"

struct CmdArgs {
    uint32_t timeBeforeDisconnect; // milli-seconds
    uint32_t timeToDisconnect; // milli-seconds
    std::string socketFile;
};

void Usage(const std::string & progname)
{
    std::cout << "Usage:" << std::endl;
    std::cout << std::string("  ") + progname + " [options]" << std::endl;
    std::cout << "    -b <ms>          : Wait for <ms> milli-seconds before disconnect socket." << std::endl;
    std::cout << "    -d <ms>          : Disconnect socket after <ms> milli-seconds." << std::endl;
    std::cout << "    -u <socketFile>  : Listen to a Unix socket file. Create it if not exists." << std::endl;
}

CmdArgs
ParseCmdLine(int argc, char** argv)
{
    if (argc <= 1) {
        Usage(argv[0]);
        exit(1);
    }

    CmdArgs cmdargs;
    int opt = 0;
    while((opt = getopt(argc, argv, "b:d:u:")) != -1) {
        switch(opt) {
        case 'b':
            cmdargs.timeBeforeDisconnect = atoi(optarg);
            break;
        case 'd':
            cmdargs.timeToDisconnect = atoi(optarg);
            break;
        case 'u':
            cmdargs.socketFile = optarg;
            break;
        default:
            std::cout << "Error: unexpected cmd option: " << opt << std::endl;
            Usage(argv[0]);
            exit(1);
        }
    }
    return cmdargs;
}

bool
RunMockServer(
    const CmdArgs& cmdargs
    )
{
    try {
        TestUtil::MockServer mockServer(cmdargs.socketFile);

        auto restartTask = std::async(std::launch::async, [&mockServer, cmdargs] () {
            usleep(cmdargs.timeBeforeDisconnect*1000);
            mockServer.DisconnectAndRun(cmdargs.timeToDisconnect);
        });

        mockServer.Init();
        mockServer.Run();
        restartTask.wait();

        return true;
    }
    catch(const std::exception & ex) {
        std::cout << "Error: RunMockServer exception: " << ex.what() << std::endl;
    }
    return false;
}

int main(int argc, char** argv)
{
    auto cmdargs = ParseCmdLine(argc, argv);

    if (!RunMockServer(cmdargs)) {
        return 1;
    }

    return 0;
}
