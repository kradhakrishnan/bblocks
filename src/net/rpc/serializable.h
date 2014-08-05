#pragma once

#include <vector>

#include "net/rpc/rpc-data.h"

namespace dh_core {

using namespace std;

class Serializable : public RPCData
{
public:

	void Add(RPCData * var)
	{
		vars_.push_back(var);
	}

	virtual void Encode(IOBuffer & buf, size_t & pos) override
	{
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			auto rpcdata = *it;
			rpcdata->Encode(buf, pos);
		}
	}

	virtual void Decode(IOBuffer & buf, size_t & pos) override
	{
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			auto rpcdata = *it;
			rpcdata->Decode(buf, pos);
		}
	}

	virtual size_t Size() const override
	{
		size_t size = 0;
		for (auto it = vars_.begin(); it != vars_.end(); ++it) {
			auto rpcdata = *it;
			size += rpcdata->Size();
		}

		return size;
	}

private:

    vector<RPCData *> vars_;
};

}
