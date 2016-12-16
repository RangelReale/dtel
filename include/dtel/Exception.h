#pragma once

#include <stdexcept>

#include <duktape.h>

namespace dtel {

class BaseException : public std::exception {};

class Exception : public BaseException {
	std::string _message, _stack;
public:
	explicit Exception(const std::string &message, const std::string &stack = "")
		: _message(message), _stack(stack) {}
	char const * what() const noexcept override {
		return _message.c_str();
	}
	char const * stack() const noexcept {
		return _stack.c_str();
	}
};

// gets message and possible traceback from Error object
inline std::string ErrorMessageWithStack(duk_context *ctx, duk_idx_t index) {
	std::string ret;
	duk_idx_t eindex = duk_normalize_index(ctx, index);

	// TODO: check if index is really an error object
	// gets the "stack" property of the Error object
	if (duk_is_object(ctx, eindex)) {
		duk_get_prop_string(ctx, eindex, "stack");
		if (duk_is_string(ctx, -1)) {
			size_t size;
			const char *buff = duk_get_lstring(ctx, -1, &size);
			ret = std::string{ buff, size };
			duk_pop(ctx);
			return ret;
		}
		duk_pop(ctx);
	}

	// gets message from object using ToString()
	ret = duk_safe_to_string(ctx, eindex);
	return ret;
}

// gets message from Error object
inline std::string ErrorMessage(duk_context *ctx, duk_idx_t index) {
	std::string ret;
	// gets message from object using ToString()
	duk_dup(ctx, index);
	ret = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	return ret;
}

// gets stack from Error object, or blank string if no stack is present
inline std::string ErrorStack(duk_context *ctx, duk_idx_t index) {
	std::string ret;
	duk_idx_t eindex = duk_normalize_index(ctx, index);

	// TODO: check if index is really an error object
	// gets the "stack" property of the Error object
	if (duk_is_object(ctx, eindex)) {
		duk_get_prop_string(ctx, eindex, "stack");
		if (duk_is_string(ctx, -1)) {
			size_t size;
			const char *buff = duk_get_lstring(ctx, -1, &size);
			ret = std::string{ buff, size };
			duk_pop(ctx);
			return ret;
		}
		duk_pop(ctx);
	}
	return ret;
}

inline void ThrowError(duk_context *ctx, duk_idx_t index)
{
	std::string emsg(ErrorMessage(ctx, index));
	std::string estack(ErrorStack(ctx, index));

	duk_remove(ctx, index);

	throw Exception(emsg, estack);
}

}