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
	void AsyncReadHead(int total_len);	//��ȡͷ������
	void AsyncReadBody(int total_len);	//��ȡ���ȳɹ��󣬻ص������ڲ������˶�������߼�
	/*
	* ��ȡ������ɺ��ڻص��м�������ͷ���Դ�ѭ������ֱ�������������ݡ�����Է����������ݣ���ص��������ᴥ����
	* ����Ӱ�����ִ��������������Ϊ���ǲ��õ���asio�첽�Ķ�д������
	* ��������������LogicSystem�����������õ���Ϣ��װΪ�߼��ڵ㴫�ݸ��߼�����д���
	*/
	void Start() {}
private:
	//��AsyncReadHead���ã���ȡ�������ȣ�Ȼ������յ������ݣ�ǰ����ֱ��Ϊid��֮�������ֽ�Ϊ���ȣ����n�������ֽ�Ϊ��Ϣ����
	void AsyncReadFull(size_t maxLength, std::function<void(const boost::system::error_code&, size_t)>handler);
	void AsyncReadLen(size_t read_len, size_t total_len, std::function<void(const boost::system::error_code&, size_t)> handler);
	void AsyncReadLen(size_t read_len, size_t total_len);	//��ȡָ������
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> share_self);

	int _user_uid;
	std::string _session_id;	//�Ựid
	char _data[MAX_LENGTH];	//���ݻ�����
	std::mutex _session_mtx; 	//�Ự��
	std::mutex _send_lock;	//������
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
