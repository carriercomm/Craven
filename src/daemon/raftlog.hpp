#pragma once

#include <vector>

namespace raft_log
{
	class Loggable
	{
	public:
		Loggable(uint32_t term);

		uint32_t term() const;

	protected:
		Json::Value write() const;

		uint32_t term_;
	};

	class LogEntry : public Loggable
	{
	public:
		LogEntry(uint32_t term, uint32_t index, const Json::Value& action);

		Json::Value write() const;

		uint32_t index() const;
		Json::Value action() const;

	protected:
		uint32_t index_;
		Json::Value action_;
	};

	class Vote : public Loggable
	{
	public:
		Vote(uint32_t term, const std::string& node);

		Json::Value write() const;


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

		//! Thrown when a vote is added that comes from an older term
		struct invalid_vote : std::runtime_error
		{
			invalid_vote(uint32_t current_term,  uint32_t term,
					const std::string& current_vote, const std::string& requested_vote);

		};
	}
}

//! Manages the Raft write-ahead log.
class RaftLog
{
public:
	//! Construct the log manager from a provided file.
	/*!
	 *  \param file_name The name of the file to use as the Raft log. It will
	 *  be opened in read/append mode and the state of the Raft log determined
	 *  from it.
	 */
	RaftLog(const char* file_name);

	//! \overload
	RaftLog(const std::string& file_name);

	//! \overload
	RaftLog(const boost::filesystem::path& file_name);

	//! Retrieve the current election term from the log.
	uint32_t term() const noexcept;

	//! Retrieve the node we voted for in the current term.
	/*!
	 *  If no vote was made this election term, the optional return value will
	 *  be null.
	 */
	boost::optional<std::string> last_vote() const noexcept;

	//! Writes a log entry to the file and internal log
	/*!
	 *  Throws if an entry with the same index (but not necessarly the same
	 *  term) exists. Will not throw when valid(entry) is true.
	 */
	void write(const raft_log::LogEntry& entry) noexcept(false);

	//! Records a vote taking place.
	/*!
	 *  If the vote is not from the current term or later, or a vote is already
	 *  recorded for this term (and is not for the same endpoint), this throws.
	 *  Otherwise, it updates the last recorded vote and writes to file.
	 */
	void write(const raft_log::Vote& vote) noexcept(false);

	//! Invalidates a log entry with a given index.
	/*!
	 *  This function throws if the index does not exist.
	 */
	void invalidate(uint32_t index) noexcept(false);

	//! Checks to see if the given log entry is valid.
	/*!
	 *  In this context, valid means in the log.
	 */
	bool valid(const raft_log::LogEntry entry) const noexcept;

	//! Checks to see if the given log index/term match our log
	bool match(uint32_t term, uint32_t index) const noexcept;

	//! Retrieves the log entry at index, throwing if it doesn't exist.
	raft_log::LogEntry log(uint32_t index) const noexcept(false);

protected:
	std::fstream stream_;
	uint32_t term_;
	boost::optional<std::string> last_vote_;

	std::vector<raft_log::LogEntry> log_;
};
