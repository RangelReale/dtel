#pragma once

#include "IntrusiveRefCntPtr.h"

#include <duktape.h>

#include <memory>

namespace dtel {

class Task : public ThreadSafeRefCountedBase<Task>
{
public:
	typedef IntrusiveRefCntPtr<Task> Ptr;

	virtual ~Task() {}

	/**
	 * Execute the task
	 */
	virtual void run() = 0;
};

}