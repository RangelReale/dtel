#pragma once

#include <dtel.h>
#include <dtel/detail/any.hpp>

namespace dtel {
namespace eventtarget {

/**
 * Value helper for an Event JS object
 */
class Event : public Value
{
public:
	typedef IntrusiveRefCntPtr<Event> Ptr;

	Value::Ptr target;
	std::string eventClass;
	std::string eventType;
	ValueObject eventInit;

	Event(const std::string &eventType, const std::string &eventClass = "Event", Value::Ptr target = Ref::Ptr()) :
		Value(), eventType(eventType), eventClass(eventClass), target(target)
	{

	}

	int push(duk_context *ctx) override
	{
		duk_push_global_object(ctx);
		// get constructor
		duk_get_prop_string(ctx, -1, eventClass.c_str());
		// remmove global object
		duk_remove(ctx, -2);
		// arg0: event type
		duk_push_string(ctx, eventType.c_str());
		// arg1: eventInit object
		int oc = eventInit.push(ctx);
		// creates the object with 'new'
		if (duk_pnew(ctx, 1 + oc) != 0) {
			ThrowError(ctx, -1);
		}
		// set "target"
		if (target) {
			target->push(ctx);
			duk_put_prop_string(ctx, -2, "target");
		}
		return 1;
	}
};

inline bool IsEventTarget(duk_context *ctx)
{
	duk_push_global_object(ctx);
	bool isEventTarget = duk_has_prop_string(ctx, -1, "EventTarget") != 0;
	duk_pop(ctx);
	return isEventTarget;
}

inline void EventTarget_dispatchEvent(duk_context *ctx, Value::Ptr target, Event::Ptr event)
{
	target->push(ctx);

	// call 'dispatchEvent' on the event
	duk_push_string(ctx, "dispatchEvent");
	if (!event->target)
		event->target = target;
	event->push(ctx);
	duk_int_t status = duk_pcall_prop(ctx, -3, 1);
	if (status != 0) {
		ThrowError(ctx, -1);
	}
	// pop call result, ref
	duk_pop_2(ctx); 
}

inline void RegisterEventTarget(EventLoop *eventloop)
{
	duk_context *ctx = eventloop->ctx();
	ResetStackOnScopeExit r(ctx);

	//
	// Event
	//
	if (duk_peval_string(eventloop->ctx(), R"(

var Event = function(typeArg, eventInit) {	
	this.type = typeArg;
	if (typeof eventInit == "object") {
		//if (eventInit.bubbles != undefined) this.bubbles = eventInit.bubbles;
		//if (eventInit.cancelable != undefined) this.cancelable = eventInit.cancelable;
		for (var k in eventInit) {
			this[k] = eventInit[k];
		}
	}
};

Event.prototype.bubbles = false;
Event.prototype.cancelable = false;
Event.prototype.currentTarget = null;
Event.prototype.defaultPrevented = false;
Event.prototype.eventPhase = null;
Event.prototype.target = null;
Event.prototype.timeStamp = null;
Event.prototype.type = null;
Event.prototype.isTrusted = null;

Event.prototype.preventDefault = function()
{
	this.defaultPrevented = true;
};

Event.prototype.stopImmediatePropagation = function()
{
};

Event.prototype.stopPropagation = function()
{
	this.defaultPrevented = true;
};

	)") != 0)
	{
		ThrowError(ctx, -1);
	};
	duk_pop(ctx);

	//
	// ErrorEvent
	//
	if (duk_peval_string(eventloop->ctx(), R"(

var ErrorEvent = function() {	
	Event.apply(this, arguments);
};

ErrorEvent.prototype.message = null;
ErrorEvent.prototype.filename = null;
ErrorEvent.prototype.lineno = null;
ErrorEvent.prototype.colno = null;
ErrorEvent.prototype.error = null;

ErrorEvent.prototype = Object.create(Event.prototype);
ErrorEvent.prototype.constructor = ErrorEvent;

	)") != 0)
	{
		ThrowError(ctx, -1);
	};
	duk_pop(ctx);

	//
	// EventTarget
	//

	if (duk_peval_string(eventloop->ctx(), R"(

var EventTarget = function() {
  this.listeners = {};
};

EventTarget.prototype.listeners = null;
EventTarget.prototype.addEventListener = function(type, callback) {
  if(!(type in this.listeners)) {
    this.listeners[type] = [];
  }
  this.listeners[type].push(callback);
};

EventTarget.prototype.removeEventListener = function(type, callback) {
  if(!(type in this.listeners)) {
    return;
  }
  var stack = this.listeners[type];
  for(var i = 0, l = stack.length; i < l; i++) {
    if(stack[i] === callback){
      stack.splice(i, 1);
      return this.removeEventListener(type, callback);
    }
  }
};

EventTarget.prototype.dispatchEvent = function(event) {
  if(!(event.type in this.listeners)) {
    return;
  }
  var stack = this.listeners[event.type];
  event.target = this;
  for(var i = 0, l = stack.length; i < l; i++) {
      stack[i].call(this, event);
  }
};

	)") != 0)
	{
		ThrowError(ctx, -1);
	};
	duk_pop(ctx);
}

} }