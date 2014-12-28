#pragma once

#include <random>

namespace raft
{
	//! The raft controller.
	/*!
	 *  Creates the state machine and client handler, binding them together as
	 *  they require.
	 */
	class Controller
	{
	public:
		//! Class abstracting timer length generation
		class TimerLength
		{
		public:
			//! Construct the timer length generator
			/*!
			 *	The timeouts are distributed uniformly as average +- fuzz/2.
			 *
			 *  \param leader The average length of a leader timeout
			 *  \param election The average length of an election timeout
			 *  \param fuzz The timeouts vary with a range of this parameter.
			 */
			TimerLength(uint32_t seed, uint32_t leader, uint32_t election, uint32_t fuzz);

			//! Generate a timeout of the specified type.
			uint32_t operator()(State::Handlers::timeout_length length);

		protected:
			std::mt19937 gen_;
			std::uniform_int_distribution<uint32_t> leader_, election_;
		};

		typedef TopLevelDispatch<TCPConnectionPool> dispatch_type;

		//! Construct the controller
		/*!
		 *  \param io The process' io_service, used for the raft timers
		 *  \param tld The top-level dispatch, used to register dispatch handlers
		 *  \param id The ID of this process
		 *  \param nodes All nodes in the system, not including this one
		 *  \param log_file The path to the log_file
		 */
		Controller(boost::asio::io_service& io, dispatch_type& dispatch, const TimerLength& tl,
				const std::string& id, const std::vector<std::string>& nodes,
				const std::string& log_file);

		//! Retrieve the state
		State& state();
		//! \overload
		const State& state() const;

		//! Dispatch manager for the state
		void dispatch_state(const Json::Value& msg, typename dispatch_type::Callback cb);


		//! Retrieve the client
		Client& client();

		//! \overload
		const Client& client() const;

		//! Dispatch manager for the client
		void dispatch_client(const Json::Value& msg);

	protected:
		boost::asio::io_service& io_;
		TimerLength tl_;
		boost::asio::deadline_timer t_;

		State::Handlers state_handlers_;
		Client::Handlers client_handlers_;

		//The RPC send handlers
		std::function<void(const std::string&, const std::string&, const Json::Value&)>
			state_rpc_, client_rpc_;

		State state_;
		Client client_;

		void async_reset_timer(State::Handlers::timeout_length length);
	};

}
