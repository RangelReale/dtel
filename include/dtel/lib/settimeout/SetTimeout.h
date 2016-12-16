#pragma once

#include <dtel.h>

#include <duktape.h>

#include <iostream>
#include <list>
#include <chrono>
#include <mutex>

namespace dtel {
namespace settimeout {

namespace detail {
	static const char* PROP_EL = "\xFF" "DTEL_SETTIMEOUT";
	static const char* PROP_ELHANDLER = "\xFF" "DTEL_SETTIMEOUT_HANDLER";
	static const char* PROP_ELREFS = "\xFF" "DTEL_SETTIMEOUT_REFS";
}

/**
 * Timeout event
 */
class TimeoutEvent : public Event
{
public:
	typedef IntrusiveRefCntPtr<TimeoutEvent> TPtr;

	TimeoutEvent(std::chrono::milliseconds delay, bool oneShot = true) :
		Event(), _delay(delay), _oneshot(oneShot), _id(-1), _removed(false)
	{

	}

	int id() const
	{
		return _id;
	}

	void setId(int id)
	{
		_id = id;
	}

	bool removed() const
	{
		return _removed;
	}

	void setRemoved(bool value)
	{
		_removed = value;
	}

	bool oneShot() const
	{
		return _oneshot;
	}
	
	std::chrono::milliseconds delay() const
	{
		return _delay;
	}
private:
	std::chrono::milliseconds _delay;
	bool _oneshot;
	int _id;
	bool _removed;
};

/**
 * SetTimeout handler
 */
class SetTimeoutHandler : public ThreadSafeRefCountedBase<SetTimeoutHandler>
{
public:
	typedef IntrusiveRefCntPtr<SetTimeoutHandler> Ptr;

	SetTimeoutHandler(EventLoop *eventloop) :
		_eventloop(eventloop), _id(0)
	{

	}

	/**
	 * Post a timeout event
	 * Returns a timeout id that can be used to cancel the event
	 */
	int postEvent(TimeoutEvent::TPtr event)
	{
		std::lock_guard<std::recursive_mutex> lock(_mutex);
		int newid;
		if (event->id() <= 0)
		{
			newid = ++_id;
			event->setId(newid);
		}
		else
			newid = event->id();
		_events.emplace_back(std::chrono::steady_clock::now() + event->delay(), event);
		_events.sort([](const auto &first, const auto &second) {
			return first.first < second.first;
		});
		// wake the event loop to process the possibly new timeout
		_eventloop->notifyChanged();
		return newid;
	}

	/**
	 * Cancels an event by id
	 */
	bool cancelEvent(int id)
	{
		std::lock_guard<std::recursive_mutex> lock(_mutex);
		for (auto i = _events.begin(); i != _events.end(); i++)
		{
			if (i->second->id() == id)
			{
				i->second->setRemoved(true);
				return true;
			}
		}
		return false;
	}

	EventLoop *eventLoop() const 
	{
		return _eventloop;
	}

	/**
	 * Check for expired timeouts and post to the event loop.
	 * Also removes removed timeout events.
	 */
	LoopRunner::looped_result_t checkTimeouts(duk_context *ctx)
	{
		static const int MAX_EXPIRYS = 10;

		auto now = std::chrono::steady_clock::now();
		int sanity = MAX_EXPIRYS;
		while (sanity-- > 0)
		{
			TimeoutEvent::TPtr event;
			{
				// gets the first available expired or removed event
				std::lock_guard<std::recursive_mutex> lock(_mutex);
				if (!_events.empty())
				{
					bool expired = _events.front().first < now;
					if (_events.front().second->removed() || expired)
					{
						event = _events.front().second;
						_events.pop_front();
					}
				}
			}

			if (event)
			{
				if (!event->removed())
				{
					// expired, post to EventLoop
					_eventloop->postEvent(event);
				}
				else
				{
					// removed, release directly
					event->release(ctx);
				}
			}
		}

		/**
		 * Returns the expiration of the first item from list, if available
		 */
		LoopRunner::looped_result_t ret;
		{
			std::lock_guard<std::recursive_mutex> lock(_mutex);
			if (!_events.empty())
				ret = _events.front().first;
		}
		return ret;
	}
private:
	typedef std::list<std::pair<std::chrono::steady_clock::time_point, TimeoutEvent::TPtr>> events_t;

