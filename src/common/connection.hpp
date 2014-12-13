#pragma once

#include <mutex>
#include <deque>
#include <functional>
#include <memory>

#include <boost/signals2.hpp>
#include <boost/asio.hpp>

#include "linebuffer.hpp"

namespace util
{
	//! Specifies that util::connection should use a single read handler
	struct connection_single_handler_tag {};

	//! Specifies that util::connection should use a read signaller.
	struct connection_multiple_handler_tag {};

	template <class T>
	struct connection_traits;

	template <>
	struct connection_traits<connection_single_handler_tag>
	{
		typedef connection_single_handler_tag tag;
		typedef connection_traits<tag> type;

		typedef std::function<void (const std::string&)> handler_type;

		typedef void connection_return;

		template <class Callable>
		static connection_return connect(handler_type& handler, Callable&& f)
		{
			handler = handler_type(std::forward<Callable>(f));
		}
	};

	template <>
	struct connection_traits<connection_multiple_handler_tag>
	{
		typedef connection_multiple_handler_tag tag;
		typedef connection_traits<tag> type;

		typedef boost::signals2::signal<void (const std::string&)> handler_type;

		typedef boost::signals2::connection connection_return;

		template <class Callable>
		static connection_return connect(handler_type& handler, Callable&& f)
		{
			return handler.connect(f);
		}
	};

	//! Connection handler
	/*!
	 *  This class handles a connection to a client from the server. It can
	 *  only be created as a std::shared_ptr -- it's a shared object. When it
	 *  destructs, it closes the socket it owns, cleaning up the connection
	 *  automatically.
	 */
	template <class Socket, class HandlerTag, class HandlerTraits = connection_traits<HandlerTag>>
	class connection : public std::enable_shared_from_this<connection<Socket, HandlerTag>>
	{
	public:
		//! The socket type used by this clas
		typedef Socket socket_type;

		//! Handler tag
		typedef HandlerTag handler_tag;

		//! Handler traits
		typedef HandlerTraits handler_traits;

		//! Convenience
		typedef connection<socket_type, handler_tag> type;


		//! The pointer type for this class.
		typedef std::shared_ptr<type> pointer;

	private:
		//! Construct the connection.
		/*!
		 *  This function is only accessable within the class, so we cannot
		 *  make instances of this class outside its members. See create() for
		 *  how instances can be publicly created.
		 *  \param socket A reference to the socket for which this class is
		 *  built around. This socket is moved into the class.
		 */
		connection(socket_type socket)
			:socket_(std::move(socket)),
			writing_(false)
		{

		}
	public:

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
		static pointer create(boost::asio::io_service& io_service)
		{
			return create(socket_type(io_service));
		}

		//! Create a new instance of the class from a socket
		/*!
		 *  This is one of the two overloads of this function, the only
		 *  function able to create new instances of this class.
		 *
		 *  It creates a new class to manage the given socket.
		 *
		 *  \param socket The socket to manage.
		 *  \returns A new instance of the class, wrapped in a std::shared_ptr
		 */
		template <class T>
		static pointer create(T&& socket)
		{
			auto ptr = pointer(new type(std::move(socket)));
			ptr->setup_read();
			return ptr;
		}

		//! Return the socket managed by this class.
		//! \return The socket managed by this class.
		socket_type& socket()
		{
			return socket_;
		}

		//! \overload
		const socket_type& socket() const
		{
			return socket_;
		}

		//! Add a message to the write queue.
		/*!
		 *  This function adds a message to the queue to be asynchronously sent
		 *  when the socket is available for writing. Thread safe.
		 *
		 *  \param msg The message to add to the message queue.
		 */
		void queue_write(const std::string& msg)
		{
			{
				std::lock_guard<std::mutex> guard(wq_mutex_);
				write_queue_.push_back(msg);
			}//cleanup lock

			setup_write();
		}

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
		typename handler_traits::connection_return connect_read(Callable&& f)
		{
			return handler_traits::connect(read_handler_, std::forward<Callable>(f));
		}


	protected:
		//! The socket managed by this class.
		socket_type socket_;

		//! The message queue to be placed on the socket.
		std::deque<std::string> write_queue_;

		//! Mutex protecting write_queue_ and writing_
		std::mutex wq_mutex_;

		//! For handling the write queue
		bool writing_;

		//! Handles the read callbacks.
		typename handler_traits::handler_type read_handler_;

		typedef util::line_buffer<std::array<char, 512>> line_buffer_type;
		//! Forms complete lines from buffers.
		line_buffer_type lb;


		//! Setup a read
		void setup_read()
		{
			auto buf = std::make_shared<line_buffer_type::buffer_type>();

			//Compiler won't look for shared_from_this in this class unless we tell it to
			auto shared(this->shared_from_this());

			socket_.async_receive(boost::asio::buffer(*buf),
					[this, shared, buf](const boost::system::error_code& ec, std::size_t bytes_tx)
					{
						if(ec)
							throw boost::system::system_error(ec);

						auto lines = lb(*buf, bytes_tx);
						for(const std::string& line : lines)
							read_handler_(line);

						setup_read();
					});
		}

		//! Setup a write
		void setup_write()
		{
			std::lock_guard<std::mutex> guard(wq_mutex_);

			if(!writing_ && !write_queue_.empty())
			{
				writing_ = true;

				auto msg = std::make_shared<std::string>(write_queue_.front());
				write_queue_.pop_front();

				//Compiler won't look for shared_from_this in this class unless we tell it to
				auto shared(this->shared_from_this());

				boost::asio::async_write(socket_, boost::asio::buffer(*msg),
						//Capture shared & msg to ensure lifetime
						[this, shared, msg](const boost::system::error_code& ec, std::size_t bytes_tx)
						{
							if(ec)
								throw boost::system::system_error(ec);

							wq_mutex_.lock();
							writing_ = false;
							wq_mutex_.unlock();

							setup_write();
						});
			}
		}

	};
}
