#include <dtel.h>
#include <dtel/lib/eventtarget/EventTarget.h>
#include <dtel/lib/console/Console.h>
#include <dtel/lib/settimeout/SetTimeout.h>

#include <iostream>

using namespace dtel;

class Console : public console::ConsoleWorker
{
public:
	void clear() override
	{
		std::cout << "[[[[[ CONSOLE CLEAR ]]]]]" << std::endl;
	}

	void output(const std::string &outputtype, const std::string &message) override
	{
		std::cout << "[[[[[ CONSOLE OUTPUT: " << outputtype << " ** " << message << " ]]]]]" << std::endl;
	}
};


int main(int argc, char *argv[])
{
	duk_context *ctx = duk_create_heap_default();

	{
		EventLoop el(ctx);
		eventtarget::RegisterEventTarget(&el);
		auto CNHandler = console::RegisterConsole(&el);
		CNHandler->setWorker(make_intrusive<Console>());
		settimeout::RegisterSetTimeout(&el);

		if (duk_peval_string(el.ctx(), R"(	
console.log("Message from console");

	)") != 0)
		{
			ThrowError(ctx, -1);
		}

		el.run();
	}

	duk_destroy_heap(ctx);

	return 0;
}