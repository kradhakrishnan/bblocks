#include "net/event-bus/data.hpp"

using namespace bblocks;

void
String::Encode(IOBuffer & buf, size_t & pos)
{
	buf.UpdateInt<uint32_t>(v_.size(), pos);
	for (size_t i = 0; i < v_.size(); ++i) {
		buf.Update(v_[i], pos);
	}
}

void
String::Decode(IOBuffer & buf, size_t & pos)
{
	uint32_t size;
	buf.ReadInt(size, pos);

	v_.resize(size);

	for (size_t i = 0; i < size; ++i) {
		buf.Read(v_[i], pos);
	}
}

