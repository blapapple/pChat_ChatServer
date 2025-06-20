#include "RedisMgr.h"
#include "ConfigMgr.h"

RedisMgr::RedisMgr() {
	auto& gConfigMgr = ConfigMgr::Inst();
	std::string host = gConfigMgr["Redis"]["Host"];
	std::string port = gConfigMgr["Redis"]["Port"];
	std::string password = gConfigMgr["Redis"]["Passwd"];
	_con_pool.reset(new RedisConPool(20, host.c_str(), atoi(port.c_str()), password.c_str()));

}

bool RedisMgr::Get(const std::string& key, std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		std::cout << "no connection" << std::endl;
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "GET %s", key.c_str());
	if (reply == nullptr) {
		std::cout << "[ GET " << key << " ] failed" << std::endl;
		freeReplyObject(reply);
		return false;
	}

	if (reply->type != REDIS_REPLY_STRING) {
		std::cout << "[ GET " << key << " ] failed, type error" << std::endl;
		freeReplyObject(reply);
		return false;
	}

	value = reply->str;
	freeReplyObject(reply);

	std::cout << "Succeed to execute command [ GET " << key << " ]" << std::endl;
	return true;
}

bool RedisMgr::Set(const std::string& key, const std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());
	if (reply == nullptr) {
		std::cout << "[ SET " << key << " ] failed" << std::endl;
		freeReplyObject(reply);
		return false;
	}

	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok")))) {
		std::cout << "[ SET " << key << " ] failed, type error" << std::endl;
	}

	freeReplyObject(reply);
	std::cout << "Succeed to execute command [ SET " << key << " " << value << " ] success !" << std::endl;
	return true;
}

bool RedisMgr::Auth(const std::string& password) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "AUTH %s", password.c_str());
	if (reply->type == REDIS_REPLY_ERROR) {
		std::cout << "认证失败" << std::endl;
		freeReplyObject(reply);
		return false;
	}
	else {
		freeReplyObject(reply);
		std::cout << "认证成功" << std::endl;
		return true;
	}
}

bool RedisMgr::LPush(const std::string& key, const std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (nullptr == reply) {
		std::cout << "[ LPush " << key << " ] failed" << std::endl;
		freeReplyObject(reply);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		std::cout << "[ LPush " << key << " " << value << " ] failed, type error" << std::endl;
		freeReplyObject(reply);
		return false;
	}

	std::cout << "Succeed to execute command [ LPush " << key << " " << value << " ]" << std::endl;
	freeReplyObject(reply);
	return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
		std::cout << "Execut command [ LPOP " << key << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	value = reply->str;
	std::cout << "Execut command [ LPOP " << key << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}

	std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
		std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	value = reply->str;
	std::cout << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}


bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	const char* argv[4];
	size_t argvlen[4];
	argv[0] = "HSET";
	argvlen[0] = 4;
	argv[1] = key;
	argvlen[1] = strlen(key);
	argv[2] = hkey;
	argvlen[2] = strlen(hkey);
	argv[3] = hvalue;
	argvlen[3] = hvaluelen;
	auto reply = (redisReply*)redisCommandArgv(connect, 4, argv, argvlen);
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return "";
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	const char* argv[3];
	size_t argvlen[3];
	argv[0] = "HGET";
	argvlen[0] = 4;
	argv[1] = key.c_str();
	argvlen[1] = key.length();
	argv[2] = hkey.c_str();
	argvlen[2] = hkey.length();
	auto reply = (redisReply*)redisCommandArgv(connect, 3, argv, argvlen);
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
		std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! " << std::endl;
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
	return value;
}

bool RedisMgr::Del(const std::string& key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
		std::cout << "Execut command [ Del " << key << " ] failure ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	std::cout << "Execut command [ Del " << key << " ] success ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

bool RedisMgr::ExistsKey(const std::string& key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr)
	{
		return false;
	}
	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});
	auto reply = (redisReply*)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
		std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
		freeReplyObject(reply);
		return false;
	}
	std::cout << " Found [ Key " << key << " ] exists ! " << std::endl;
	freeReplyObject(reply);
	return true;
}

void RedisMgr::Close()
{
	_con_pool->Close();
}

RedisMgr::~RedisMgr() {
	Close();
}

RedisConPool::RedisConPool(size_t poolSize, const char* host, int port, const char* pwd)
	: poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {
	for (size_t i = 0; i < poolSize_; ++i) {
		auto* context = redisConnect(host, port);
		if (context == nullptr || context->err != 0) {
			if (context != nullptr) {
				redisFree(context);
			}
			continue;
		}

		auto reply = (redisReply*)redisCommand(context, "AUTH %s", pwd);
		if (reply->type == REDIS_REPLY_ERROR) {
			std::cout << "认证失败" << std::endl;
			//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
			freeReplyObject(reply);
			redisFree(context);
			continue;
		}

		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(reply);
		std::cout << "认证成功" << std::endl;
		connections_.push(context);
	}

}

redisContext* RedisConPool::getConnection() {
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this] {
		if (b_stop_) {
			return true;
		}
		return !connections_.empty();
		});
	//如果停止则直接返回空指针
	if (b_stop_) {
		return  nullptr;
	}
	auto* context = connections_.front();
	connections_.pop();
	return context;
}

void RedisConPool::returnConnection(redisContext* context) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}
	connections_.push(context);
	cond_.notify_one();
}

void RedisConPool::Close() {
	b_stop_ = true;
	cond_.notify_all();
}

RedisConPool::~RedisConPool() {
	std::lock_guard<std::mutex> lock(mutex_);
	//这里应该要释放所有的连接，但是有个问题是在连接池外的连接若没有归还，则不会被释放，这问题待解决。
	while (!connections_.empty()) {
		auto connect = connections_.front();
		connections_.pop();
		if (connect != nullptr) {
			redisFree(connect);
		}
	}
}