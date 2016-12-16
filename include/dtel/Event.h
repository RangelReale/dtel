#pragma once

#include "IntrusiveRefCntPtr.h"

#include <duktape.h>

namespace dtel {

class Event : public ThreadSafeRefCountedBase<Event>
{
public:
	typedef IntrusiveRefCntPtr<Event> Ptr;

	virtual ~Event() {}

	/**
	 * Execute the event
	 */
	virtual void apply(duk_context *ctx) = 0;

	/**
	 * Release any event resources if needed
	 */
	virtual void release(duk_context *ctx) = 0;
};

}