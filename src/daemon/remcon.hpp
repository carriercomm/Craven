#pragma once

#include <unordered_map>

namespace detail
{
	class connection : public std::enable_shared_from_this<connection>
	{
		connection(boost::asio::local::stream_protocol::socket& socket);
	public:
		typedef std::shared_ptr<connection> pointer;

		static pointer create(boost::asio::io_service& io_service);

		template <class T>
		static pointer create(T&& socket)
		{
			return pointer(new connection(std::forward(socket)));
		}

		boost::asio::local::stream_protocol::socket socket();

		void queue_write(const std::string& msg);

		template <class Callable>
		boost::signals2::connection connect_read(Callable&& f)
		{
			return read_handlers_.connect(std::forward(f));
		}


	protected:
		boost::asio::local::stream_protocol::socket socket_;

		std::deque<std::string> write_queue_;

		boost::signals2::signal<void (const std::string&)> read_handlers_;

		void handle_read(boost::system::error_code& error, std::size_t bytes_tx);
		void handle_write(boost::system::error_code& error);
	};

}

//! Responsible for handling the connection to a CLI instance; provides callbacks for this.
class RemoteControl
{
public:
	//! Provides thread-safe write access to a ctl.
	/*!
	 *  This class provides thread-safe write access to a controller. It should
	 *  close the connection when the final instance targeted at the session is
	 *  destructed -- do this by wrapping a shared connection object managed by a
	 *  shared_ptr; this shared connection object should handle the connection via
	 *  RAII.
	 */
	class CtlSession
	{};

	typedef boost::signals2::signal<void(const std::vector<std::string>&, const CtlSession&)> signal_type;

	RemoteControl(boost::asio::io_service& io, const boost::filesystem::path& socket);


	//! Register a handler for a verb.
	/*!
	 *  This function registers a callback for a verb sent for the CTL.
	 *  \param verb The verb for which this callback should be called
	 *  \param f The callback. Must be a callable of type void f(const
	 *  std::vector<std::string>&, const CtlSession&);
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
		return registry_[verb].connect(std::forward(f));
	}

protected:
	boost::asio::io_service& io_;

	std::unordered_map<std::string, signal_type> registry_;
};