	EventLoop *_eventloop;
	std::recursive_mutex _mutex;
	events_t _events;
	int _id;
};

namespace detail {

	/**
	 * Storage of the handler inside duktape
	 */
	struct SetTimeoutHandlerStorage
	{
		SetTimeoutHandler::Ptr handler;
	};

	/**
	 * Gets the handler from the context
	 */
	inline SetTimeoutHandlerStorage *handler_from_ctx(duk_context *ctx)
	{
		duk_push_heap_stash(ctx);
		// object on stash
		duk_get_prop_string(ctx, -1, PROP_ELHANDLER); 
		// property on object
		duk_get_prop_string(ctx, -1, PROP_ELHANDLER);
		SetTimeoutHandlerStorage *ret = static_cast<SetTimeoutHandlerStorage*>(duk_get_pointer(ctx, -1));
		duk_pop_3(ctx);
		return ret;
	}

	/**
	 * Loop runner, checks for timer expiration
	 */
	class ST_LoopRunner : public LoopRunner
	{
	public:
		ST_LoopRunner(SetTimeoutHandler::Ptr handler) :
			LoopRunner(), _handler(handler)
		{

		}

		looped_result_t looped(duk_context *ctx)
		{
			return _handler->checkTimeouts(ctx);
		}
	private:
		SetTimeoutHandler::Ptr _handler;
	};

	/**
	 * Timeout event used by the setTimeout/setInterval functions
	 */
	class DefaultTimeoutEvent : public TimeoutEvent
	{
	public:
		DefaultTimeoutEvent(SetTimeoutHandler::Ptr handler, std::chrono::milliseconds delay, bool oneShot = true) :
			TimeoutEvent(delay, oneShot), _handler(handler) {}

		void apply(duk_context *ctx) override
		{
			if (!removed() && id() > 0)
			{
				//std::cout << "**** CALLING " << id() << " DELAY " << delay().count() << std::endl;

				// call function stored on the REFS array
				duk_push_heap_stash(ctx);
				duk_get_prop_string(ctx, -1, PROP_ELREFS);
				// get function using id() as index on the array
				duk_get_prop_index(ctx, -1, id());

				duk_remove(ctx, -2); // remove ELREFS
				duk_remove(ctx, -2); // remove stash

				// call the function
				duk_int_t callret = duk_pcall(ctx, 0);

				// remove if oneshot or removed by the call above, re-add otherwise
				if (oneShot())
				{
					setRemoved(true);
				}
				else if (!removed())
				{
					_handler->postEvent(this);
				}

				// check result
				if (callret != DUK_EXEC_SUCCESS) 
				{
					ThrowError(ctx, -1);
				}

				// pop pcall result
				duk_pop(ctx);
			}
		}

		void release(duk_context *ctx) override
		{
			if (removed())
			{
				//std::cout << "&&&& RELEASING " << id() << " DELAY " << delay().count() << std::endl;

				// remove the function from the refs array using id()
				duk_push_heap_stash(ctx);
				duk_get_prop_string(ctx, -1, PROP_ELREFS);
				duk_del_prop_index(ctx, -1, id());
				duk_pop_2(ctx);
			}
		}
	private:
		SetTimeoutHandler::Ptr _handler;
	};

	//
	// setTimeout / setInterval function definitions
	//
	duk_ret_t r_setTimeoutBase(duk_context *ctx, bool oneShot)
	{
		duk_idx_t args = duk_get_top(ctx);
		if (args < 2)
			duk_error(ctx, DUK_ERR_ERROR, "setTimeout requires at least 2 parameters");

		// get the SetTimeoutHandler
		SetTimeoutHandlerStorage *h = handler_from_ctx(ctx);

		// get the parameters
		duk_require_function(ctx, 0);
		int delay = duk_require_int(ctx, 1);

		// TODO: the next parameters should be sent to the function

		// post the timer
		int id = h->handler->postEvent(new detail::DefaultTimeoutEvent(h->handler, std::chrono::milliseconds(delay), oneShot));

		// reference function using id as index on the REFS array
		duk_push_heap_stash(ctx);
		duk_get_prop_string(ctx, -1, PROP_ELREFS);
		duk_dup(ctx, 0); // push function
		duk_put_prop_index(ctx, -2, id);
		duk_pop_2(ctx);

		// return id
		duk_push_int(ctx, id);
		return 1;
	}

