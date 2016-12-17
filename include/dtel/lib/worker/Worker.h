#pragma once

#include <dtel.h>
#include <dtel/ResetStackOnScopeExit.h>
#include <dtel/lib/eventtarget/EventTarget.h>
#include <dtel/detail/util.h>

#include <duktape.h>

#include <memory>
#include <thread>
#include <functional>
#include <mutex>

namespace dtel {
namespace worker {

namespace detail {

	static const char* PROP_ELHANDLER = "\xFF" "DTEL_WORKER_HANDLER";
	static const char* PROP_DATA = "\xFF" "DTEL_WORKER_DATA";

	class ErrorEvent : public Event
	{
	public:
		ErrorEvent(Ref::Ptr ref, const std::string &message) :
			Event(), _ref(ref), _message(message)
		{}

		void apply(duk_context *ctx)
		{
			// call 'dispatchEvent' on the "error" event
			auto evt = make_intrusive<eventtarget::Event>("error", "ErrorEvent", _ref);
			evt->eventInit.properties["message"] = _message;
			eventtarget::EventTarget_dispatchEvent(ctx, _ref, evt);
		}

		void release(duk_context *ctx)
		{

		}

	private:
		Ref::Ptr _ref;
		std::string _message;
	};

	// This is run in the caller worker
	class PostMessageEvent : public Event
	{
	public:
		PostMessageEvent(Ref::Ptr ref, const std::string &message) :
			Event(), _ref(ref), _message(message)
		{}

		void apply(duk_context *ctx)
		{
			// parse message
			dtel::detail::util::json_custom_decode_push(ctx, _message);
			Value::Ptr mref(new Ref(ctx));

			// call 'dispatchEvent' on the "message" event
			auto evt = make_intrusive<eventtarget::Event>("message", "Event");
			evt->eventInit.properties["data"] = mref;
			eventtarget::EventTarget_dispatchEvent(ctx, _ref, evt);
		}

		void release(duk_context *ctx)
		{

		}

	private:
		Ref::Ptr _ref;
		std::string _message;
	};

	// This is run INSIDE the worker
	class WorkerPostMessageEvent : public Event
	{
	public:
		WorkerPostMessageEvent(const std::string &message) :
			Event(), _message(message)
		{}

		void apply(duk_context *ctx)
		{
			// parse message
			dtel::detail::util::json_custom_decode_push(ctx, _message);
			Value::Ptr mref(new Ref(ctx));

			// call 'dispatchEvent' on the "message" event
			Value::Ptr global(make_intrusive<ValueGlobal>());
			auto evt = make_intrusive<eventtarget::Event>("message", "Event");
			evt->eventInit.properties["data"] = mref;
			eventtarget::EventTarget_dispatchEvent(ctx, global, evt);
		}

		void release(duk_context *ctx)
		{

		}

	private:
		std::string _message;
	};

}

class WorkerWorker : public ThreadSafeRefCountedBase<WorkerWorker>
{
public:
	typedef IntrusiveRefCntPtr<WorkerWorker> Ptr;

	virtual ~WorkerWorker() {}

	virtual duk_context *createContext()
	{
		return duk_create_heap_default();
	}

	virtual void destroyContext(duk_context *ctx)
	{
		return duk_destroy_heap(ctx);
	}

	virtual void loadUrl(duk_context *ctx, EventLoop *eventloop, const std::string &url)
	{
		duk_push_error_object(ctx, DUK_ERR_UNIMPLEMENTED_ERROR, "Worker url loading not implemented");
		duk_throw(ctx);
	}
};


/**
 * WorkerHandler
 */
class WorkerHandler : public ThreadSafeRefCountedBase<WorkerHandler>
{
public:
	typedef IntrusiveRefCntPtr<WorkerHandler> Ptr;

	WorkerHandler(EventLoop *eventloop) :
		_eventloop(eventloop), _worker(new WorkerWorker)
	{

	}

	EventLoop *eventLoop() const 
	{
		return _eventloop;
	}

	WorkerWorker::Ptr worker()
	{
		return _worker;
	}

	void setWorker(WorkerWorker::Ptr worker)
	{
		_worker = worker;
	}
private:
	EventLoop *_eventloop;
	WorkerWorker::Ptr _worker;
};

namespace detail {

