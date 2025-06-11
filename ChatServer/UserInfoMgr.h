#pragma once
#include "Singleton.h"
#include "data.h"

class UserInfoMgr :
	public Singleton<UserInfoMgr>
{
	friend class Singleton<UserInfoMgr>;
public:
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
};

