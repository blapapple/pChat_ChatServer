#pragma once
#include "const.h"
#include "data.h"
#include <thread>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/resultset.h>

class SqlConnection {
public:
	SqlConnection(sql::Connection* con, int64_t lasttime) :_con(con), _last_oper_time(lasttime) {}
	std::unique_ptr<sql::Connection> _con;
	int64_t _last_oper_time;
};

class MySqlPool
{
public:
	MySqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize);
	~MySqlPool();
	std::unique_ptr<SqlConnection> GetConnection();
	void ReturnConnection(std::unique_ptr<SqlConnection> con);
	void CheckConnection();
	void Close();

private:
	std::string url_;
	std::string user_;
	std::string password_;
	std::string schema_;
	std::string port_;
	int64_t poolSize_;
	std::atomic<bool> b_stop_;
	std::queue<std::unique_ptr<SqlConnection>> pool_;
	std::condition_variable condv_;
	std::mutex mutex_;
	std::thread check_thread_;
};


class MysqlDao
{
public:
	MysqlDao();
	~MysqlDao();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool AddFriendApply(const int& from, const int& to);
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(std::string name);
private:
	std::unique_ptr<MySqlPool> pool_;
};