	/**
	* Storage of the handler inside duktape
	*/
	struct WorkerHandlerStorage
	{
		WorkerHandler::Ptr handler;
	};

	class WorkerEventLoop : public EventLoop
	{
	public:
		typedef std::function<bool(const std::exception &e)> func_t;

		WorkerEventLoop(duk_context *ctx, func_t func) :
			EventLoop(ctx), _func(func) {}

		bool processException(const std::exception &e) override
		{
			if (_func)
				return _func(e);
			return false;
		}
	private:
		func_t _func;
	};

	class WorkerCallerPostMessage
	{
	public:
		virtual ~WorkerCallerPostMessage() {}

		virtual void callerPostMessage(const std::string &message) = 0;
	};

	// This runs INSIDE the worker
	inline duk_ret_t r_dedicatedWorkerGlobal_postMessage(duk_context *ctx)
	{
		// 0: "this"
		// 1: message

		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, PROP_DATA);
		WorkerCallerPostMessage *wd = static_cast<WorkerCallerPostMessage*>(duk_get_pointer(ctx, -1));
		duk_pop_2(ctx);

		// convert 1 to json
		std::string message = dtel::detail::util::json_custom_encode(ctx, 1);
		wd->callerPostMessage(message);

		return 0;
	}

	class WorkerData : public WorkerCallerPostMessage
	{
	public:
		WorkerData(WorkerHandler::Ptr handler, Ref::Ptr workerref) :
			_handler(handler), _workerref(workerref), _lock(), _ctx(NULL), _eventloop(NULL)
		{
		}

		void onProcessException(const std::exception &e)
		{
			std::lock_guard<std::recursive_mutex> lock(_lock);
			_handler->eventLoop()->postEvent(make_intrusive<ErrorEvent>(_workerref, e.what()));
		}

		void callerPostMessage(const std::string &message) override
		{
			std::lock_guard<std::recursive_mutex> lock(_lock);
			_handler->eventLoop()->postEvent(make_intrusive<PostMessageEvent>(_workerref, message));
		}

