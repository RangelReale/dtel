#pragma once

#include "IntrusiveRefCntPtr.h"
#include "detail/optional.hpp"

#include <duktape.h>

#include <memory>
#include <chrono>

namespace dtel {

class LoopRunner : public ThreadSafeRefCountedBase<LoopRunner>
{
public:
	typedef IntrusiveRefCntPtr<LoopRunner> Ptr;
	typedef std::experimental::optional<std::chrono::steady_clock::time_point> looped_result_t;

	virtual ~LoopRunner() {}

	/**
	 * Called on each loop.
	 * May return an optional time point, indicating the minimum time point that this runner expect to be called again.
	 */
	virtual looped_result_t looped(duk_context *ctx) = 0;
};

}