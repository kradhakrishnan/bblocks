#include "core/net/rpc.h"

using namespace dh_core;

void
String::Encode(NetBuffer & buf)
{
	buf.AppendInt<uint32_t>(v_.size());
	for (size_t i = 0; i < v_.size(); ++i) {
		buf.Append(v_[i]);
	}
}

void
String::Decode(NetBuffer & buf)
{
	uint32_t size;
	buf.ReadInt(size);

	v_.resize(size);

	for (size_t i = 0; i < size; ++i) {
		buf.Read(v_[i]);
	}
}

