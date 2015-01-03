#pragma once

#include <unordered_map>
#include <list>

#include "raftrequest.hpp"

namespace dfs
{
	class State
	{
		State();

		commit_update(const raft::rpc::Update& rpc);
		commit_delete(const raft::rpc::Delete& rpc);
		commit_rename(const raft::rpc::Rename& rpc);
		commit_add(const raft::rpc::Add& rpc);


	protected:

		//! Directory cache. Single map of list so we can represent empty dirs
		std::unordered_map<std::string, std::list<std::string>> dcache_;

	};
}
