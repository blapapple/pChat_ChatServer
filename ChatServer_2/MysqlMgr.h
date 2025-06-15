#pragma once
#include "const.h"
#include "MysqlDao.h"
#include "data.h"
class MysqlMgr : public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
	bool UpdatePwd(const std::string& name, const std::string& pwd);
	bool AddFriendApply(const int& from, const int& to);
	bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit);
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(std::string name);
	bool AuthFriendApply(int fromuid, int touid);
	bool AddFriend(int fromuid, int touid, const std::string& bakname);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list);
private:
	MysqlMgr();
	MysqlDao  _dao;
};