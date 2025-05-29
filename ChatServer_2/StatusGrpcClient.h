#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

class StatusConPool
{
public:
	StatusConPool(size_t poolSize, std::string host, std::string port);
	~StatusConPool();
	std::unique_ptr<StatusService::Stub> GetConnection();
	void ReturnConnection(std::unique_ptr<StatusService::Stub> context);

private:
	size_t _poolSize;
	std::string _host;
	std::string _port;
	std::atomic<bool> _b_stop;
	std::queue<std::unique_ptr<StatusService::Stub>> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;

	void Close() {
		_b_stop = true;
		cond_.notify_all();
	}
};

class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
	friend class Singleton<StatusGrpcClient>;
public:
	~StatusGrpcClient() {}
	GetChatServerRsp GetChatServer(int uid);
	LoginRsp Login(int uid, std::string token);

private:
	StatusGrpcClient();
	std::unique_ptr<StatusConPool> pool_;
};

