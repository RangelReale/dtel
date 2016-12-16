#pragma once

#include "../Handler.h"

#include <duktape.h>

#include <sstream>

namespace dtel {
namespace console {
namespace detail {

	static const char* PROP_ELHANDLER = "\xFF" "DTEL_CONSOLE_HANDLER";

	/**
	* Storage of the handler inside duktape
	*/
	struct ConsoleHandlerStorage
	{
		ConsoleHandler::Ptr handler;
	};

	inline ConsoleHandlerStorage *consolehandler_from_this(duk_context *ctx)
	{
		duk_push_this(ctx);
		duk_require_object_coercible(ctx, -1);

		duk_get_prop_string(ctx, -1, PROP_ELHANDLER);
		ConsoleHandlerStorage *ret = static_cast<ConsoleHandlerStorage*>(duk_get_pointer(ctx, -1));
		duk_pop_2(ctx); // "this", prop
		return ret;
	}

	duk_ret_t r_console_Clear(duk_context *ctx)
	{
		ConsoleHandlerStorage *storage = consolehandler_from_this(ctx);
		if (storage->handler->worker()) {
			storage->handler->worker()->clear();
		}
		return 0;
	}

	duk_ret_t r_console_Output(duk_context *ctx)
	{
		ConsoleHandlerStorage *storage = consolehandler_from_this(ctx);
		if (storage->handler->worker()) {
			// 0: outputtype
			// 1: message (array)
			std::string outputype(duk_require_string(ctx, 0));
			duk_require_object_coercible(ctx, 1);

			// build message
			std::stringstream ss;
			duk_enum(ctx, 1, DUK_ENUM_ARRAY_INDICES_ONLY | DUK_ENUM_SORT_ARRAY_INDICES);
			while (duk_next(ctx, -1, 1)) {
				if (!ss.str().empty())
					ss << " ";
				ss << duk_to_string(ctx, -1);
				duk_pop_2(ctx); // pop key and value
			}

			storage->handler->worker()->output(outputype, ss.str());
		}
		return 0;
	}

	duk_ret_t r_console_Finalize(duk_context *ctx)
	{
		// 0 = console object
		duk_get_prop_string(ctx, 0, PROP_ELHANDLER);
		if (duk_is_pointer(ctx, -1))
		{
			ConsoleHandlerStorage *storage = static_cast<ConsoleHandlerStorage*>(duk_get_pointer(ctx, -2));
			delete storage;
			duk_del_prop_string(ctx, 0, PROP_ELHANDLER);
		}
		duk_pop(ctx);

		return 0;
	}

	void r_console_Setup(ConsoleHandler::Ptr handler)
	{
		duk_context *ctx = handler->eventLoop()->ctx();

		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, "console");

		// register the handler inside the console object
		ConsoleHandlerStorage *storage = new ConsoleHandlerStorage{ handler };
		duk_push_pointer(ctx, storage);
		duk_put_prop_string(ctx, -2, PROP_ELHANDLER);

		// finalizer for the storage
		duk_push_c_function(ctx, &r_console_Finalize, 1);
		duk_set_finalizer(ctx, -2);

		// register __clear
		duk_push_c_function(ctx, &r_console_Clear, 0);
		duk_put_prop_string(ctx, -2, "__clear");

		// register __output
		duk_push_c_function(ctx, &r_console_Output, 2);
		duk_put_prop_string(ctx, -2, "__output");

		duk_pop_2(ctx);
	}

} } }
