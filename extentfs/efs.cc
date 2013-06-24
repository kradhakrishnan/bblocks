#include <iostream>
#include <boost/program_options.hpp>
#include <string>

#include "core/defs.h"
#include "extentfs/extentfs.h"

using namespace std;
using namespace extentfs;
using namespace dh_core;

namespace po = boost::program_options;

// ................................................................... main ....

void InitSetup()
{
    LogHelper::InitConsoleLogger();
    RRCpuId::Init();
    NonBlockingThreadPool::Init();
}

void TeardownSetup()
{
    NonBlockingThreadPool::Destroy();
    RRCpuId::Destroy();
    LogHelper::DestroyLogger();
}


int
main(int argc, char ** argv)
{
    string devPath;
    disksize_t devSizeGiB;
    size_t pageSize;
    size_t ncpu;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "Print usage")
        ("devpath", po::value<string>(&devPath)->required(), "Device path")
        ("devsize", po::value<disksize_t>(&devSizeGiB)->required(), 
         "Device size in GiB")
        ("pgsize", po::value<size_t>(&pageSize)->required(), "page size in B")
        ("ncpu", po::value<size_t>(&ncpu)->required(), "cores to use")
    ;
    po::variables_map parg;

    try {
        po::store(po::parse_command_line(argc, argv, desc), parg);
        po::notify(parg);
    } catch(...) {
        cout << "Error parsing the command arguments." << endl;
        cout << desc << endl;
        return -1;
    }

    // help command
    const bool showUsage = parg.count("help");
    if (showUsage) {
        cout << desc << endl;
        return 0;
    }

    // check for config errors


    // initialize threadpool
    InitSetup();
    ThreadPool::Start(ncpu);

    // start extent fs
    LinuxAioProcessor * aio = new LinuxAioProcessor();

    const disksize_t nsec = GiB(devSizeGiB) / SpinningDevice::SECTOR_SIZE;

    SpinningDevice * dev = new SpinningDevice(devPath, nsec, aio);
    int status = dev->OpenDevice();
    INVARIANT(status != -1);

    ExtentFs * efs = new ExtentFs(dev, pageSize);
    ThreadPool::Schedule(efs, &ExtentFs::Start, /*newstore=*/ true);

    // wait for thread pool to exit
    ThreadPool::Wait();

    // cleanup
    delete efs;
    delete dev;
    delete aio;

    TeardownSetup();

    return 0;
}
