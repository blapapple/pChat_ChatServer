#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
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

enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005,		// 用户登录
	MSG_CHAT_LOGIN_RSP = 1006,	// 用户登录返回包
	ID_SEARCH_USER_RSP = 1008,           // 搜索用户回包
	ID_SEARCH_USER_REQ = 1007,           // 搜索用户请求
	ID_ADD_FRIEND_REQ = 1009,            // 添加好友请求
	ID_ADD_FRIEND_RSP = 1010,            // 添加好友回复
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,     // 通知添加好友请求
	ID_AUTH_FRIEND_REQ = 1013,           // 认证好友请求
	ID_AUTH_FRIEND_RSP = 1014,           // 认证好友回复
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015,    // 通知认证好友请求
	ID_TEXT_CHAT_MSG_REQ = 1017,         // 文本聊天消息请求
	ID_TEXT_CHAT_MSG_RSP = 1018,         // 文本聊天消息回复
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019,  // 通知文本聊天消息请求
	ID_NOTIFY_TEXT_CHAT_MSG_RSP = 1020,  // 通知文本聊天消息回复
};

#define CODEPREFIX "code_"
#define USERTOKENPREFIX  "utoken_"
#define USERIPREFIX "uip_"
#define IPCOUNTPREFIX "ipcount_"
#define LOGIN_COUNT "logincount"
#define USER_BASE_INFO "ubaseinfo_"
#define NAME_INFO "nameinfo_"

#define MAX_LENGTH 1024*2
#define HEAD_TOTAL_LEN 4		// 头部总长度
#define HEAD_ID_LEN 2			// 头部id长度
#define HEAD_DATA_LEN 2			// 头部数据长度
#define MAX_RECVQUE 10000		// 最大接收队列长度
#define MAX_SENDQUE 10000		// 最大发送队列长度

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
