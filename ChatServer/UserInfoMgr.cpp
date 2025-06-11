#include "UserInfoMgr.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"

bool UserInfoMgr::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	//优先查询Redis
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
			return false; //用户不存在
		}
		userinfo = user_info;

		//将数据库内容写入redis缓存
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