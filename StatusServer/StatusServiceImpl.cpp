#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"

std::string generate_unique_string() {
	// 创建UUID对象，雪花算法
	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	// 将UUID转换为字符串
	std::string unique_string = to_string(uuid);

	return unique_string;
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
	std::string prefix("pChat status server has received :  ");
	std::cout << prefix << request->ShortDebugString() << std::endl;
	const auto& server = getChatServer();
	reply->set_host(server.host);
	reply->set_port(server.port);
	reply->set_error(ErrorCodes::Success);
	reply->set_token(generate_unique_string());
	insertToken(request->uid(), reply->token());
	return Status::OK;
}

ChatServer StatusServiceImpl::getChatServer()
{
	std::lock_guard<std::mutex> guard(_server_mtx);
	auto minServer = _servers.begin()->second;
	auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, minServer.name);
	if (count_str.empty()) {
		//不存在则默认最大值
		minServer.con_count = INT_MAX;
	}
	else {
		minServer.con_count = std::stoi(count_str);
	}

	for (auto& server : _servers) {
		if (server.second.name == minServer.name) {
			continue;	//跳过当前最小的服务器
		}
		auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.second.name);
		if (count_str.empty()) {
			server.second.con_count = INT_MAX;
		}
		else {
			server.second.con_count = std::stoi(count_str);
		}
		if (server.second.con_count < minServer.con_count) {
			minServer = server.second;
		}
	}
	//for (const auto& server : _servers) {		//可以尝试用小根堆优化
	//	if (server.second.con_count < minServer.con_count) {
	//		minServer = server.second;
	//	}
	//}
	return minServer;
}

Status StatusServiceImpl::Login(ServerContext* context, const message::LoginReq* request, message::LoginRsp* reply)
{
	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (success) {//若成功则代表此用户已登录，不允许再次登录
		reply->set_error(ErrorCodes::UidInvalid);
		return Status::OK;
	}
	if (token_value != token) {
		reply->set_error(ErrorCodes::TokenInvalid);
		return Status::OK;
	}

	reply->set_error(ErrorCodes::Success);
	reply->set_token(token);
	reply->set_uid(uid);
	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token)
{
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(token_key, token);
}

StatusServiceImpl::StatusServiceImpl()
{
	auto& cfg = ConfigMgr::Inst();
	auto server_list = cfg["ChatServers"]["Name"];

	std::vector<std::string> words;
	std::stringstream ss(server_list);
	std::string word;

	while (std::getline(ss, word, ',')) {
		words.emplace_back(word);
	}

	for (auto word : words) {
		if (cfg[word]["Name"].empty()) {
			continue;
		}

		ChatServer server;
		server.port = cfg[word]["Port"];
		server.host = cfg[word]["Host"];
		server.name = cfg[word]["Name"];
		_servers[server.name] = server;
	}
}