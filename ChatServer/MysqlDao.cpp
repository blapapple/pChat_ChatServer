#include "MysqlDao.h"
#include "ConfigMgr.h"

MySqlPool::MySqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize) : url_(url), user_(user), password_(pass), schema_(schema), poolSize_(poolSize), b_stop_(false)
{
	try {
		for (int i = 0; i < poolSize_; ++i) {
			sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
			auto* con = driver->connect(url_, user_, password_);
			con->setSchema(schema_);
			auto currentTime = std::chrono::system_clock::now().time_since_epoch();
			long long timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime).count();
			pool_.emplace(std::make_unique<SqlConnection>(con, timeStamp));
		}

		check_thread_ = std::thread([this]() {
			while (!b_stop_) {
				CheckConnection();
				std::this_thread::sleep_for(std::chrono::seconds(60));
			}
			});
		check_thread_.detach();
	}
	catch (sql::SQLException& e) {
		std::cout << "mysql pool init failed, error is " << e.what() << std::endl;
	}
}

/**
* 检查连接池中的连接是否可用
*/
void MySqlPool::CheckConnection() {
	std::lock_guard<std::mutex> guard(mutex_);
	int poolSize = pool_.size();

	auto currentTime = std::chrono::system_clock::now().time_since_epoch();
	long long timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime).count();
	for (int i = 0; i < poolSize; ++i) {
		auto con = std::move(pool_.front());
		pool_.pop();
		Defer defer([this, &con]() {
			pool_.emplace(std::move(con));
			});

		if (timeStamp - con->_last_oper_time < 5) {
			continue;
		}

		try {
			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			stmt->executeQuery("SELECT 1");
			con->_last_oper_time = timeStamp;
			//std::cout << "execute timer alive query , cur is " << timeStamp << std::endl;
		}
		catch (sql::SQLException& e) {
			std::cout << "Error keeping connection alive " << e.what() << std::endl;
			//重新创建连接替换旧的连接
			sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
			auto* new_con = driver->connect(url_, user_, password_);
			new_con->setSchema(schema_);
			con->_con.reset(new_con);
			con->_last_oper_time = timeStamp;
		}
	}
}

std::unique_ptr<SqlConnection> MySqlPool::GetConnection() {
	std::unique_lock<std::mutex> lock(mutex_);
	condv_.wait(lock, [this]() {
		if (b_stop_) {
			return true;
		}
		return !pool_.empty();
		});
	if (b_stop_) {
		return nullptr;
	}
	std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
	pool_.pop();
	return con;
}

void MySqlPool::Close() {
	b_stop_ = true;
	condv_.notify_all();
}

void MySqlPool::ReturnConnection(std::unique_ptr<SqlConnection> con) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (b_stop_) {
		return;
	}
	pool_.emplace(std::move(con));
	condv_.notify_one();
}

MySqlPool::~MySqlPool() {
	std::unique_lock<std::mutex> lock(mutex_);
	while (!pool_.empty()) {
		pool_.pop();
	}
}



MysqlDao::MysqlDao() {
	auto& cfg = ConfigMgr::Inst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& user = cfg["Mysql"]["User"];
	const auto& password = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& port = cfg["Mysql"]["Port"];
	pool_.reset(new MySqlPool(host + ":" + port, user, password, schema, 5));
}

MysqlDao::~MysqlDao() {
	if (pool_) {
		pool_->Close();
	}
}

