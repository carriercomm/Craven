#pragma once

#include <utility>
#include <boost/signals2.hpp>

//! Namespace to provide hidden signal management singleton
namespace detail
{
	//! Signal management singleton
	class signal_singleton
	{
		//! Hidden constructor
		signal_singleton();

		//! Stores the instance of the singleton
		static signal_singleton* inst_;

		static void handler(int signum);
	public:
		//! Retrieve the singleton, constructing it if this is the first call.
		static signal_singleton* inst();

		//! Function template to forward a connection to the handlers_ signal.
		template <class Callable>
		boost::signals2::connection connect(Callable&& f)
		{
			return handlers_.connect(std::forward<Callable>(f));
		}

	protected:
		boost::signals2::signal<void (int)> handlers_;
	};
}

//! Handles registration of system signal handlers.
class Signals
{
public:
	//! Handles the registration of this signal handler
	Signals();

	//! Function template to forward registration to term
	template <class Callable>
	boost::signals2::connection register_term(Callable&& f)
	{
		return term_.connect(std::forward<Callable>(f));
	}

	//! Function template to forward registration to term
	template <class Callable>
	boost::signals2::connection register_int(Callable&& f)
	{
		return int_.connect(std::forward<Callable>(f));
	}

	//! Function template to forward registration to term
	template <class Callable>
	boost::signals2::connection register_hup(Callable&& f)
	{
		return hup_.connect(std::forward<Callable>(f));
	}

protected:
	void signal_handler(int signal);

	boost::signals2::scoped_connection handler_connection_;

	boost::signals2::signal<void (void)> term_;
	boost::signals2::signal<void (void)> int_;
	boost::signals2::signal<void (void)> hup_;

};
