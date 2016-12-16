#pragma once

#include "IntrusiveRefCntPtr.h"
#include "detail/any.hpp"

#include <duktape.h>

namespace dtel {

class Value : public ThreadSafeRefCountedBase<Value>
{
public:
	typedef IntrusiveRefCntPtr<Value> Ptr;

	virtual ~Value() {}

	/**
	 * Push the value. Returns the amount of pushed values
	 */
	virtual int push(duk_context *ctx) = 0;
};

class ValueGlobal : public Value
{
public:
	int push(duk_context *ctx)
	{
		duk_push_global_object(ctx);
		return 1;
	}
};

}