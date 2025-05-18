#pragma once
#include "const.h"
#include "LogicSystem.h"
#include "CServer.h"
#include "MsgNode.h"

class CServer;
class LogicSystem;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession(net::io_context& io_context, CServer* server);
	~CSession();
	void Close();
	tcp::socket& GetSocket() { return _socket; }
	std::string& GetSessionId() { return _session_id; }
	void SetUserId(int uid);
	int GetUuid();
	void Send(char* msg, short max_length, short msgid);
	void Send(std::string msg, short msgid);
	void AsyncReadHead(int total_len);	//读取头部数据
	void AsyncReadBody(int total_len);	//读取长度成功后，回调函数内部调用了读包体的逻辑
	/*
	* 读取包体完成后，在回调中继续读包头。以此循环往复直到读完所有数据。如果对方不发送数据，则回调函数不会触发。
	* 不会影响程序执行其他工作，因为我们采用的是asio异步的读写操作。
	* 解析完包体后会调用LogicSystem单例将解析好的消息封装为逻辑节点传递给逻辑层进行处理
	*/
	void Start() {}
private:
	//由AsyncReadHead调用，读取整个长度，然后解析收到的数据，前两个直接为id，之后两个字节为长度，最后n个长度字节为消息内容
	void AsyncReadFull(size_t maxLength, std::function<void(const boost::system::error_code&, size_t)>handler);
	void AsyncReadLen(size_t read_len, size_t total_len, std::function<void(const boost::system::error_code&, size_t)> handler);
	void AsyncReadLen(size_t read_len, size_t total_len);	//读取指定长度
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> share_self);

	int _user_uid;
	std::string _session_id;	//会话id
	char _data[MAX_LENGTH];	//数据缓冲区
	std::mutex _session_mtx; 	//会话锁
	std::mutex _send_lock;	//发送锁
	tcp::socket _socket;
	std::string _uuid;
	CServer* _server;
	std::atomic<bool> _b_close;
	std::atomic<bool> _b_head_parse;
	std::queue<shared_ptr<SendNode> > _send_que;
	std::shared_ptr<RecvNode> _recv_msg_node;
	std::shared_ptr<MsgNode> _recv_head_node;
};

class LogicNode {
	friend class LogicSystem;
public:
	LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<RecvNode> recvnode) :_session(session), _recvnode(recvnode) {};
private:
	std::shared_ptr<CSession> _session;
	std::shared_ptr<RecvNode> _recvnode;
};
