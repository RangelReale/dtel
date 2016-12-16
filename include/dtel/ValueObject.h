#pragma once

#include "Value.h"
#include "Exception.h"
#include "detail/any.hpp"

#include <duktape.h>

#include <map>

namespace dtel {

inline void PushAnyValue(duk_context *ctx, const linb::any &value);

/**
 * Value that represents an object with properties
 */
class ValueObject : public Value
{
public:
	typedef std::map<std::string, linb::any> properties_t;

	properties_t properties;

	ValueObject(const properties_t &properties = properties_t()) :
		Value(), properties(properties) {}

	virtual int push(duk_context *ctx)
	{
		if (properties.size() == 0)
			return 0;
		duk_push_object(ctx);
		for (auto &kv : properties)
		{
			PushAnyValue(ctx, kv.second);
			duk_put_prop_string(ctx, -2, kv.first.c_str());
		}
		return 1;
	}
};

inline void PushAnyValue(duk_context *ctx, const linb::any &value)
{
	if (value.empty())
		duk_push_undefined(ctx);
	else if (value.type() == typeid(void))
		duk_push_null(ctx);
	else if (value.type() == typeid(int))
		duk_push_int(ctx, linb::any_cast<int>(value));
	else if (value.type() == typeid(float))
		duk_push_number(ctx, linb::any_cast<float>(value));
	else if (value.type() == typeid(double))
		duk_push_number(ctx, linb::any_cast<double>(value));
	else if (value.type() == typeid(std::string))
		duk_push_string(ctx, linb::any_cast<std::string>(value).c_str());
	else
	{
		try
		{
			Value::Ptr vo = linb::any_cast<Value::Ptr>(value);
			vo->push(ctx);
			return;
		}
		catch (linb::bad_any_cast &)
		{
			// is not a ValueObject
		}
		throw Exception("Unknown 'any' value type");
	}
}


}