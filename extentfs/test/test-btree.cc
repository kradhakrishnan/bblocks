#include "core/test/unit-test.h"
#include "extentfs/btree.h"
#include "extentfs/test/test-btree-helper.h"

using namespace std;
using namespace dh_core;
using namespace extentfs;

// ......................................................... BTreeBasicTest ....

class BTreeBasicTest
{
public:

    static const size_t PAGE_SIZE = 4 * 1024; // 4 KiB

    BTreeBasicTest()
        : btree_(dynamic_cast<BTreeIOProvider &>(iomgr_), PAGE_SIZE)
    {
    }

private:

    BTreeHelper iomgr_;
    BTree btree_;
};

void
test_btree_basic()
{
}

// ................................................................... main ....

int
main(int argc, char ** argv)
{
    InitTestSetup();

    TEST(test_btree_basic);

    TeardownTestSetup();

    return 0;
}
