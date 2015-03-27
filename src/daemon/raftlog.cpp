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
#include <json_help.hpp>

#include "raftlog.hpp"

raft::log::Loggable::Loggable(uint32_t term)
	:term_(term)
{
}

raft::log::Loggable::Loggable(const Json::Value& json)
{
	throw_if_not_member(json, "term");

	if(json["term"].isInt() && json["term"].asInt() >= 0)
		term_ = static_cast<uint32_t>(json["term"].asInt());
	else
		throw exceptions::json_bad_type("term", "unsigned int");
}

uint32_t raft::log::Loggable::term() const
{
	return term_;
}

raft::log::Loggable::operator Json::Value() const
{
	Json::Value root;
	root["term"] = term_;
	return root;
}

void raft::log::Loggable::throw_if_not_member(const Json::Value& json, const std::string& member)
{
	if(!json.isMember(member))
		throw exceptions::json_missing_member(member);
}

raft::log::LogEntry::LogEntry()
	:Loggable(0),
	index_(0),
	spawn_term_(0)
{

}

raft::log::LogEntry::LogEntry(uint32_t term, uint32_t index, uint32_t spawn_term,
		const Json::Value& action)
	:Loggable(term),
	index_(index),
	spawn_term_(spawn_term),
	action_(action)
{
}

raft::log::LogEntry::LogEntry(const Json::Value& json)
	:Loggable(json)
{
	throw_if_not_member(json, "type");
	throw_if_not_member(json, "index");
	throw_if_not_member(json, "action");
	throw_if_not_member(json, "spawn_term");

	if(!json["type"].isString())
		throw exceptions::json_bad_type("type", "string");

	if(!(json["index"].isInt() && json["index"].asInt() >= 0))
		throw exceptions::json_bad_type("index", "unsigned int");

	if(!(json["spawn_term"].isInt() && json["spawn_term"].asInt() >= 0))
		throw exceptions::json_bad_type("spawn_term", "unsigned int");

	if(json["type"].asString() != "entry")
		throw exceptions::bad_json("Json not an entry");


	index_ = json["index"].asUInt();
	spawn_term_ = json["spawn_term"].asUInt();
	action_ = json["action"];
}

raft::log::LogEntry::operator Json::Value() const
{
	Json::Value root = Loggable::operator Json::Value();
	root["type"] = "entry";
	root["index"] = index_;
	root["spawn_term"] = spawn_term_;
	root["action"] = action_;
	return root;
}

uint32_t raft::log::LogEntry::index() const
{
	return index_;
}

uint32_t raft::log::LogEntry::spawn_term() const
{
	return spawn_term_;
}

Json::Value raft::log::LogEntry::action() const
{
	return action_;
}

raft::log::NewTerm::NewTerm()
	:Loggable(0)
{
}

raft::log::NewTerm::NewTerm(uint32_t term)
	:Loggable(term)
{
}

raft::log::NewTerm::NewTerm(const Json::Value& json)
	:Loggable(json)
{
	throw_if_not_member(json, "type");

	if(!json["type"].isString())
		throw exceptions::json_bad_type("type", "string");

	if(json["type"].asString() != "term")
		throw exceptions::bad_json("Json not an explicit term marker");

}

raft::log::NewTerm::operator Json::Value() const
{
	auto root = Loggable::operator Json::Value();

	root["term"] = term_;
	root["type"] = "term";

	return root;
}

raft::log::Vote::Vote(uint32_t term, const std::string& node)
	:Loggable(term),
	node_(node)
{
}

raft::log::Vote::Vote(const Json::Value& json)
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

raft::log::Vote::operator Json::Value() const
{
	Json::Value root = Loggable::operator Json::Value();
	root["type"] = "vote";
	root["for"] = node_;

	return root;
}

std::string raft::log::Vote::node() const
{
	return node_;
}

raft::log::CommitMarker::CommitMarker(uint32_t term, uint32_t index)
	:Loggable(term),
	index_(index)
{

}

raft::log::CommitMarker::CommitMarker(const Json::Value& json)
	:Loggable(json)
{
	throw_if_not_member(json, "type");
	throw_if_not_member(json, "index");

	if(!json["type"].isString())
		throw exceptions::json_bad_type("type", "string");

	if(!(json["index"].isInt() || json["index"].asInt() >= 0))
		throw exceptions::json_bad_type("index", "uint");

	if(json["type"].asString() != "commit")
		throw exceptions::bad_json("Json not a commit marker.");

	index_ = json["index"].asInt();
}

