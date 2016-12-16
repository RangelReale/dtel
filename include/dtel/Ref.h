#pragma once

#include "IntrusiveRefCntPtr.h"
#include "Value.h"
#include "detail/refs.h"

#include <duktape.h>

#include <memory>

namespace dtel {

class Ref : public Value
{
public:
	typedef IntrusiveRefCntPtr<Ref> Ptr;

	Ref(duk_context *ctx) : _ctx(ctx)
	{
		_refid = detail::duv_ref(ctx);
		//std::cout << "&& REF " << _refid << std::endl;
	}

	virtual ~Ref()
	{
		//std::cout << "** UNREF " << _refid << std::endl;
		detail::duv_unref(_ctx, _refid);
	}

	int push(duk_context *ctx)
	{
		detail::duv_push_ref(ctx, _refid);
		return 1;
	}
private:
	duk_context *_ctx;
	int _refid;
};

}