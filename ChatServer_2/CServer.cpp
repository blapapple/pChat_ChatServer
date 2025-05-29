#include "CServer.h"
//#include "HttpConnection.h"
#include "AsioIOServicePool.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port) :_io_context(ioc),
_acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
	std::cout << "Server started on port: " << port << std::endl;
	StartAccept();
}

CServer::~CServer() {
	cout << "Server destruct listen on port : " << _port << endl;
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error) {
	if (!error) {
		new_session->Start();
		std::lock_guard<std::mutex> lock(_mutex);
		_sessions.insert(std::make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		std::cout << "session accept failed, error is " << error.what() << std::endl;
	}

	StartAccept();
}

void CServer::StartAccept() {
	auto& _io_context = AsioIOServicePool::GetInstance()->GetIOService();
	std::shared_ptr<CSession> new_session = std::make_shared<CSession>(_io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, std::placeholders::_1));
}

void CServer::ClearSession(std::string uuid) {
	std::lock_guard<std::mutex> lock(_mutex);
	_sessions.erase(uuid);
}