#include <iostream>
#include <boost/program_options.hpp>
#include <string>

#include "core/defs.h"
#include "extentfs/extentfs.h"
#include "extentfs/disklayout.h"
#include "extentfs/logoff.h"
#include "extentfs/btree.h"

using namespace std;
using namespace extentfs;
using namespace dh_core;

namespace po = boost::program_options;


// .... ExtentFs ...............................................................

ExtentFs::ExtentFs(BlockDevice * dev, const size_t pageSize)
    : log_("/extentFs")
    , dev_(dev)
    , pageSize_(pageSize)
{
    INFO(log_) << "ExtentFs :"
               << " dev: " << dev_
               << " pageSize: " << pageSize;
}

ExtentFs::~ExtentFs()
{
    dev_ = NULL;
}

void ExtentFs::Create(const CHandler<int> & h)
{
    FormatHelper * fhelper = new FormatHelper(*this);

    ThreadPool::Schedule(fhelper, &FormatHelper::Start, /*quck_format=*/ true,
                         async_fn(this, &ExtentFs::CreateDone));
}

void ExtentFs::CreateDone(int status)
{
    DEADEND
}

void ExtentFs::Open(const CHandler<int> & h)
{
    DEADEND
}

void FormatHelper::Start(const bool isQuickFormat, CHandler<int> h)
{
    // Layout the extent file system
    // 1. Layout superblock
    // 2. Layout extent index root

}

// ................................................................... main ....

int
main(int argc, char ** argv)
{
    string devPath;
    disksize_t devSizeGiB;
    size_t pageSize;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "Print usage")
        ("devpath", po::value<string>(&devPath)->required(), "Device path")
        ("devsize", po::value<disksize_t>(&devSizeGiB)->required(), 
         "Device size in GiB")
        ("pgsize", po::value<size_t>(&pageSize)->required(), "io size in B");

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

    // check for config errors


    // start extent fs

    if (showUsage) {
        cout << desc << endl;
        return 0;
    }

    // normal initialization


    return 0;
}
