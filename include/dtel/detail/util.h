#pragma once

#include "../ResetStackOnScopeExit.h"
#include "../Exception.h"

#include <duktape.h>

#include <string>

namespace dtel {
namespace detail {

namespace util {

	inline static std::string json_custom_encode(duk_context *ctx, duk_idx_t index, const std::string &format = "jx")
	{
		ResetStackOnScopeExit r(ctx);

		duk_idx_t in = duk_normalize_index(ctx, index);

		// call Duktape.enc
		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, "Duktape");
		duk_push_string(ctx, "enc");
		duk_push_string(ctx, format.c_str());
		duk_dup(ctx, in);

		duk_int_t status = duk_pcall_prop(ctx, -4, 2);
		if (status != 0) {
			ThrowError(ctx, -1);
		}
		
		duk_size_t len;
		const char *d = duk_to_lstring(ctx, -1, &len);
		std::string ret(d, len);
		duk_pop_3(ctx);

		return ret;
	}

	inline static void json_custom_decode_push(duk_context *ctx, const std::string &data, const std::string &format = "jx")
	{
		// call Duktape.dec
		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, "Duktape");
		duk_remove(ctx, -2); // global
		duk_push_string(ctx, "dec");
		duk_push_string(ctx, format.c_str());
		duk_push_lstring(ctx, data.c_str(), data.length());
		duk_int_t status = duk_pcall_prop(ctx, -4, 2);
		duk_remove(ctx, -2); // Duktape
		if (status != 0) {
			ThrowError(ctx, -1);
		}
	}

}

} }