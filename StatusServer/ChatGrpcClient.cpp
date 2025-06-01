#include "ChatGrpcClient.h"


ChatConPool::ChatConPool(size_t poolSize, std::string host, std::string port) :
	poolSize_(poolSize), host_(host), port_(port), b_stop(false) {
	for (size_t i = 0; i < poolSize_; i++) {
		std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
		connections_.push(ChatService::NewStub(channel));
	}
}

ChatConPool::~ChatConPool() {
	std::lock_guard<std::mutex> lock(mutex_);
	Close();
	while (!connections_.empty()) {
		connections_.pop();
	}
}

std::unique_ptr<ChatService::Stub> ChatConPool::getConnection()
{
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this] {
		if (b_stop) {
			return true;
		}
		return !connections_.empty();//不为空就继续往下走，为空就阻塞等待，直到连接释放notify_one或all通知,
		//all的情况下是所有连接都被释放了，b_stop = true，下方直接返回空指针 ，notify_one的情况下是有一个连接被释放了
		});
	if (b_stop) {
		return nullptr;
	}
	auto context = std::move(connections_.front());
	connections_.pop();
	return context;
}

void ChatConPool::returnConnection(std::unique_ptr<ChatService::Stub> context)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop) {
		return;
	}
	connections_.push(std::move(context));
	cond_.notify_one(); // 通知一个等待的线程，表示有连接可用
}

void ChatConPool::Close() {
	b_stop = true;
	cond_.notify_all();
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& req)
{
	return AddFriendRsp();
}

ChatGrpcClient::ChatGrpcClient()
{
	auto& cfg = ConfigMgr::Inst();
	auto server_list = cfg["ChatServers"]["Name"];
	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;

	while (std::getline(ss, word, ',')) {
		words.emplace_back(word);
	}

	for (auto& word : words) {
		if (cfg[word]["Name"].empty()) {
			continue;
		}
		_pools[cfg[word]["Name"]] = std::make_unique<ChatConPool>(5, cfg[word]["Host"], cfg[word]["Port"]);
	}
}


