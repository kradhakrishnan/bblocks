#include <iostream>
#include <memory>

#include "test/unit/unit-test.h"
#include "net/event-bus/data.hpp"
#include "net/event-bus/packet.hpp"

using namespace std;
using namespace bblocks;

static string _log("test/net/event-bus/test_data");

// ............................................................................. test_datatypes ....

struct Data : NetPacket
{
	Data()
	{
		NetPacket::Add(i16_);
		NetPacket::Add(i32_);
		NetPacket::Add(i64_);
		NetPacket::Add(str_);
		NetPacket::Add(lu32_);
		NetPacket::Add(lu64_);
		NetPacket::Add(lstr_);
		NetPacket::Add(raw_);
	}

	UInt16 i16_;
	UInt32 i32_;
	UInt64 i64_;
	String str_;
	List<UInt32> lu32_;
	List<UInt64> lu64_;
	List<String> lstr_;
	Raw<10> raw_;
};

static void
test_datatypes()
{
	Data data;
	size_t pos;

	List<UInt32> lu32({ UInt32(2), UInt32(4), UInt32(16) });
	List<UInt64> lu64({ UInt64(32), UInt64(64), UInt64(128) });
	List<String> lstr({ String("a"), String("b"), String("c") });
	uint8_t rawdata[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	Raw<10> raw(rawdata);

	data.i16_.Set(257);
	data.i32_.Set(55);
	data.i64_.Set(555);
	data.str_.Set("5555");
	data.lu32_.Set(lu32);
	data.lu64_.Set(lu64);
	data.lstr_.Set(lstr);
	data.raw_.Set(rawdata);

	INFO(_log) << "Encoding. size=" << data.Size();

	IOBuffer buf = IOBuffer::Alloc(data.Size());
	pos = 0;
	data.Encode(buf, pos);

	INFO(_log) << "Decoding";

	Data data2;
	pos = 0;
	data2.Decode(buf, pos);

	INFO(_log) << "Checking data";

	INVARIANT(data2.i16_ == 257);
	INVARIANT(data2.i32_ == 55);
	INVARIANT(data2.i64_ == 555);
	INVARIANT(data2.str_ == "5555");
	INVARIANT(data2.lu32_ == lu32);
	INVARIANT(data2.lu64_ == lu64);
	INVARIANT(data2.lstr_ == lstr);
	INVARIANT(data2.raw_ == rawdata);
}

//........................................................................................ main ....

int
main(int argc, char ** argv)
{
	srand(time(NULL));

	InitTestSetup();

	TEST(test_datatypes);

	TeardownTestSetup();

	return 0;
}
