#pragma once

#include <unordered_map>

namespace detail
{
	//! Connection handler
	/*!
	 *  This class handles a connection to a client from the server. It can
	 *  only be created as a std::shared_ptr -- it's a shared object. When it
	 *  destructs, it closes the socket it owns, cleaning up the connection
	 *  automatically.
	 */
	class connection : public std::enable_shared_from_this<connection>
	{
		//! Construct the connection.
		/*!
		 *  This function is only accessable within the class, so we cannot
		 *  make instances of this class outside its members. See create() for
		 *  how instances can be publicly created.
		 *  \param socket A reference to the socket for which this class is
		 *  built around. This socket is moved into the class.
		 */
		connection(boost::asio::local::stream_protocol::socket& socket);
	public:
		//! The pointer type for this class.
		typedef std::shared_ptr<connection> pointer;

		//! Create a new instance of the class from an io_service.
		/*!
		 *  This is one of two overloads of this function, the only function
		 *  able to create new instances of this class.
		 *
		 *  It creates a new class (with a new socket) from the given
		 *  io_service, wrapping it in a shared_ptr and returning it.
		 *  \param io_service The io_service with which the connection's socket
		 *  should be registered.
		 *  \returns A new instance of the class, wrapped in a std::shared_ptr
		 */
		static pointer create(boost::asio::io_service& io_service);

		//! Create a new instance of the class from a socket
		/*!
		 *  This is one of the two overloads of this function, the only
		 *  function able to create new instances of this class.
		 *
		 *  It creates a new class to manage the given socket.
		 *
		 *  \param socket The socket to manage.
		 *  \returns A new instance of the class, wrapped in a std::shared_ptr
		template <class T>
		static pointer create(T&& socket)
		{
			return std::make_shared<connection>(std::forward(socket));
		}

		//! Return the socket managed by this class.
		//! \return The socket managed by this class.
		boost::asio::local::stream_protocol::socket& socket();

		//! Add a message to the write queue.
		/*!
		 *  This function adds a message to the queue to be asynchronously sent
		 *  when the socket is available for writing. Thread safe.
		 *
		 *  \param msg The message to add to the message queue.
		 */
		void queue_write(const std::string& msg);

		//! Register a callback for a read event.
		/*!
		 *  This function registers a callback to receive the strings read
		 *  asynchronously from the managed socket.
		 *  \param f The callable object to call when a string is received --
		 *  must be able to take a string.
		 *
		 *  \returns An object that can be used for deregistering the callback.
		 */
		template <class Callable>
		boost::signals2::connection connect_read(Callable&& f)
		{
			return read_handlers_.connect(std::forward(f));
		}


	protected:
		//! The socket managed by this class.
		boost::asio::local::stream_protocol::socket socket_;

		//! The message queue to be placed on the socket.
		std::deque<std::string> write_queue_;

		//! Handles the read callbacks.
		boost::signals2::signal<void (const std::string&)> read_handlers_;

		//! Callback for handling reads on the socket.
		void handle_read(boost::system::error_code& error, std::size_t bytes_tx);

		//! Callback for handling writes on the socket.
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
	class session
	{};

	typedef boost::signals2::signal<void(const std::vector<std::string>&, const session&)> signal_type;

	RemoteControl(boost::asio::io_service& io, const boost::filesystem::path& socket);


	//! Register a handler for a verb.
	/*!
	 *  This function registers a callback for a verb sent for the CTL.
	 *  \param verb The verb for which this callback should be called
	 *  \param f The callback. Must be a callable of type void f(const
	 *  std::vector<std::string>&, const session&);
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