	duk_ret_t r_clearTimeoutBase(duk_context *ctx, bool oneShot)
	{
		// get the SetTimeoutHandler
		SetTimeoutHandlerStorage *h = handler_from_ctx(ctx);

		// get the parameters
		int id = duk_require_int(ctx, 0);

		// cancel the timer
		bool ret = h->handler->cancelEvent(id);

		// return if the event was cancelled
		duk_push_boolean(ctx, ret);
		return 1;
	}

	duk_ret_t r_setTimeout(duk_context *ctx)
	{
		return r_setTimeoutBase(ctx, true);
	}

	duk_ret_t r_setInterval(duk_context *ctx)
	{
		return r_setTimeoutBase(ctx, false);
	}

	duk_ret_t r_clearTimeout(duk_context *ctx)
	{
		return r_clearTimeoutBase(ctx, true);
	}

	duk_ret_t r_clearInterval(duk_context *ctx)
	{
		return r_clearTimeoutBase(ctx, false);
	}

	duk_ret_t r_setTimeout_Finalizer(duk_context *ctx)
	{
		// 0 = object to finalize
		duk_get_prop_string(ctx, 0, PROP_ELHANDLER);
		if (duk_is_pointer(ctx, -1) != 0)
		{
			SetTimeoutHandlerStorage* p = static_cast<SetTimeoutHandlerStorage*>(duk_get_pointer(ctx, -1));
			delete p;
		}
		duk_pop(ctx);
		duk_del_prop_string(ctx, 0, PROP_ELHANDLER);
		return 0;
	}

	void r_setTimeout_Setup(SetTimeoutHandler::Ptr handler)
	{
		duk_context *ctx = handler->eventLoop()->ctx();

		// register the refs array on the stash if don't exists
		duk_push_heap_stash(ctx);
		if (duk_has_prop_string(ctx, -1, PROP_ELREFS) == 0)
		{
			duk_push_array(ctx);
			duk_put_prop_string(ctx, -2, PROP_ELREFS);
		}
		duk_pop(ctx);

		// register the handler to the stash
		SetTimeoutHandlerStorage *h = new SetTimeoutHandlerStorage{ handler };
		duk_push_heap_stash(ctx);
		// object container to allow finalizer
		duk_push_object(ctx);
		// pointer into object
		duk_push_pointer(ctx, h);
		duk_put_prop_string(ctx, -2, PROP_ELHANDLER);
		// set object finalizer
		duk_push_c_function(ctx, &r_setTimeout_Finalizer, 1);
		duk_set_finalizer(ctx, -2);
		// put object into stash using the same property name
		duk_put_prop_string(ctx, -2, PROP_ELHANDLER);
		duk_pop(ctx);

		duk_push_global_object(ctx);

		// function: setTimeout
		duk_push_c_function(ctx, &r_setTimeout, DUK_VARARGS);
		duk_put_prop_string(ctx, -2, "setTimeout");

		// function: setInterval
		duk_push_c_function(ctx, &r_setInterval, DUK_VARARGS);
		duk_put_prop_string(ctx, -2, "setInterval");

		// function: clearTimeout
		duk_push_c_function(ctx, &r_clearTimeout, 1);
		duk_put_prop_string(ctx, -2, "clearTimeout");

		// function: clearInterval
		duk_push_c_function(ctx, &r_clearInterval, 1);
		duk_put_prop_string(ctx, -2, "clearInterval");

		// pop global object
		duk_pop(ctx);
	}
}

/**
 * Register the setTimeout handling on the event loop
 */
inline SetTimeoutHandler::Ptr RegisterSetTimeout(EventLoop *eventloop)
{
	duk_context *ctx = eventloop->ctx();
	ResetStackOnScopeExit r(ctx);

	SetTimeoutHandler::Ptr handler(new SetTimeoutHandler(eventloop));

	// register the functions
	detail::r_setTimeout_Setup(handler);

	// must be a very low priority, must be run prior to almost everything
	eventloop->addLoopRunner(new detail::ST_LoopRunner(handler), 5);

	// return the handler
	return handler;
}

} }