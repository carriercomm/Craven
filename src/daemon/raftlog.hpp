#pragma once

#include <vector>
#include <fstream>

namespace raft
{
	namespace log
	{
		class Loggable
		{
		public:
			Loggable(uint32_t term);
			Loggable(const Json::Value& json);

			uint32_t term() const;

		protected:
			Json::Value write() const;
			void throw_if_not_member(const Json::Value& json, const std::string& member);

			uint32_t term_;
		};

		class LogEntry : public Loggable
		{
		public:
			//! Constructs the zeroth log entry, which doesn't actually exist.
			LogEntry();

			LogEntry(uint32_t term, uint32_t index, const Json::Value& action);
			LogEntry(const Json::Value& json);

			Json::Value write() const;

			uint32_t index() const;
			Json::Value action() const;

		protected:
			uint32_t index_;
			Json::Value action_;
		};

		//! Only difference from Loggable is that this produces more JSON
		class NewTerm : public Loggable
		{
		public:
			//! Constructs the zeroth term, which is the default startup term.
			NewTerm();

			NewTerm(uint32_t term);
			NewTerm(const Json::Value& json);

			Json::Value write() const;
		};

		class Vote : public Loggable
		{
		public:
			Vote(uint32_t term, const std::string& node);
			Vote(const Json::Value& json);

			Json::Value write() const;

			std::string node() const;


		protected:
			std::string node_;
		};

		namespace exceptions
		{
			//! Exception thrown when an entry with the same index already exists.
			struct entry_exists : std::runtime_error
			{
				entry_exists(uint32_t term, uint32_t index);
			};

			//! Exception thrown when addition of an entry would cause the term to
			//! decrease
			struct term_conflict : std::runtime_error
			{
				term_conflict(uint32_t proposed_term, uint32_t conflicting_term,
						uint32_t index);
			};

			//! Exception thrown when an entry is invalidated that does not exist.
			struct entry_missing : std::runtime_error
			{
				entry_missing(uint32_t index);
			};

			//! Exception thrown when a vote already exists for the current term.
			struct vote_exists : std::runtime_error
			{
				vote_exists(uint32_t term, const std::string& current_vote, const
						std::string& requested_vote);
			};

			struct bad_log : std::runtime_error
			{
				bad_log(const std::string& what, uint32_t line_number);
			};

			struct bad_json : std::runtime_error
			{
				bad_json(const std::string& msg);
			};

			struct json_missing_member : bad_json
			{
				json_missing_member(const std::string& member);
			};

			struct json_bad_type : bad_json
			{
				json_bad_type(const std::string& member, const std::string& expected);

			};
		}
	}

	//! Manages the Raft write-ahead log.
	class Log
	{
	public:
		//! Construct the log manager from a provided file.
		/*!
		 *  \param file_name The name of the file to use as the Raft log. It will
		 *  be opened in read/append mode and the state of the Raft log determined
		 *  from it.
		 *
		 *  \param term_handler The handler to call when the term count advances.
		 */
		Log(const char* file_name, std::function<void(uint32_t)> term_handler = nullptr);

		//! \overload
		Log(const std::string& file_name, std::function<void(uint32_t)> term_handler = nullptr);

		//! \overload
		Log(const boost::filesystem::path& file_name, std::function<void(uint32_t)> term_handler = nullptr);

		//! Retrieve the current election term from the log.
		uint32_t term() const noexcept;

		//! Retrieve the node we voted for in the current term.
		/*!
		 *  If no vote was made this election term, the optional return value will
		 *  be null.
		 */
		boost::optional<std::string> last_vote() const noexcept;

		//! Retrieve the last known log index
		/*!
		 *  Note that indexes are numbered from one. (i.e. the 0th index means we have
		 *  no log entries).
		 */
		uint32_t last_index() const noexcept;

		//! Writes a loggable to the log.
		template <typename Log>
		void write(const Log& log) noexcept(false)
		{
			handle_state(log);
			write_json(log.write());
		}

		//Helper for terms
		void write(uint32_t term) noexcept(false);

		//! Invalidates a log entry with a given index.
		/*!
		 *  This function throws if the index does not exist.
		 */
		void invalidate(uint32_t index) noexcept(false);

		//! Checks to see if the given log entry is valid.
		/*!
		 *  In this context, valid means that it can be added to the log.
		 */
		bool valid(const raft::log::LogEntry entry) const noexcept;

		//! Checks to see if the given log index/term match our log
		/*!
		 *	This function checks the log to see if an entry with the same term and
		 *	index exist. If index is greater than the last we know about this returns
		 *	false.
		 */
		bool match(uint32_t term, uint32_t index) const noexcept;

		//! Retrieves the log entry at index, throwing if it doesn't exist.
		raft::log::LogEntry operator[](uint32_t index) const noexcept(false);

	protected:
		std::fstream stream_;

		std::function<void(uint32_t)> new_term_handler_;

		uint32_t term_;
		boost::optional<std::string> last_vote_;

		std::vector<raft::log::LogEntry> log_;

		void recover();

		void recover_line(const Json::Value& root, uint32_t line_number);

		//! Handles the state change required to add a log entry without writing to
		//! the log file.
		/*!
		 *  This function is responsible for updating internal state to reflect the
		 *  addition of the log entry log. It performs sanity checks too: that the
		 *  index and term make sense, throwing if they don't.
		 *
		 *  This function is used in log recovery & writing and makes a call to
		 *  handle_state(const raft::log::Loggable) to check the term.
		 *
		 *  \param log The log entry to update with
		 */
		void handle_state(const raft::log::LogEntry& log) noexcept(false);

		//! Handles the state change required to add an explicit term notification
		//! without writing to the log file.
		/*!
		 *  This function is responsible for updating internal state to reflect the
		 *  addition of an explicit new term marker. It also checks that the term
		 *  isn't older than the current one, throwing if that's not the case.
		 *
		 *  \param term The explicit term marker to update with.
		 */
		void handle_state(const raft::log::NewTerm& term) noexcept(false);

		//! Handles the state change required to add a vote without writing to
		//! the log file.
		/*!
		 *  This function is responsible for updating internal state to reflect a new
		 *  vote. It performs sanity checks too: that there hasn't been a vote made
		 *  this term already and that the vote is from the current term or later,
		 *  throwing if these fail.
		 *
		 *  This function is used in log recovery & writing and makes a call to
		 *  handle_state(const raft::log::Loggable) to check the term.
		 *
		 *  \param vote The vote to update with
		 */
		void handle_state(const raft::log::Vote& vote) noexcept(false);

		//! Checks the validity of the term on the loggable, updating the internal
		//! state to match if it's later.
		/*!
		 *  This function is used in log recovery & writing.
		 *
		 *  \param entry The loggable holding the proposed election term
		 */
		void handle_state(const raft::log::Loggable& entry) noexcept(false);

		//! Write the given JSON to the end of the log
		void write_json(const Json::Value& root);
	};
}
