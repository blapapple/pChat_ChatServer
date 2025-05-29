#include "StatusGrpcClient.h"

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
	ClientContext context;
	GetChatServerRsp reply;
	GetChatServerReq request;
	request.set_uid(uid);
	auto stub = pool_->GetConnection();
	Status status = stub->GetChatServer(&context, request, &reply);
	Defer defer([&stub, this]() {
		pool_->ReturnConnection(std::move(stub));
		});
	if (status.ok())
	{
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
	ClientContext context;
	LoginRsp reply;
	LoginReq request;
	request.set_uid(uid);
	request.set_token(token);

	auto stub = pool_->GetConnection();
	Status status = stub->Login(&context, request, &reply);
	Defer defer([&stub, this]() {
		pool_->ReturnConnection(std::move(stub));
		});
	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

StatusGrpcClient::StatusGrpcClient()
{
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["StatusServer"]["Host"];
	std::string port = gCfgMgr["StatusServer"]["Port"];
	pool_.reset(new StatusConPool(5, host, port));
}

StatusConPool::StatusConPool(size_t poolSize, std::string host, std::string port) : _poolSize(poolSize), _host(host),
_port(port), _b_stop(false)
{
	for (size_t i = 0; i < _poolSize; ++i)
	{
		std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port, grpc::InsecureChannelCredentials());
		connections_.emplace(StatusService::NewStub(channel));
	}
}

StatusConPool::~StatusConPool()
{
	std::lock_guard<std::mutex> lock(mutex_);
	Close();
	while (!connections_.empty())
	{
		connections_.pop();
	}
}

std::unique_ptr<StatusService::Stub> StatusConPool::GetConnection() {
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this] {
		if (_b_stop) {
			return true;
		}
		return !connections_.empty();
		});
	if (_b_stop) {
		return nullptr;
	}
	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void StatusConPool::ReturnConnection(std::unique_ptr<StatusService::Stub> context) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (_b_stop) {
		return;
	}
	connections_.emplace(std::move(context));
	cond_.notify_one();
}