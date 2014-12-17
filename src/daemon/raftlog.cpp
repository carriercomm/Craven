#include <cstdint>

#include <string>
#include <exception>
#include <fstream>
#include <iostream>

#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>

#include <json/json.h>

#include "raftlog.hpp"

raft_log::Loggable::Loggable(uint32_t term)
{
	throw std::runtime_error("Not yet implemented");
}


uint32_t raft_log::Loggable::term() const
{
	throw std::runtime_error("Not yet implemented");
}

raft_log::LogEntry::LogEntry(uint32_t term, uint32_t index, const Json::Value& action)
	:Loggable(term)
{
	throw std::runtime_error("Not yet implemented");
}

Json::Value raft_log::LogEntry::write() const
{
	throw std::runtime_error("Not yet implemented");
}

uint32_t raft_log::LogEntry::index() const
{
	throw std::runtime_error("Not yet implemented");
}

Json::Value raft_log::LogEntry::action() const
{
	throw std::runtime_error("Not yet implemented");
}

raft_log::Vote::Vote(uint32_t term, const std::string& node)
	:Loggable(term)
{
	throw std::runtime_error("Not yet implemented");
}

Json::Value raft_log::Vote::write() const
{
	throw std::runtime_error("Not yet implemented");
}

raft_log::exceptions::entry_exists::entry_exists(uint32_t term, uint32_t index)
	:std::runtime_error("Not yet implemented")
{
	throw std::runtime_error("Not yet implemented");
}


raft_log::exceptions::entry_missing::entry_missing(uint32_t index)
	:std::runtime_error("Not yet implemented")
{
	throw std::runtime_error("Not yet implemented");
}

raft_log::exceptions::vote_exists::vote_exists(uint32_t term, const std::string& current_vote,
		const std::string& requested_vote)
	:std::runtime_error("Not yet implemented")
{
	throw std::runtime_error("Not yet implemented");
}

raft_log::exceptions::invalid_vote::invalid_vote(uint32_t current_term,  uint32_t term,
		const std::string& current_vote, const std::string& requested_vote)
	:std::runtime_error("Not yet implemented")
{
	throw std::runtime_error("Not yet implemented");
}

RaftLog::RaftLog(const char* file_name)
	:stream_("/tmp/nope")
{
	throw std::runtime_error("Not yet implemented");
}

RaftLog::RaftLog(const std::string& file_name)
	:stream_("/tmp/nope")
{
	throw std::runtime_error("Not yet implemented");
}

RaftLog::RaftLog(const boost::filesystem::path& file_name)
	:stream_("/tmp/nope")
{
	throw std::runtime_error("Not yet implemented");
}

uint32_t RaftLog::term() const noexcept
{
	throw std::runtime_error("Not yet implemented");
}

boost::optional<std::string> RaftLog::last_vote() const noexcept
{
	throw std::runtime_error("Not yet implemented");
}

void RaftLog::write(const raft_log::LogEntry& entry) noexcept(false)
{
	throw std::runtime_error("Not yet implemented");
}

void RaftLog::write(const raft_log::Vote& vote) noexcept(false)
{
	throw std::runtime_error("Not yet implemented");
}

void RaftLog::invalidate(uint32_t index) noexcept(false)
{
	throw std::runtime_error("Not yet implemented");
}

bool RaftLog::valid(const raft_log::LogEntry entry) const noexcept
{
	throw std::runtime_error("Not yet implemented");
}

bool RaftLog::match(uint32_t term, uint32_t index) const noexcept
{
	throw std::runtime_error("Not yet implemented");
}

raft_log::LogEntry RaftLog::log(uint32_t index) const noexcept(false)
{
	throw std::runtime_error("Not yet implemented");
}
