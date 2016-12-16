#pragma once

#include <duktape.h>

namespace dtel {

class ResetStackOnScopeExit {
    duk_context * _stack;
    int _saved_top_index;

public:
    explicit ResetStackOnScopeExit(duk_context * stack)
        : _stack(stack),
          _saved_top_index(duk_get_top(_stack))
    {}

    ~ResetStackOnScopeExit() {
        if (_stack) {
            duk_set_top(_stack, _saved_top_index);
        }
    }

    ResetStackOnScopeExit(ResetStackOnScopeExit const & ) = delete;
    ResetStackOnScopeExit(ResetStackOnScopeExit       &&) = delete;
    ResetStackOnScopeExit & operator=(ResetStackOnScopeExit const & ) = delete;
    ResetStackOnScopeExit & operator=(ResetStackOnScopeExit       &&) = delete;
};

}