#include <iostream>
#include <boost/program_options.hpp>
#include <string>

#include "core/defs.h"
#include "extentfs/logoff.h"
#include "extentfs/btree.h"

using namespace std;

namespace po = boost::program_options;

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

    if (showUsage) {
        cout << desc << endl;
        return 0;
    }

    // normal initialization


    return 0;
}
