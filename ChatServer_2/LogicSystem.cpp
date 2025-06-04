#include "LogicSystem.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusGrpcClient.h"
#include "const.h"
#include "UserMgr.h"

LogicSystem::~LogicSystem()
{
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> msg)
{
	std::unique_lock<std::mutex> unique_lk(_mutex);
	_msg_que.push(msg);
	//��0��Ϊ1����֪ͨ�ź�
	if (_msg_que.size() == 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}

/*
* ע��ص��������½������߳�
*/
LogicSystem::LogicSystem() :_b_stop(false) {
	RegisterCallBacks();
	_worker_thread = std::thread(&LogicSystem::DealMsg, this);
}

void LogicSystem::DealMsg()
{
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//�ж϶���Ϊ�������������������ȴ������ͷ���
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}

		//�ж��Ƿ�Ϊ�ر�״̬���������߼�ִ��������˳�ѭ��
		if (_b_stop) {
			while (!_msg_que.empty()) {
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end()) {
					_msg_que.pop();
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				_msg_que.pop();
			}
			break;
		}

		//���û��ͣ������˵��������������
		auto msg_node = _msg_que.front();
		cout << "recv_msg id is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks()
{
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	std::cout << "user login uid is " << root["uid"].asInt() << " ; user token is " << root["token"].asString() << std::endl;

	Json::Value rtvalue;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, MSG_CHAT_LOGIN_RSP);
		});

	//��redis��ȡ�û�token�Ƿ���ȷ
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	if (token_value != token) {
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid; //�û�������
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	//�����ݿ��ȡ�����б�
	//�����ݿ��ȡ�����б�
	//todo

	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	//����¼��������
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!rd_res.empty()) {
		count = std::stoi(rd_res);
	}
	count++;

	auto count_str = std::to_string(count);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, count_str);

	//session���û�uid
	session->SetUserId(uid);
	//Ϊ�û����õ�¼ip server������
	std::string ipkey = USERIPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(ipkey, server_name);

	//uid��session�󶨹��������Ժ����˲���
	UserMgr::GetInstance()->SetUserSession(uid, session);

	return; //���سɹ�
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const std::string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	std::cout << "search user uid is " << uid_str << std::endl;

	Json::Value rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP);
		});

	bool b_digit = isPureGiget(uid_str);

	if (b_digit) {
		GetUserByUid(uid_str, rtvalue);
	}
	else {
		GetUserByName(uid_str, rtvalue);
	}
}

bool LogicSystem::isPureGiget(const std::string& str)
{
	for (char c : str) {
		if (!isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(const std::string& uid_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;
	std::string base_key = USER_BASE_INFO + uid_str;

	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto pwd = root["pwd"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user login uid is " << uid << " ; user name is " << name
			<< " ; user email is " << email << std::endl;

		rtvalue["uid"] = uid;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["pwd"] = pwd;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = std::stoi(uid_str);
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid; //�û�������
		return;
	}

	//��redis��û�У������ݿ�����д��redis���棬�����´β�ѯ
	//�����ݿ�����д��redis����
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["name"] = user_info->name;
	redis_root["pwd"] = user_info->pwd;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;
	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	rtvalue["uid"] = user_info->uid;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;

}

void LogicSystem::GetUserByName(const std::string& name_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;
	std::string base_key = NAME_INFO + name_str;

	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto pwd = root["pwd"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user login uid is " << uid << " ; user name is " << name
			<< " ; user email is " << email << std::endl;

		rtvalue["uid"] = uid;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["pwd"] = pwd;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name_str);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid; //�û�������
		return;
	}

	//��redis��û�У������ݿ�����д��redis���棬�����´β�ѯ
	//�����ݿ�����д��redis����
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["name"] = user_info->name;
	redis_root["pwd"] = user_info->pwd;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;
	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//���÷�������
	rtvalue["uid"] = user_info->uid;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	//���Ȳ�ѯRedis
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is " << userinfo->uid << " ; user name is " << userinfo->name
			<< " ; user email is " << userinfo->email << std::endl;
	}
	else {
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			return false; //�û�������
		}
		userinfo = user_info;

		//�����ݿ�����д��redis����
		Json::Value redis_root;
		redis_root["uid"] = user_info->uid;
		redis_root["name"] = user_info->name;
		redis_root["pwd"] = user_info->pwd;
		redis_root["email"] = user_info->email;
		redis_root["nick"] = user_info->nick;
		redis_root["desc"] = user_info->desc;
		redis_root["sex"] = user_info->sex;
		redis_root["icon"] = user_info->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}
}

