#pragma once

#include <deque>
#include <functional>
#include <memory>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/signals2.hpp>
#include <boost/asio.hpp>

#include <uuid.hpp>

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

		template <typename function_type>
		struct handler_type
		{
			typedef std::function<function_type> type;
		};

		typedef void connection_return;

		template <class F>
		struct build_connect;

		template <class R, class... Args>
		struct build_connect<R(Args...)>
		{
			typedef typename handler_type<R(Args...)>::type object_type;

			template <class Callable>
			static connection_return connect(object_type& handler, Callable&& f)
			{
				handler = object_type(std::forward<Callable>(f));
			}
		};

		template <typename function_type>
		static bool valid(const std::function<function_type>& f)
		{
			return static_cast<bool>(f);
		}
	};


	template <>
	struct connection_traits<connection_multiple_handler_tag>
	{
		typedef connection_multiple_handler_tag tag;
		typedef connection_traits<tag> type;

		template <typename function_type>
		struct handler_type
		{
			typedef boost::signals2::signal<function_type> type;
		};

		typedef boost::signals2::connection connection_return;

		template <class F>
		struct build_connect;

		template <class R, class... Args>
		struct build_connect<R(Args...)>
		{
			typedef typename handler_type<R(Args...)>::type object_type;

			template <class Callable>
			static connection_return connect(object_type& handler, Callable&& f)
			{
				return handler.connect(std::forward<Callable>(f));
			}
		};

		//signals are never invalid
		template <typename function_type>
		static bool valid(const boost::signals2::signal<function_type>&)
		{
			return true;
		}
	};

	template <class Socket, class HandlerTag, class HandlerTraits = connection_traits<HandlerTag>>
	class connection;

	namespace detail
	{
		template <class Socket, class HandlerTag, class HandlerTraits>
		class socket_detail : public std::enable_shared_from_this<socket_detail<Socket, HandlerTag, HandlerTraits>>
		{
		public:
			typedef Socket socket_type;
			typedef socket_detail<socket_type, HandlerTag, HandlerTraits> type;

			typedef connection<Socket, HandlerTag, HandlerTraits> connection_type;

			typedef std::shared_ptr<type> pointer;

		private:
			socket_detail(std::shared_ptr<connection_type> connection)
				:connection_(connection),
				writing_(false)
			{ }

		public:
			static pointer create(std::shared_ptr<connection_type> connection)
			{
				auto ptr = pointer(new socket_detail(connection));
				ptr->setup_read();

				return ptr;
			}

			void queue_write(const std::string& msg)
			{
				write_queue_.push_back(msg);

				setup_write();
			}

		protected:
			std::weak_ptr<connection_type> connection_;

			//! The message queue to be placed on the socket.
			std::deque<std::string> write_queue_;

			//! For handling the write queue
			bool writing_;

			typedef util::line_buffer<std::array<char, 4096>> line_buffer_type;
			//! Forms complete lines from buffers.
			line_buffer_type lb;

			//! Handle error codes given by the asio library
			/*!
			 *  This function is responsible for handling true errors & cleanly
			 *  shutting the socket down when it received an end of file
			 *
			 *  At the moment, it just logs the error and shuts down.
			 *
			 *  \returns True if the calling handler is allowed to continue; false o/w
			 */
			bool handle_error(const boost::system::error_code& ec)
			{
				if(ec)
				{
					if(ec != boost::asio::error::eof && ec != boost::asio::error::operation_aborted)
						BOOST_LOG_TRIVIAL(warning) << "Shutting down socket: " << ec.message();
					if(ec)
					{
						auto shared(this->shared_from_this());

						if(!connection_.expired())
						{
							auto conn = connection_.lock();

							//There's a transient issue with .expired not working.
							if(conn)
							{
								//Can't execute close in any of the handlers that call this function
								conn->socket_->get_io_service().post(
										[this, shared, conn]()
										{
											conn->close();
										});
							}
						}
					}

					return false;
				}

				return true;
			}


			//! Setup a read
			void setup_read()
			{
				auto buf = std::make_shared<line_buffer_type::buffer_type>();

				//Compiler won't look for shared_from_this in this class unless we tell it to
				auto shared(this->shared_from_this());

				if(!connection_.expired())
				{
					auto conn = connection_.lock();
					auto socket = conn->socket_;

					socket->async_receive(boost::asio::buffer(*buf),
							[this, shared, buf, socket](const boost::system::error_code& ec, std::size_t bytes_tx)
							{
								bool go = handle_error(ec);

								if(go)
								{
									if(auto conn = connection_.lock())
									{
										auto lines = lb(*buf, bytes_tx);
										for(const std::string& line : lines)
											conn->read_handler_(line);

										if(conn->is_open())
											setup_read();
									}
								}
							});
				}
			}

			//! Setup a write
			void setup_write()
			{
				if(!writing_ && !write_queue_.empty())
				{
					writing_ = true;

					auto msg = std::make_shared<std::string>(write_queue_.front());
					write_queue_.pop_front();

					//Compiler won't look for shared_from_this in this class unless we tell it to
					auto shared(this->shared_from_this());
					auto conn = connection_.lock();

					boost::asio::async_write(*(conn->socket_), boost::asio::buffer(*msg),
							//Capture shared & msg to ensure lifetime
							[this, shared, conn, msg](const boost::system::error_code& ec, std::size_t /*bytes_tx*/)
							{
								bool go = handle_error(ec);

								if(go)
								{
									writing_ = false;

									if(conn->is_open())
										setup_write();

								}
							});
				}
			}
		};

	}

	//! Connection handler
	/*!
	 *  This class handles a connection to a client from the server. It can
	 *  only be created as a std::shared_ptr -- it's a shared object. When it
	 *  destructs, it closes the socket it owns, cleaning up the connection
	 *  automatically.
	 */
	template <class Socket, class HandlerTag, class HandlerTraits>
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
		//! The socket handler type
		typedef detail::socket_detail<socket_type, handler_tag, handler_traits> socket_detail_type;
		friend socket_detail_type;


		//! Construct the connection.
		/*!
		 *  This function is only accessable within the class, so we cannot
		 *  make instances of this class outside its members. See create() for
		 *  how instances can be publicly created.
		 *  \param socket A reference to the socket for which this class is
		 *  built around. This socket is moved into the class.
		 */
		connection(std::shared_ptr<socket_type> socket)
			:socket_(socket),
			 uuid_(uuid::gen())
		{

		}
	public:
		~connection()
		{
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
			auto ptr = pointer(new type(socket));
			ptr->socket_manager_ =socket_detail_type::create(ptr);

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
			socket_manager_->queue_write(msg);
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
			typedef typename handler_traits::template build_connect<void(const std::string&)> build_connect_type;
			return build_connect_type::connect(read_handler_, std::forward<Callable>(f));
		}

		//! Register a callback for a close event
		template <class Callable>
		typename handler_traits::connection_return connect_close(Callable&& f)
		{
			typedef typename handler_traits::template build_connect<void(const std::string&)> build_connect_type;
			return build_connect_type::connect(close_handler_, std::forward<Callable>(f));
		}

		std::string uuid()
		{
			return uuid_;
		}

		//! Reports the open/closed status of the internal socket
		bool is_open() const
		{
			return socket_->is_open();
		}

		void close()
		{
			socket_->close();
			if(handler_traits::valid(close_handler_))
				close_handler_(uuid_);
		}

	protected:
		//! The socket managed by this class.
		std::shared_ptr<socket_type> socket_;

		//! The uuid for this instance
		std::string uuid_;

		typename socket_detail_type::pointer socket_manager_;

		//! Handles the read callbacks.
		typename handler_traits::template handler_type<void (const std::string&)>::type read_handler_;

		//! Handles the close callbacks.
		typename handler_traits::template handler_type<void (const std::string&)>::type close_handler_;


	};
}
