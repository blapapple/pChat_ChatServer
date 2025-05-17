#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <chrono>
#include <functional>
#include <map>
#include <unordered_map>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <boost/filesystem.hpp>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <string>
#include <queue>
#include <mutex>
#include <cassert>

#include "Singleton.h"
#include "hiredis.h"


namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,				//Json解析错误
	RPCFailed = 1002,				//RPC调用失败
	VarifyExpired = 1003,			//验证码过期
	VarifyCodeErr = 1004,			//验证码错误
	UserExist = 1005,				//用户已存在
	PasswdErr = 1006,				//密码错误
	EmailNotMatch = 1007,			//邮箱不匹配
	PasswdUpFailed = 1008,			//密码更新失败
	PasswdInvalid = 1009,			//密码不合法
	UidInvalid = 1010,				//用户ID无效
	TokenInvalid = 1011,			//Token无效
};

#define CODEPREFIX "code_"

/**
* Defer类，用于在作用域结束前调用指定的函数
*/
class Defer {
public:
	Defer(std::function<void()> func) : func_(func) {}
	~Defer() { func_(); };
private:
	std::function<void()> func_;
};
