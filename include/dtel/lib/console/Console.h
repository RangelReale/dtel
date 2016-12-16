#pragma once

#include <dtel.h>
#include <dtel/ResetStackOnScopeExit.h>

#include "Handler.h"
#include "detail/functions.h"

#include <duktape.h>

namespace dtel {
namespace console {

/**
* Register the console handling on the event loop
*/
inline ConsoleHandler::Ptr RegisterConsole(EventLoop *eventloop)
{
	duk_context *ctx = eventloop->ctx();
	ResetStackOnScopeExit r(ctx);

	//
	// Event
	//
	if (duk_peval_string(eventloop->ctx(), R"(

var console = {};

console.clear = function() { this.__clear(); };
console.log = function() { this.__output("log", arguments); };
console.debug = function() { this.__output("debug", arguments); };
console.error = function() { this.__output("error", arguments); };
console.info = function() { this.__output("info", arguments); };
console.warn = function() { this.__output("warn", arguments); };

	)") != 0)
	{
		ThrowError(eventloop->ctx(), -1);
	};
	duk_pop(ctx);

	ConsoleHandler::Ptr handler(new ConsoleHandler(eventloop));

	// register the functions
	detail::r_console_Setup(handler);

	// return the handler
	return handler;
}


} }