int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	auto con = pool_->GetConnection();
	try {
		if (con == nullptr) {
			return false;
		}
		// 准备调用存储过程
		std::unique_ptr<sql::PreparedStatement> stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
		// 设置输入参数
		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);

		// 由于PreparedStatement不直接支持注册输出参数，我们需要使用会话变量或其他方法来获取输出参数的值

		// 执行存储过程
		stmt->execute();
		// 如果存储过程设置了会话变量或有其他方式获取输出参数的值，可以在这里执行SELECT查询来获取它们
		std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
		if (res->next()) {
			int result = res->getInt("result");
			std::cout << "Result: " << result << std::endl;
			pool_->ReturnConnection(std::move(con));
			return result;
		}
		pool_->ReturnConnection(std::move(con));
		return -1;
	}
	catch (sql::SQLException& e) {
		pool_->ReturnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return -1;
	}
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email)
{
	auto con = pool_->GetConnection();
	try
	{
		if (con == nullptr) {
			return false;
		}

		//从数据库中查询用户的邮件地址
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
		pstmt->setString(1, name);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		while (res->next()) {
			std::cout << "Check Email: " << res->getString("email") << std::endl;
			if (email != res->getString("email")) {
				pool_->ReturnConnection(std::move(con));
				return false;
			}
			pool_->ReturnConnection(std::move(con));
			return true;
		}
		return true;

	}
	catch (const sql::SQLException& e)
	{
		pool_->ReturnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd)
{
	auto con = pool_->GetConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

		// 绑定参数
		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);

		// 执行更新
		int updateCount = pstmt->executeUpdate();

		std::cout << "Updated rows: " << updateCount << std::endl;
		pool_->ReturnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->ReturnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo) {
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->ReturnConnection(std::move(con));
		});

	try
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE email = ? AND pwd = ?"));
		pstmt->setString(1, email);
		pstmt->setString(2, pwd);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		if (res->next()) {
			userInfo.name = res->getString("name");
			userInfo.email = res->getString("email");
			userInfo.pwd = res->getString("pwd");
			userInfo.uid = res->getInt("uid");
			std::cout << "Check Pwd: " << userInfo.email << " : " << userInfo.pwd << std::endl;
			return true;
		}
		else {
			return false;
		}

	}
	catch (sql::SQLException& e)
	{
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->icon = res->getString("icon");
			user_ptr->uid = uid;
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(std::string name)
{
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->uid = res->getInt("uid");
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

bool MysqlDao::AddFriendApply(const int& from, const int& to) {
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([&con, this]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT INTO friend_apply (from_uid, to_uid) values (?, ?) ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid"));	   // 准备插入语句
		pstmt->setInt(1, from);
		pstmt->setInt(2, to);
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}

		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit) {
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([&con, this]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select apply.from_uid, apply.status, user.name, user.nick, user.sex from friend_apply as apply join user on apply.from_uid = user.uid where apply.to_uid = ? and apply.id > ? order by apply.id ASC LIMIT ?"));

		pstmt->setInt(1, touid);	// 查询申请指向自己的item
		pstmt->setInt(2, begin);	// 查询起始id
		pstmt->setInt(3, limit);	// 偏移量

		//执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next()) {
			auto name = res->getString("name");
			auto uid = res->getInt("from_uid");
			auto status = res->getInt("status");
			auto nick = res->getString("nick");
			auto sex = res->getInt("sex");
			auto apply_ptr = std::make_shared<ApplyInfo>(uid, name, "", "", nick, sex, status);
			applyList.emplace_back(apply_ptr);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::AuthFriendApply(const int& from, const int& to)
{
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([&con, this]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE friend_apply SET status = 1 WHERE from_uid = ? AND to_uid = ?"));

		// 这里是认证，认证的from是申请的to，这里是反过来的
		pstmt->setInt(1, to);
		pstmt->setInt(2, from);
		// 执行更新
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected <= 0) {
			return false;
		}

		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::AddFriend(const int& from, const int& to, std::string back_name)
{
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([&con, this]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		//开始事务
		con->_con->setAutoCommit(false);

		//	准备第一个sql语句，插入认证方好友数据
		std::unique_ptr<sql::PreparedStatement> pstmt1(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) VALUES(?,?,?)"));
		//	认证的from是申请的to，这里是反过来的
		pstmt1->setInt(1, from);
		pstmt1->setInt(2, to);
		pstmt1->setString(3, back_name);
		//	执行更新
		int rowAffected = pstmt1->executeUpdate();
		if (rowAffected < 0) {
			con->_con->rollback();
			return false;
		}

		//	插入申请方好友数据
		std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) VALUES(?,?,?)"));
		pstmt2->setInt(1, to);
		pstmt2->setInt(2, from);
		pstmt2->setString(3, "");
		//	执行更新
		int rowAffected2 = pstmt2->executeUpdate();
		if (rowAffected2 < 0) {
			con->_con->rollback();
			return false;
		}

		//	提交事务
		con->_con->commit();
		std::cout << "addfriend insert friends success " << std::endl;
		return true;

	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	auto con = pool_->GetConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->ReturnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select * from friend where self_id = ?"));
		pstmt->setInt(1, self_id);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next()) {
			auto friend_id = res->getInt("friend_id");
			auto back = res->getString("back");
			//再一次查询friend_id对应消息
			auto user_info = GetUser(friend_id);
			if (user_info == nullptr) {
				continue;
			}
			user_info->back = user_info->name;
			user_list.emplace_back(user_info);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}