		void init()
		{
			_ctx = _handler->worker()->createContext();
			ResetStackOnScopeExit r(_ctx);
			_eventloop = new WorkerEventLoop(_ctx, [this](const std::exception &e) -> bool {
				this->onProcessException(e);
				return true;
			});

			eventtarget::RegisterEventTarget(_eventloop);

			try
			{
				// WorkerGlobalScope
				if (duk_peval_string(_ctx, R"(

var WorkerGlobalScope = function() {
	EventTarget.call(this);

	this.self = this;
	this.testglobalscope = "YES I AM GLOBAL SCOPE";

	var defevt = function(obj, name) {
		Object.defineProperty(obj, "on"+name, {
			get: function() { return this["_on"+name]; },
			set: function(val) { 
				if (this["_on"+name] != undefined)
					this.removeEventListener(name, this["_on"+name]);
				this["_on"+name] = val; 
				this.addEventListener(name, val); 
			}
		});
	}

	defevt(this, "error");
	defevt(this, "offline");
	defevt(this, "online");
	defevt(this, "languagechange");
};

WorkerGlobalScope.prototype = Object.create(EventTarget.prototype);
WorkerGlobalScope.prototype.constructor = WorkerGlobalScope;

WorkerGlobalScope.prototype.close = function() {
};

WorkerGlobalScope.prototype.importScripts = function() {
};

	)") != 0)
				{
					ThrowError(_ctx, -1);
				};
				duk_pop(_ctx);

				// DedicatedWorkerGlobalScope
				if (duk_peval_string(_ctx, R"(

var DedicatedWorkerGlobalScope = function() {
	WorkerGlobalScope.call(this);

	var defevt = function(obj, name) {
		Object.defineProperty(obj, "on"+name, {
			get: function() { return this["_on"+name]; },
			set: function(val) { 
				if (this["_on"+name] != undefined)
					this.removeEventListener(name, this["_on"+name]);
				this["_on"+name] = val; 
				this.addEventListener(name, val); 
			}
		});
	}

	defevt(this, "message");
};

DedicatedWorkerGlobalScope.prototype = Object.create(WorkerGlobalScope.prototype);
DedicatedWorkerGlobalScope.prototype.constructor = DedicatedWorkerGlobalScope;

DedicatedWorkerGlobalScope.prototype.postMessage = function(message) {
	___WORKER_POSTMessage___(this, message);
};

	)") != 0)
				{
					ThrowError(_ctx, -1);
				};
				duk_pop(_ctx);

				//
				// REPLACE GLOBAL OBJECT WITH INSTANCE OF DedicatedWorkerGlobalScope
				//
				duk_push_global_object(_ctx);

				// set postmessage function
				duk_push_c_function(_ctx, &r_dedicatedWorkerGlobal_postMessage, 2);
				duk_put_prop_string(_ctx, -2, "___WORKER_POSTMessage___");

				// create DedicatedWorkerGlobalScope
				duk_get_prop_string(_ctx, -1, "DedicatedWorkerGlobalScope");
				if (duk_pnew(_ctx, 0) != 0) {
					ThrowError(_ctx, -1);
				}

				// set WorkerData
				duk_push_pointer(_ctx, static_cast<WorkerCallerPostMessage*>(this));
				duk_put_prop_string(_ctx, -2, PROP_DATA);

				//** COPY USEFUL DATA FROM PREVIOUS GLOBAL
				//** ALL CURRENT PROPERTIES WILL NOT EXIST ON THE NEW GLOBAL
				duk_get_prop_string(_ctx, -2, "Object");
				duk_put_prop_string(_ctx, -2, "Object");

				duk_get_prop_string(_ctx, -2, "print");
				duk_put_prop_string(_ctx, -2, "print");

				duk_get_prop_string(_ctx, -2, "Error");
				duk_put_prop_string(_ctx, -2, "Error");

				duk_get_prop_string(_ctx, -2, "Duktape");
				duk_put_prop_string(_ctx, -2, "Duktape");

				// pop global
				duk_remove(_ctx, -2);
				// set global object
				duk_set_global_object(_ctx);

			}
			catch (std::exception &e)
			{
				onProcessException(e);
			}
		}

		void loadUrl(const std::string &url)
		{
			_handler->worker()->loadUrl(_eventloop->ctx(), _eventloop, url);
		}

		/**
		 * Posts the message inside the worker
		 */
		void postMessage(const std::string &message)
		{
			std::lock_guard<std::recursive_mutex> lock(_lock);
			if (_eventloop)
			{
				_eventloop->postEvent(make_intrusive<WorkerPostMessageEvent>(message));
			}
		}

		// Run eventloop in thread
		void run()
		{
			if (!_thread)
				_thread.reset(new std::thread(&WorkerData::internalRun, this));
		}

		void internalRun()
		{
			_eventloop->run();
		}

		~WorkerData()
		{
			std::lock_guard<std::recursive_mutex> lock(_lock);
			if (_eventloop)
			{
				_eventloop->terminate();
				_thread->join();
				delete _eventloop;
				_eventloop = NULL;
			}
			if (_ctx)
				_handler->worker()->destroyContext(_ctx);
		}

		EventLoop *eventLoop()
		{
			return _eventloop;
		}
	private:
		WorkerHandler::Ptr _handler;
		Ref::Ptr _workerref;
		std::recursive_mutex _lock;
		EventLoop *_parenteventloop;
		duk_context *_ctx;
		EventLoop *_eventloop;
		std::shared_ptr<std::thread> _thread;
	};

	inline WorkerHandlerStorage *workerhandler_from_worker(duk_context *ctx)
	{
		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, "Worker");
		duk_get_prop_string(ctx, -1, PROP_ELHANDLER);
		WorkerHandlerStorage *ret = static_cast<WorkerHandlerStorage*>(duk_get_pointer(ctx, -1));
		duk_pop_3(ctx); // global, Worker, prop
		return ret;
	}

	inline WorkerData *workerdata_from_this(duk_context *ctx)
	{
		duk_push_this(ctx);
		duk_require_object_coercible(ctx, -1);

		duk_get_prop_string(ctx, -1, PROP_DATA);
		WorkerData *ret = static_cast<WorkerData*>(duk_get_pointer(ctx, -1));
		duk_pop_2(ctx); // "this", prop
		return ret;
	}

