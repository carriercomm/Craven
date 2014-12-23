#include <cstdint>

#include <string>
#include <exception>
#include <fstream>
#include <iostream>
#include <functional>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>

#include <json/json.h>
#include "../common/json_help.hpp"

#include "raftlog.hpp"

raft_log::Loggable::Loggable(uint32_t term)
	:term_(term)
{
}

raft_log::Loggable::Loggable(const Json::Value& json)
{
	throw_if_not_member(json, "term");

	if(json["term"].isInt() && json["term"].asInt() >= 0)
		term_ = static_cast<uint32_t>(json["term"].asInt());
	else
		throw exceptions::json_bad_type("term", "unsigned int");
}

uint32_t raft_log::Loggable::term() const
{
	return term_;
}

Json::Value raft_log::Loggable::write() const
{
	Json::Value root;
	root["term"] = term_;
	return root;
}

void raft_log::Loggable::throw_if_not_member(const Json::Value& json, const std::string& member)
{
	if(!json.isMember(member))
		throw exceptions::json_missing_member(member);
}

raft_log::LogEntry::LogEntry()
	:Loggable(0),
	index_(0)
{

}

raft_log::LogEntry::LogEntry(uint32_t term, uint32_t index, const Json::Value& action)
	:Loggable(term),
	index_(index),
	action_(action)
{
}

raft_log::LogEntry::LogEntry(const Json::Value& json)
	:Loggable(json)
{
	throw_if_not_member(json, "type");
	throw_if_not_member(json, "index");
	throw_if_not_member(json, "action");

	if(!json["type"].isString())
		throw exceptions::json_bad_type("type", "string");

	if(!(json["index"].isInt() && json["index"].asInt() >= 0))
		throw exceptions::json_bad_type("index", "unsigned int");

	if(json["type"].asString() != "entry")
		throw exceptions::bad_json("Json not an entry");

	index_ = json["index"].asUInt();
	action_ = json["action"];
}

Json::Value raft_log::LogEntry::write() const
{
	Json::Value root = Loggable::write();
	root["type"] = "entry";
	root["index"] = index_;
	root["action"] = action_;
	return root;
}

uint32_t raft_log::LogEntry::index() const
{
	return index_;
}

Json::Value raft_log::LogEntry::action() const
{
	return action_;
}

raft_log::NewTerm::NewTerm()
	:Loggable(0)
{
}

raft_log::NewTerm::NewTerm(uint32_t term)
	:Loggable(term)
{
}

raft_log::NewTerm::NewTerm(const Json::Value& json)
	:Loggable(json)
{
	throw_if_not_member(json, "type");

	if(!json["type"].isString())
		throw exceptions::json_bad_type("type", "string");

	if(json["type"].asString() != "term")
		throw exceptions::bad_json("Json not an explicit term marker");

}

Json::Value raft_log::NewTerm::write() const
{
	auto root = Loggable::write();

	root["term"] = term_;
	root["type"] = "term";

	return root;
}

raft_log::Vote::Vote(uint32_t term, const std::string& node)
	:Loggable(term),
	node_(node)
{
}

raft_log::Vote::Vote(const Json::Value& json)
	:Loggable(json)
{
	throw_if_not_member(json, "type");
	throw_if_not_member(json, "for");

	if(!json["type"].isString())
		throw exceptions::json_bad_type("type", "string");

	if(!json["for"].isString())
		throw exceptions::json_bad_type("for", "string");

	if(json["type"].asString() != "vote")
		throw exceptions::bad_json("Json not a vote.");

	node_ = json["for"].asString();
}

Json::Value raft_log::Vote::write() const
{
	Json::Value root = Loggable::write();
	root["type"] = "vote";
	root["for"] = node_;

	return root;
}

std::string raft_log::Vote::node() const
{
	return node_;
}

raft_log::exceptions::entry_exists::entry_exists(uint32_t term, uint32_t index)
	:std::runtime_error(boost::str(boost::format("Entry exists with index %|s| (term: %|s|)") % index % term))
{
}


raft_log::exceptions::entry_missing::entry_missing(uint32_t index)
	:std::runtime_error(boost::str(boost::format("No entry with index %|s|") % index))
{
}

raft_log::exceptions::vote_exists::vote_exists(uint32_t term, const std::string& current_vote,
		const std::string& requested_vote)
	:std::runtime_error(boost::str(boost::format("Vote already exists for term %s: %s (requested %s)")
				% term % current_vote % requested_vote))
{
}

raft_log::exceptions::bad_log::bad_log(const std::string& what, uint32_t line_number)
	:std::runtime_error(boost::str(boost::format(
					"Log error on line %s: %s") % line_number % what))
{
}

raft_log::exceptions::bad_json::bad_json(const std::string& msg)
	:std::runtime_error("Bad Json: " + msg)
{
}

raft_log::exceptions::json_missing_member::json_missing_member(const std::string& member)
	:bad_json("Missing member " + member)
{
}

raft_log::exceptions::json_bad_type::json_bad_type(const std::string& member, const std::string& expected)
	:bad_json(boost::str(boost::format("Bad type for member %|s|: expected %|s|") % member % expected))
{

}

RaftLog::RaftLog(const char* file_name, std::function<void(uint32_t)> term_handler)
	:stream_(file_name, std::ios::in | std::ios::out | std::ios::app),
	//We don't want to call this during recovery
	new_term_handler_(nullptr),
	term_(0),
	last_vote_(boost::none)
{
	BOOST_LOG_TRIVIAL(info) << "Recovering log from " << file_name;
	recover();
	if(term_handler != nullptr)
		new_term_handler_ = term_handler;
	stream_.seekg(0);
	stream_.seekp(0);
	stream_.clear();
}

