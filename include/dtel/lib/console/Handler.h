#pragma once

#include <dtel.h>

namespace dtel {
namespace console {

class ConsoleWorker : public ThreadSafeRefCountedBase<ConsoleWorker>
{
public:
	typedef IntrusiveRefCntPtr<ConsoleWorker> Ptr;

	virtual ~ConsoleWorker() {}

	virtual void clear()
	{

	}

	virtual void output(const std::string &outputtype, const std::string &message)
	{

	}
};

/**
 * ConsoleHandler
 */
class ConsoleHandler : public ThreadSafeRefCountedBase<ConsoleHandler>
{
public:
	typedef IntrusiveRefCntPtr<ConsoleHandler> Ptr;

	ConsoleHandler(EventLoop *eventloop) :
		_eventloop(eventloop)
	{

	}

	EventLoop *eventLoop() const 
	{
		return _eventloop;
	}

	ConsoleWorker::Ptr worker()
	{
		return _worker;
	}

	void setWorker(ConsoleWorker::Ptr worker)
	{
		_worker = worker;
	}
private:
	EventLoop *_eventloop;
	ConsoleWorker::Ptr _worker;
};

} }