	duk_ret_t r_Worker_finalizer(duk_context *ctx)
	{
		// 0 = object
		duk_get_prop_string(ctx, 0, PROP_DATA);
		if (duk_is_pointer(ctx, -1) != 0) {
			WorkerData *w = static_cast<WorkerData*>(duk_get_pointer(ctx, -1));
			delete w;
			duk_del_prop_string(ctx, 0, PROP_DATA);
		}
		duk_pop(ctx);

		return 0;
	}

	duk_ret_t r_Worker_construct(duk_context *ctx)
	{
		duk_require_string(ctx, 0);

		//
		// CALL BASE CLASS
		//
		duk_push_global_object(ctx);
		// get base class to call
		duk_get_prop_string(ctx, -1, "AbstractWorker");
		// remove global
		duk_remove(ctx, -2); 
		// method name is "call"
		duk_push_string(ctx, "call");
		// parameter is "this"
		duk_push_this(ctx);
		if (duk_pcall_prop(ctx, -3, 1) != 0) {
			duk_remove(ctx, -2);
			ThrowError(ctx, -1);
		}
		// base class, pcall result
		duk_pop_2(ctx); 


		//
		// Create
		//
		WorkerHandlerStorage *storage = workerhandler_from_worker(ctx);

		duk_push_this(ctx);

		// data
		duk_dup(ctx, -1);
		WorkerData *data = new WorkerData(storage->handler, new Ref(ctx));
		data->init();
		duk_push_pointer(ctx, data);
		duk_put_prop_string(ctx, -2, PROP_DATA);

		// finalizer
		duk_push_c_function(ctx, &r_Worker_finalizer, 1);
		duk_set_finalizer(ctx, -2);

		// start thread
		std::string url(duk_get_string(ctx, 0));
		data->loadUrl(url);
		data->run();

		return 0;
	}

	duk_ret_t r_Worker_postMessage(duk_context *ctx)
	{
		WorkerData *data = workerdata_from_this(ctx);

		// convert 0 to json
		std::string message = dtel::detail::util::json_custom_encode(ctx, 0);
		data->postMessage(message);

		return 0;
	}

}

/**
* Register the worker handling on the event loop
*/
inline WorkerHandler::Ptr RegisterWorker(EventLoop *eventloop)
{
	duk_context *ctx = eventloop->ctx();
	ResetStackOnScopeExit r(ctx);

	// handler
	WorkerHandler::Ptr handler(new WorkerHandler(eventloop));

	// must have EventTarget registered
	if (!eventtarget::IsEventTarget(ctx))
		throw Exception("EventTarget is not registered");

	if (duk_peval_string(ctx, R"(

var AbstractWorker = function() {
	EventTarget.call(this);

	var defevt = function(obj, name) {
		Object.defineProperty(obj, "on"+name, {
			get: function() { return this["_on"+name]; },
			set: function(val) { 
				if (this["_on"+name] != undefined)
					this.removeEventListener(name, this["_on"+name]);
				this["_on"+name] = val; 
				this.addEventListener(name, val); 
			}
		});
	}

	defevt(this, "error");
};

AbstractWorker.prototype = Object.create(EventTarget.prototype);
AbstractWorker.prototype.constructor = AbstractWorker;

	)") != 0)
	{
		ThrowError(ctx, -1);
	};
	duk_pop(ctx);

	// Worker
	duk_push_global_object(ctx);
	duk_push_c_function(ctx, &detail::r_Worker_construct, 1);
	duk_dup(ctx, -1);
	duk_put_prop_string(ctx, -3, "Worker");
	duk_remove(ctx, -2); // global

	// Worker constructor is at -1

	// put storage into Worker constructor
	detail::WorkerHandlerStorage *storage = new detail::WorkerHandlerStorage{ handler };
	duk_push_pointer(ctx, storage);
	duk_put_prop_string(ctx, -2, detail::PROP_ELHANDLER);

	if (duk_peval_string(ctx, R"(

Worker.prototype = Object.create(AbstractWorker.prototype);
Worker.prototype.constructor = Worker;

	)") != 0)
	{
		ThrowError(ctx, -1);
	};
	duk_pop(ctx); // peval result

	// get prototype
	duk_get_prop_string(ctx, -1, "prototype");

	// postMessage
	duk_push_c_function(ctx, &detail::r_Worker_postMessage, 2);
	duk_put_prop_string(ctx, -2, "postMessage");

	// Worker constructor, prototype
	duk_pop_2(ctx);

	// return the handler
	return handler;
}

} }
