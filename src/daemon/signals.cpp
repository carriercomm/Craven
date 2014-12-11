#include <signal.h>

#include <functional>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include "signals.hpp"

detail::signal_singleton* detail::signal_singleton::inst_ = nullptr;

detail::signal_singleton* detail::signal_singleton::inst()
{
	if(!inst_)
		inst_ = new signal_singleton();
	return inst_;
}

void detail::signal_singleton::handler(int signum)
{
	inst()->handlers_(signum);
}

detail::signal_singleton::signal_singleton()
{
	struct sigaction new_handler;

	//Register our handler.
	//The handler needs to be a function ptr, so we're using a lambda to wrap
	//our (member function) hander -- std::function and std::bind are both
	//functor objects.
	new_handler.sa_handler = detail::signal_singleton::handler;

	sigemptyset(&new_handler.sa_mask);
	new_handler.sa_flags = 0;

	sigaction(SIGTERM, &new_handler, nullptr);
	sigaction(SIGINT, &new_handler, nullptr);
	sigaction(SIGHUP, &new_handler, nullptr);
}

Signals::Signals()
	:handler_connection_(detail::signal_singleton::inst()->connect(std::function<void(int)>(std::bind(&Signals::signal_handler,
					this, std::placeholders::_1))))
{
}

void Signals::signal_handler(int signal)
{
	switch(signal)
	{
	case SIGTERM:
		term_();
		break;
	case SIGINT:
		int_();
		break;
	case SIGHUP:
		hup_();
		break;
	default:
		BOOST_LOG_TRIVIAL(warning) << "Unhandled signal received: " << signal;
	};
}
