#pragma once

#include <vector>

#include "net/rpc/rpc-data.h"

namespace bblocks {

using namespace std;
using namespace bblocks;

class Serializable : public RpcData
{
public:

	void Add(const RpcData & var)
	{
		vars_.push_back(var);
	}

	virtual void Encode(IOBuffer & buf, size_t & pos) override
	{
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			it->Encode(buf, pos);
		}
	}

	virtual void Decode(IOBuffer & buf, size_t & pos) override
	{
		for (auto it = vars_.beging(); it != vars_.end(); ++it) {
			it->Decode(buf, pos);
		}
	}

	virtual size_t Size() const override
	{
		size_t size = 0;
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			size += it->Size();
		}

		return size;
	}

private:

    vector<RpcData &> vars_;
};

}
