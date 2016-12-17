#include <dtel.h>
#include <dtel/lib/eventtarget/EventTarget.h>
#include <dtel/lib/console/Console.h>
#include <dtel/lib/settimeout/SetTimeout.h>
#include <dtel/lib/worker/Worker.h>

#include <iostream>

using namespace dtel;

class MyEL : public EventLoop
{
public:
	MyEL(duk_context *ctx) : EventLoop(ctx) {}

	bool processException(const std::exception &e) override
	{
		std::cout << "@@@@@@@ EXCEPTION: " << e.what() << " @@@@@@@" << std::endl;
		return true;
	}
};


class Console : public console::ConsoleWorker
{
public:
	void clear() override
	{
		std::lock_guard<std::mutex> lock(_lock);
		std::cout << "-- CONSOLE CLEAR" << std::endl;
	}

	void output(const std::string &outputtype, const std::string &message) override
	{
		std::lock_guard<std::mutex> lock(_lock);
		std::cout << "-- CONSOLE: " << outputtype << " ** " << message << std::endl;
	}
private:
	std::mutex _lock;
};

class Worker : public worker::WorkerWorker
{
public:
	void loadUrl(duk_context *ctx, EventLoop *eventloop, const std::string &url) override
	{
		// for testing, treat url as javascript eval

		// register all libs on the worker context
		eventtarget::RegisterEventTarget(eventloop);

		auto CNHandler = console::RegisterConsole(eventloop);
		CNHandler->setWorker(make_intrusive<Console>());

		auto STHandler = settimeout::RegisterSetTimeout(eventloop);

		auto WKHandler = worker::RegisterWorker(eventloop);
		WKHandler->setWorker(make_intrusive<Worker>());

		if (duk_peval_string(ctx, url.c_str()) != 0)
		{
			ThrowError(ctx, -1);
		}
		duk_pop(ctx);
	}
};

void test_console(EventLoop &el)
{
	if (duk_peval_string(el.ctx(), R"(	

console.log("Message from console");

	)") != 0)
	{
		ThrowError(el.ctx(), -1);
	}
}

void test_setTimeout(EventLoop &el)
{
	if (duk_peval_string(el.ctx(), R"(	

setTimeout(function() {
	console.log("Single message after 1000ms");
}, 1000);

var id = setInterval(function() {
	console.log("Message every 500ms");
}, 500);

setTimeout(function() {
	console.log("Cancelling message every 500ms");
	clearInterval(id);
}, 5000);

	)") != 0)
	{
		ThrowError(el.ctx(), -1);
	}

}

void test_worker(EventLoop &el)
{
	if (duk_peval_string(el.ctx(), R"(	

var w = new Worker("onmessage = function(e) { console.log('$$ WORKER RECEIVED MESSAGE $$: '+e.data); postMessage('## WORKER RESPONSE ##'); }; console.log('worker started!');");
w.addEventListener("message", function(e) { console.log("Message from WORKER! " + e.data); } );
w.addEventListener("error", function(e) { console.error("WORKER Error! " + e.message); } );

w.postMessage("Message from main loop");

	)") != 0)
	{
		ThrowError(el.ctx(), -1);
	}

}

int main(int argc, char *argv[])
{
	duk_context *ctx = duk_create_heap_default();

	{
		MyEL el(ctx);

		eventtarget::RegisterEventTarget(&el);

		auto CNHandler = console::RegisterConsole(&el);
		CNHandler->setWorker(make_intrusive<Console>());

		settimeout::RegisterSetTimeout(&el);

		auto WKHandler = worker::RegisterWorker(&el);
		WKHandler->setWorker(make_intrusive<Worker>());

		test_console(el);
		test_setTimeout(el);
		test_worker(el);

		el.run();
	}

	duk_destroy_heap(ctx);

	return 0;
}