RaftLog::RaftLog(const std::string& file_name, std::function<void(uint32_t)> term_handler)
	:RaftLog(file_name.c_str(), term_handler)
{
}

RaftLog::RaftLog(const boost::filesystem::path& file_name, std::function<void(uint32_t)> term_handler)
	:RaftLog(file_name.c_str(), term_handler)
{
}

uint32_t RaftLog::term() const noexcept
{
	return term_;
}

boost::optional<std::string> RaftLog::last_vote() const noexcept
{
	return last_vote_;
}

uint32_t RaftLog::last_index() const noexcept
{
	return log_.size();
}

void RaftLog::write(const raft_log::LogEntry& entry) noexcept(false)
{
	handle_state(entry);
	write_json(entry.write());
}

void RaftLog::write(const raft_log::NewTerm& term) noexcept(false)
{
	handle_state(term);
	write_json(term.write());
}

void RaftLog::write(uint32_t term) noexcept(false)
{
	write(raft_log::NewTerm(term));
}

void RaftLog::write(const raft_log::Vote& vote) noexcept(false)
{
	handle_state(vote);
	write_json(vote.write());
}

void RaftLog::invalidate(uint32_t index) noexcept(false)
{
	if(index > last_index())
		throw raft_log::exceptions::entry_missing(index);

	else
		log_.resize(index - 1);

}

bool RaftLog::valid(const raft_log::LogEntry entry) const noexcept
{
	return (entry.index() <= last_index()
			&& entry.term() > (*this)[entry.index()].term())
			|| entry.index() == last_index() +1;
}

bool RaftLog::match(uint32_t term, uint32_t index) const noexcept
{
	return (index < log_.size() + 1) && (*this)[index].term() == term;
}

raft_log::LogEntry RaftLog::operator[](uint32_t index) const noexcept(false)
{
	//Bounds checking can be done by the vector
	return log_[index - 1]; //-1 because entries number from 1 and indexes from 0
}

void RaftLog::recover()
{
	//seek to start of file
	stream_.seekg(0, std::ios::beg);

	std::string line;
	unsigned line_count = 0;
	while(std::getline(stream_, line))
	{
		++line_count;
		try
		{
			recover_line(json_help::parse(line), line_count);
		}
		catch(raft_log::exceptions::bad_log& ex)
		{
			//To ensure we don't wrap it in the next one
			throw ex;
		}
		catch(std::runtime_error& ex)
		{
			throw raft_log::exceptions::bad_log(ex.what(), line_count);
		}
	}

	BOOST_LOG_TRIVIAL(info) << "Recovered raft log. Term: " << term_
		<< " last vote: " << (last_vote_ ? *last_vote_ : "none")
		<< " index: " << log_.size();
}

void RaftLog::recover_line(const Json::Value& root, uint32_t line_number)
{
	if(!root["type"].isString())
		throw raft_log::exceptions::json_bad_type("type", "string");

	std::string type = root["type"].asString();

	if(type == "vote")
	{
		raft_log::Vote vote(root);
		handle_state(vote);
	}
	else if(type == "entry")
	{
		raft_log::LogEntry entry(root);
		handle_state(entry);
	}
	else if(type == "term")
	{
		raft_log::NewTerm term(root);
		handle_state(term);
	}
	else
		throw raft_log::exceptions::bad_log("Unknown type: " + type, line_number);

}

void RaftLog::handle_state(const raft_log::LogEntry& entry) noexcept(false)
{

	if(entry.index() <= last_index())
	{
		auto stale_entry = log_[entry.index() - 1];
		if(stale_entry.term() < entry.term())
		{
			invalidate(stale_entry.index());
			log_.push_back(entry);
			handle_state(static_cast<const raft_log::Loggable&>(entry));
		}
		else
			throw raft_log::exceptions::entry_exists(entry.term(), entry.index());
	}
	else if(entry.index() == last_index() + 1)
	{
		log_.push_back(entry);
		handle_state(static_cast<const raft_log::Loggable&>(entry));
	}
	else
		throw std::runtime_error(boost::str(boost::format(
						"Entry index jump: expected %s, got %s")
						% (last_index() + 1) % entry.index()));

}

void RaftLog::handle_state(const raft_log::NewTerm& term) noexcept(false)
{
	handle_state(static_cast<const raft_log::Loggable&>(term));
}

void RaftLog::handle_state(const raft_log::Vote& vote) noexcept(false)
{
	handle_state(static_cast<const raft_log::Loggable&>(vote));

	if(!last_vote_ )
		last_vote_ = vote.node();
	else if(*last_vote_ != vote.node())
	{
		std::string current_vote = last_vote_ ? *last_vote_ : "none";
		throw raft_log::exceptions::vote_exists(term_, current_vote, vote.node());
	}

}

void RaftLog::handle_state(const raft_log::Loggable& entry) noexcept(false)
{
	if(entry.term() > term_)
	{
		term_ = entry.term();
		last_vote_ = boost::none;

		//If we have a handler to call for a new term, call it.
		if(new_term_handler_)
			new_term_handler_(term_);
	}
	else if(entry.term() < term_)
		throw std::runtime_error(boost::str(boost::format(
						"Stale term: %s, current: %s") % entry.term() % term_));
}

void RaftLog::write_json(const Json::Value& root)
{
	std::string line = json_help::write(root);

	stream_ << line;
	stream_.flush();
}
