#include <string>
#include <vector>
#include <set>
#include <functional>
#include <unordered_map>
#include <fstream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <json/json.h>
#include <json_help.hpp>

#include "../connection_pool.hpp"
#include "../dispatch.hpp"
#include "../raftrpc.hpp"
#include "../raftlog.hpp"
#include "../raftstate.hpp"
#include "../raftclient.hpp"
#include "../raftctl.hpp"

std::unordered_map<std::string, uint32_t> port_map =
	{
		{"foo", 9001},
		{"bar", 9002},
		{"baz", 9003}
	};

class Control
{
public:
	Control(const std::string&  id, const std::vector<std::string>& nodes);

protected:
	void setup_accept();

	void attempt_connection(const std::string& endpoint);

	void async_inject();


	std::string id_;
	uint32_t req_count_ = 0;

	//Asio
	boost::asio::io_service io_;
	boost::asio::deadline_timer t_;
	boost::asio::ip::tcp::acceptor acc_;

	TCPConnectionPool pool_;
	TopLevelDispatch<TCPConnectionPool> dispatch_;

	raft::Controller ctl_;
};

Control::Control(const std::string&  id, const std::vector<std::string>& nodes)
	:id_(id),
	io_(),
	t_(io_),
	acc_(io_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port_map.at(id))),
	pool_(std::bind(&TopLevelDispatch<TCPConnectionPool>::operator(), std::ref(dispatch_),
				std::placeholders::_1, std::placeholders::_2)),
	dispatch_(pool_),
	ctl_(io_, dispatch_, raft::Controller::TimerLength(1, 1000, 3000, 500), id, nodes,
			(fs::temp_directory_path() / fs::path(id)).string())
{
	setup_accept();

	for(const std::string& node : nodes)
		attempt_connection(node);

	async_inject();

	try
	{
		io_.run();
	}
	catch(const std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(fatal) << ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(fatal) << "Unknown exception at top level; aborting.";
	}
}


void Control::setup_accept()
{
	auto sock = std::make_shared<boost::asio::ip::tcp::socket>(io_);

	acc_.async_accept(*sock, [this, sock](const boost::system::error_code& ec)
			{
				if(!ec)
				{
					auto conn = TCPConnectionPool::connection_type::create(sock);

					bool fired = false;
					conn->connect_read([this, conn, fired](const std::string& ot) mutable
							{
								if(!fired)
								{
									fired = true;

									if(!pool_.exists(ot))
									{
										BOOST_LOG_TRIVIAL(info) << "Connected to: " << ot;
										pool_.add_connection(ot, conn);
									}

									//Drop our reference to the connection.
									conn.reset();
								}
							});

					//Continue accepting
					setup_accept();
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Error on accept: " << ec.message();
			});
}

void Control::attempt_connection(const std::string& endpoint)
{
	auto sock = std::make_shared<boost::asio::ip::tcp::socket>(io_);
	sock->async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port_map.at(endpoint)),
			[this, sock, endpoint](const boost::system::error_code& ec)
			{
				if(!ec)
				{
					auto conn = TCPConnectionPool::connection_type::create(sock);

					if(!pool_.exists(endpoint))
					{
						BOOST_LOG_TRIVIAL(info) << "Connected to: " << endpoint;
						conn->queue_write(id_ + '\n');
						pool_.add_connection(endpoint, conn);
					}
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Error on connect to " << endpoint << ": "
						<< ec.message();
			});
}

void Control::async_inject()
{
	t_.expires_from_now(boost::posix_time::seconds(5));

	t_.async_wait([this](const boost::system::error_code& error)
	{
		switch(req_count_)
		{
		case 0:
			if(id_ == "baz")
				ctl_.client().request(raft::request::Add(id_, "root", "0"));
			break;
		case 1:
			if(id_ == "baz")
				ctl_.client().request(raft::request::Update(id_, "root", "0", "1"));
			break;

		case 2:
			if(id_ == "bar")
				ctl_.client().request(raft::request::Rename(id_, "root", "boot", "1"));
			break;

		};

		if(++req_count_ < 3)
			async_inject();
	});


}

int main(int argc, char** argv)
{
	std::vector<std::string> nodes = {"foo", "bar", "baz"};
	std::string id = argv[1];

	nodes.erase(std::find(nodes.begin(), nodes.end(), id));

	Control c(argv[1], nodes);

	return 0;
}
