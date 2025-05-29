#pragma once
#include "const.h"
#include "CSession.h"

class CSession;

//ChatServer�����ǽ���tcp���ӣ�����CServerʵ������ǰGateServer��Http��ͬ
class CServer :public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& ioc, unsigned short& port);
	~CServer();
	void Start();
	void ClearSession(std::string);
private:
	void HandleAccept(std::shared_ptr<CSession> session, const boost::system::error_code& error);
	void StartAccept();
	boost::asio::io_context& _io_context;
	short _port;
	std::map <std::string, std::shared_ptr<CSession>> _sessions;
	tcp::acceptor _acceptor;
	std::mutex _mutex;
};

