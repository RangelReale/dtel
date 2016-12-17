# DTEL - Duktape event loop

DTEL is a C++11 header-only library that implements a javascript event loop for the [duktape](http://duktape.org) library.

The library provides events, tasks in a thread pool, loop runners, and comes with libraries providing the following functions:

* Console with console.log
* EventTarget and DOM-like Event handling
* setTimeout and related functions
* Worker to run background jobs in threads

#### Example

```c++
    duk_context *ctx = duk_create_heap_default();
    
	EventLoop el(ctx);

	eventtarget::RegisterEventTarget(&el);
	auto CNHandler = console::RegisterConsole(&el);
	CNHandler->setWorker(make_intrusive<Console>());
	settimeout::RegisterSetTimeout(&el);
	auto WKHandler = worker::RegisterWorker(&el);
	WKHandler->setWorker(make_intrusive<Worker>());
		
	if (duk_peval_string(el.ctx(), R"(	
console.log("Message from console");

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

var w = new Worker("onmessage = function(e) { console.log('$$ WORKER RECEIVED MESSAGE $$: '+e.data); postMessage('## WORKER RESPONSE ##'); }; console.log('worker started!');");
w.addEventListener("message", function(e) { console.log("Message from WORKER! " + e.data); } );
w.addEventListener("error", function(e) { console.error("WORKER Error! " + e.message); } );

w.postMessage("Message from main loop");

	)") != 0)
	{
		ThrowError(el.ctx(), -1);
	}
	
	std::thread t([&el] {
		std::this_thread::sleep_for(std::chrono::milliseconds(7000));
		std::cout << "** TERMINATING **" << std::endl;
		el.terminate();
	});

	el.run();

	t.join();

```

Output:

```
-- CONSOLE: log ** Message from console
-- CONSOLE: log ** worker started!
-- CONSOLE: log ** $$ WORKER RECEIVED MESSAGE $$: Message from main loop
-- CONSOLE: log ** Message from WORKER! ## WORKER RESPONSE ##
-- CONSOLE: log ** Message every 500ms
-- CONSOLE: log ** Single message after 1000ms
-- CONSOLE: log ** Message every 500ms
-- CONSOLE: log ** Message every 500ms
-- CONSOLE: log ** Message every 500ms
-- CONSOLE: log ** Cancelling message every 500ms
** TERMINATING **
PRESS ANY KEY TO CONTINUE
```

### Author

Rangel Reale (http://github.com/RangelReale)