raft::log::CommitMarker::operator Json::Value() const
{
	Json::Value root = Loggable::operator Json::Value();
	root["type"] = "commit";
	root["index"] = index_;

	return root;
}

uint32_t raft::log::CommitMarker::index() const
{
	return index_;
}

raft::log::exceptions::entry_exists::entry_exists(uint32_t term, uint32_t index)
	:std::runtime_error(boost::str(boost::format("Entry exists with index %|s| (term: %|s|)") % index % term))
{
}

raft::log::exceptions::term_conflict::term_conflict(uint32_t proposed_term, uint32_t conflicting_term,
		uint32_t index)
	:std::runtime_error(boost::str(boost::format("Addition of entry with index %|s| and term %|s| would cause a decrease in term from %|s|") % index % proposed_term % conflicting_term))
{
}


raft::log::exceptions::entry_missing::entry_missing(uint32_t index)
	:std::runtime_error(boost::str(boost::format("No entry with index %|s|") % index))
{
}

raft::log::exceptions::vote_exists::vote_exists(uint32_t term, const std::string& current_vote,
		const std::string& requested_vote)
	:std::runtime_error(boost::str(boost::format("Vote already exists for term %s: %s (requested %s)")
				% term % current_vote % requested_vote))
{
}

raft::log::exceptions::bad_log::bad_log(const std::string& what, uint32_t line_number)
	:std::runtime_error(boost::str(boost::format(
					"Log error on line %s: %s") % line_number % what))
{
}

raft::log::exceptions::bad_json::bad_json(const std::string& msg)
	:std::runtime_error("Bad Json: " + msg)
{
}

raft::log::exceptions::json_missing_member::json_missing_member(const std::string& member)
	:bad_json("Missing member " + member)
{
}

raft::log::exceptions::json_bad_type::json_bad_type(const std::string& member, const std::string& expected)
	:bad_json(boost::str(boost::format("Bad type for member %|s|: expected %|s|") % member % expected))
{

}

raft::Log::Log(const char* file_name, std::function<void(uint32_t)> term_handler)
	:stream_(file_name, std::ios::in | std::ios::out | std::ios::app),
	//We don't want to call this during recovery
	new_term_handler_(nullptr),
	term_(0),
	last_vote_(boost::none),
	commit_index_(0)
{
	BOOST_LOG_TRIVIAL(info) << "Recovering log from " << file_name;
	recover();
	if(term_handler != nullptr)
		new_term_handler_ = term_handler;
	stream_.seekg(0);
	stream_.seekp(0);
	stream_.clear();
}

raft::Log::Log(const std::string& file_name, std::function<void(uint32_t)> term_handler)
	:Log(file_name.c_str(), term_handler)
{
}

raft::Log::Log(const boost::filesystem::path& file_name, std::function<void(uint32_t)> term_handler)
	:Log(file_name.c_str(), term_handler)
{
}

uint32_t raft::Log::term() const noexcept
{
	return term_;
}

boost::optional<std::string> raft::Log::last_vote() const noexcept
{
	return last_vote_;
}

uint32_t raft::Log::last_index() const noexcept
{
	return log_.size();
}

void raft::Log::write(uint32_t term) noexcept(false)
{
	write(raft::log::NewTerm(term));
}

void raft::Log::invalidate(uint32_t index) noexcept(false)
{
	if(index > last_index())
		throw raft::log::exceptions::entry_missing(index);

	else
		log_.resize(index - 1);

}

bool raft::Log::valid(const raft::log::LogEntry entry) const noexcept
{
	return (entry.index() <= last_index()
			&& entry.term() > (*this)[entry.index()].term())
			|| (entry.index() == last_index() +1
					&& entry.spawn_term() >= (*this)[last_index()].term()
			   );
}

bool raft::Log::match(uint32_t term, uint32_t index) const noexcept
{
	//special case
	if(term == 0)
		return index == 0;
	BOOST_LOG_TRIVIAL(trace) << "Match: (" << term << ", " << index
		<<") with ("
		<< ((index <= log_.size()) ? std::to_string((*this)[index].spawn_term()) : "no term")
		<< ", " << log_.size() << ")";

	return (index <= log_.size()) && (*this)[index].spawn_term() == term;
}

raft::log::LogEntry raft::Log::operator[](uint32_t index) const noexcept(false)
{
	//Bounds checking can be done by the vector
	return log_[index - 1]; //-1 because entries number from 1 and indexes from 0
}

