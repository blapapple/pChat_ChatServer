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
		return !connections_.empty();//��Ϊ�վͼ��������ߣ�Ϊ�վ������ȴ���ֱ�������ͷ�notify_one��all֪ͨ,
		//all����������������Ӷ����ͷ��ˣ�b_stop = true���·�ֱ�ӷ��ؿ�ָ�� ��notify_one�����������һ�����ӱ��ͷ���
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
	cond_.notify_one(); // ֪ͨһ���ȴ����̣߳���ʾ�����ӿ���
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


