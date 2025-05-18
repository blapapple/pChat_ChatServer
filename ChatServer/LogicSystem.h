#pragma once
#include "const.h"
#include "MysqlMgr.h"
#include "CSession.h"
#include "CServer.h"

class CSession;
class LogicNode;

typedef std::function<void(std::shared_ptr<CSession>, const short& msg_id, const std::string& msg_data)> FunCallBack;
class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void PostMsgToQue(std::shared_ptr<LogicNode> msg);
private:
	LogicSystem();
	void DealMsg();
	void RegisterCallBacks();
	void LoginHandler(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data);
	std::thread _worker_thread;
	std::queue<std::shared_ptr<LogicNode>> _msg_queue;
	std::mutex _mutex;
	std::queue<std::shared_ptr<LogicNode>> _msg_que;
	std::condition_variable _consume;
	std::atomic<bool> _b_stop;
	std::map<short, FunCallBack> _fun_callbacks;
	std::unordered_map<int, std::shared_ptr<UserInfo>> _users;
};

