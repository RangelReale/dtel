#pragma once

#include "Event.h"
#include "Task.h"
#include "LoopRunner.h"
#include "Ref.h"
#include "ResetStackOnScopeExit.h"
#include "Value.h"
#include "ValueObject.h"
#include "Exception.h"
#include "detail/refs.h"
#include "detail/ctpl_stl.h"

#include <duktape.h>

#include <list>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <iostream>

namespace dtel {

class EventLoop
{
public:
	/**
	 * Constructor
	 */
	EventLoop(duk_context *ctx) : 
		_ctx(ctx), _mutex(), _terminated(false), _events(), _tasks(3)
	{
		detail::duv_ref_setup(ctx);
	}

	/**
	 * Destructor
	 */
	virtual ~EventLoop() {}

	/**
	 * Returns the duktape context
	 */
	duk_context *ctx() const 
	{ 
		return _ctx; 
	}

	/**
	 * Notify that the event list must be re-evaluated.
	 */
	void notifyChanged()
	{
		_events_cv.notify_one();
	}

	/**
	 * Post an event on the loop
	 */
	void postEvent(Event::Ptr event)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(_mutex);
			_events.push_back(event);
		}
		notifyChanged();
	}

	void postTask(Task::Ptr task)
	{
		_tasks.push([task](int id) { task->run(); });
	}

	/**
	 * Lower priority means higher priority, 1 being more priority, 100 being less
	 */
	void addLoopRunner(LoopRunner::Ptr looprunner, int priority = 50)
	{
		std::lock_guard<std::recursive_mutex> lock(_mutex);
		_looprunners.emplace_back(priority, looprunner);
		_looprunners.sort([](const auto &first, const auto &second) {
			return first.first < second.first;
		});
	}

	/**
	 * Runs the event loop
	 */
	void run()
	{
		_terminated = false;
		while (!_terminated)
		{
			auto now = std::chrono::steady_clock::now();
			// wait 2000 ms by default
			auto timeout(now + std::chrono::milliseconds(2000));

			// loop runners
			{
				std::unique_lock<std::recursive_mutex> lock(_mutex);
				for (auto lr : _looprunners)
				{
					auto newtimeout = lr.second->looped(_ctx);
					if (newtimeout && *newtimeout < timeout)
						timeout = *newtimeout;
				}
			}

			// run events
			while (true)
			{
				Event::Ptr event;

				{
					// retrieve the first event
					std::unique_lock<std::recursive_mutex> lock(_mutex);
					if (!_events.empty())
					{
						event = _events.front();
						_events.pop_front();
					}
				}

				if (event)
				{
					ResetStackOnScopeExit r(_ctx);

					// call event
					try
					{
						event->apply(_ctx);
					}
					catch (std::exception &e) 
					{
						if (!processException(e))
							throw;
					}

					// release event
					try
					{
						event->release(_ctx);
					}
					catch (std::exception &e)
					{
						if (!processException(e))
							throw;
					}
				}
				else
					break;
			}


			{
				// sleep the time needed for the next event
				std::unique_lock<std::mutex> cvlock(_events_mt);
				
				//std::cout << "--- SLEEP FOR " << std::chrono::duration_cast<std::chrono::milliseconds>(timeout - now).count() << std::endl;
				_events_cv.wait_until(cvlock, timeout);
			}
	
		}

		std::unique_lock<std::recursive_mutex> lock(_mutex);
		_events.clear();
	}

	/**
	 * Terminate the message loop
	 */
	void terminate()
	{
		_terminated = true;
	}

	/**
	 * Process an exception.
	 * Returning false will rethrow the exception
	 */
	virtual bool processException(const std::exception &e)
	{
		return false;
	}

	/**
	 * Sets the task thread count
	 */
	void setTaskThreadCount(int count)
	{
		_tasks.resize(count);
	}

private:
	typedef std::list<Event::Ptr> events_t;
	typedef std::list<std::pair<int, LoopRunner::Ptr>> looprunners_t;

	duk_context *_ctx;
	std::recursive_mutex _mutex;
	std::atomic_bool _terminated;
	events_t _events;
	std::mutex _events_mt;
	std::condition_variable _events_cv;
	looprunners_t _looprunners;
	ctpl::thread_pool _tasks;
};

}