uint32_t raft::Log::commit_index() const
{
	return commit_index_;
}

void raft::Log::commit_index(uint32_t index)
{
	raft::log::CommitMarker marker{term_, index};
	write(marker);
}

void raft::Log::recover()
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
		catch(raft::log::exceptions::bad_log& ex)
		{
			//To ensure we don't wrap it in the next one
			throw ex;
		}
		catch(std::runtime_error& ex)
		{
			throw raft::log::exceptions::bad_log(ex.what(), line_count);
		}
	}

	BOOST_LOG_TRIVIAL(info) << "Recovered raft log. Term: " << term_
		<< " last vote: " << (last_vote_ ? *last_vote_ : "none")
		<< " index: " << log_.size();
}

void raft::Log::recover_line(const Json::Value& root, uint32_t line_number)
{
	if(!root["type"].isString())
		throw raft::log::exceptions::json_bad_type("type", "string");

	std::string type = root["type"].asString();

	if(type == "vote")
	{
		raft::log::Vote vote(root);
		handle_state(vote);
	}
	else if(type == "entry")
	{
		raft::log::LogEntry entry(root);
		handle_state(entry);
	}
	else if(type == "term")
	{
		raft::log::NewTerm term(root);
		handle_state(term);
	}
	else if(type == "commit")
	{
		raft::log::CommitMarker marker{root};
		handle_state(marker);
	}
	else
		throw raft::log::exceptions::bad_log("Unknown type: " + type, line_number);

}

void raft::Log::handle_state(const raft::log::LogEntry& entry) noexcept(false)
{
	if(entry.index() <= last_index())
	{
		//Check the preceeding entry's term for conflict
		if(entry.index() > 2 && log_[entry.index() - 2].term() > entry.term())
			throw raft::log::exceptions::term_conflict(entry.term(),
					log_[entry.index() - 2].term(), entry.index());

		//Clear invalid entries
		invalidate(entry.index());

		//Add this entry
		log_.push_back(entry);
		BOOST_LOG_TRIVIAL(info) << "Added a log entry, now on index " << last_index();

		//sanity check the spawn term
		if(entry.term() < entry.spawn_term())
			BOOST_LOG_TRIVIAL(warning) << "Impossible spawn term for log entry: "
				<< entry.spawn_term() << " from term: " << entry.term();

		//Want a quiet ignore if the term is lower, but still bump up if term is
		//greater.
		if(entry.term() > term_)
		{
			term_ = entry.term();
			last_vote_ = boost::none;

			//If we have a handler to call for a new term, call it.
			if(new_term_handler_)
				new_term_handler_(term_);
		}
	}
	else if(entry.index() == last_index() + 1)
	{
		log_.push_back(entry);
		BOOST_LOG_TRIVIAL(info) << "Added a log entry, now on index " << last_index();
		handle_state(static_cast<const raft::log::Loggable&>(entry));
	}
	else
		throw std::runtime_error(boost::str(boost::format(
						"Entry index jump: expected %s, got %s")
						% (last_index() + 1) % entry.index()));

}

void raft::Log::handle_state(const raft::log::NewTerm& term) noexcept(false)
{
	handle_state(static_cast<const raft::log::Loggable&>(term));
}

void raft::Log::handle_state(const raft::log::Vote& vote) noexcept(false)
{
	handle_state(static_cast<const raft::log::Loggable&>(vote));

	if(!last_vote_ )
		last_vote_ = vote.node();
	else if(*last_vote_ != vote.node())
	{
		std::string current_vote = last_vote_ ? *last_vote_ : "none";
		throw raft::log::exceptions::vote_exists(term_, current_vote, vote.node());
	}

}

void raft::Log::handle_state(const raft::log::Loggable& entry) noexcept(false)
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

void raft::Log::handle_state(const raft::log::CommitMarker& marker) noexcept(false)
{
	handle_state(static_cast<const raft::log::Loggable&>(marker));

	if(marker.index() < commit_index_)
		throw std::runtime_error("Invalid commit index: can't go backwards");

	commit_index_ = marker.index();
}

void raft::Log::write_json(const Json::Value& root)
{
	std::string line = json_help::write(root);

	stream_ << line;
	stream_.flush();

	//Remove the newline
	line.pop_back();
	BOOST_LOG_TRIVIAL(trace) << "Wrote to log: " << line;
}
