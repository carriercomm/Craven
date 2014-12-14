#pragma once

#include <unordered_map>

#include "../common/connection.hpp"

//! Provides write-only shared access to a connection from the CTL
class CTLSession
{
public:
	typedef boost::asio::local::stream_protocol::socket socket_type;
	typedef util::connection<socket_type, util::connection_multiple_handler_tag>::type connection_type;

	//! Construct the session from a connection abstraction pointer
	/*!
	 *  \param connection The connection object managing the socket to the CTL
	 */
	CTLSession(connection_type::pointer connection);

	//! Write on socket to the CTL
	/*!
	 *  \param msg The message to be written
	 */
	void write(const std::string& msg);

protected:
	connection_type::pointer connection_;
};

//! Responsible for handling the connection to a CLI instance; provides callbacks for this.
class RemoteControl
{
public:

	struct parse_error : std::runtime_error
	{
		parse_error(const std::string& what);
	};


	//! Provides thread-safe write access to a ctl.
	/*!
	 *  This class provides thread-safe write access to a controller. It should
	 *  close the connection when the final instance targeted at the session is
	 *  destructed -- do this by wrapping a shared connection object managed by a
	 *  shared_ptr; this shared connection object should handle the connection via
	 *  RAII.
	 */
	class session
	{};

	typedef boost::signals2::signal<void(const std::vector<std::string>&, CTLSession)> signal_type;

	RemoteControl(boost::asio::io_service& io, const boost::filesystem::path& socket);


	//! Register a handler for a verb.
	/*!
	 *  This function registers a callback for a verb sent for the CTL.
	 *  \param verb The verb for which this callback should be called
	 *  \param f The callback. Must be a callable of type void f(const
	 *  std::vector<std::string>&, CTLSession);
	 *
	 *  \returns A Boost.Signals2 connection object; use this to unregister.
	 */
	template <class Callable>
	boost::signals2::connection connect(const std::string& verb, Callable&& f)
	{
		//construct the signal if it doesn't exist
		if(registry_.count(verb) == 0)
			registry_[verb] = signal_type();

		//Forward the connect
		return registry_[verb].connect(std::forward<Callable>(f));
	}

protected:
	boost::asio::io_service& io_;
	boost::asio::local::stream_protocol::acceptor acceptor_;

	std::unordered_map<std::string, signal_type> registry_;

	void start_accept();

	std::tuple<std::string, std::vector<std::string>> parse_line(const std::string& msg);
};
