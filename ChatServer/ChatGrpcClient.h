#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include "message.pb.h"
#include "message.grpc.pb.h"
#include "data.h"

#include <grpcpp/grpcpp.h>
#include <queue>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::ChatService;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

class ChatConPool {
public:
	ChatConPool(size_t poolSize, std::string host, std::string port);
	~ChatConPool();
	void Close();
	std::unique_ptr<ChatService::Stub> getConnection();
	void returnConnection(std::unique_ptr<ChatService::Stub> context);
private:
	size_t poolSize_;
	size_t capacity;
	std::atomic<bool> b_stop;
	std::string host_;
	std::string port_;
	//保证连接唯一性，使用unique管理
	std::queue<std::unique_ptr<ChatService::Stub>> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient() {};

	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& user_info);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);
private:
	ChatGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;
};

