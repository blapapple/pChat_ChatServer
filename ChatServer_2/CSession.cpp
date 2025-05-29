#include "CSession.h"

CSession::CSession(boost::asio::io_context& io_context, CServer* server) :
	_socket(io_context), _server(server), _b_close(false), _b_head_parse(false), _user_uid(0) {
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

CSession::~CSession() {
	std::cout << "~CSession destruct" << endl;
}

void CSession::Close()
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	_socket.close();
	_b_close = true;
}


void CSession::AsyncReadBody(int total_len)
{
	auto self = shared_from_this();
	AsyncReadFull(total_len, [self, total_len](const boost::system::error_code& ec, size_t bytes_transfered) {
		try
		{
			if (ec) {
				std::cout << "handle read failed, error is " << ec.what() << std::endl;
				self->Close();
				self->_server->ClearSession(self->_uuid);
				return;
			}

			if (bytes_transfered < total_len) {
				std::cout << "read length not match, read [" << bytes_transfered << "], total [" << total_len << "]" << std::endl;
				self->Close();
				self->_server->ClearSession(self->_uuid);
				return;
			}

			memcpy(self->_recv_msg_node->_data, self->_data, bytes_transfered);
			self->_recv_msg_node->_cur_len += bytes_transfered;
			self->_recv_msg_node->_data[self->_recv_msg_node->_total_len] = '\0';
			std::cout << "receive data is" << self->_recv_msg_node->_data << std::endl;

			//此处将消息投递到逻辑队列中
			LogicSystem::GetInstance()->PostMsgToQue(std::make_shared<LogicNode>(self->shared_from_this(), self->_recv_msg_node));
			//继续监听头部接受事件
			self->AsyncReadHead(HEAD_TOTAL_LEN);
		}
		catch (const std::exception& e)
		{
			std::cout << "Exception code is" << e.what() << std::endl;
		}
		});
}

void CSession::Start()
{
	AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::AsyncReadFull(size_t maxLength, std::function<void(const boost::system::error_code&, size_t)> handler)
{
	::memset(_data, 0, MAX_LENGTH);
	AsyncReadLen(0, maxLength, handler);
}

void CSession::AsyncReadLen(size_t read_len, size_t total_len, std::function<void(const boost::system::error_code&, size_t)> handler)
{
	auto self = shared_from_this();
	//经过total_len - read_len可以确保最多只读取到初始传入的长度，比如头部或主消息体
	_socket.async_read_some(net::buffer(_data + read_len, total_len - read_len),
		[read_len, total_len, handler, self](const boost::system::error_code& ec, size_t bytesTransered) {
			if (ec) {	//出现错误调用回调
				handler(ec, read_len + bytesTransered);
				return;
			}
			if (read_len + bytesTransered >= total_len) {	//长度够了调用回调
				handler(ec, read_len + bytesTransered);
				return;
			}

			self->AsyncReadLen(read_len + bytesTransered, total_len, handler);	//继续读取
		});

}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> share_self)
{
	try {
		auto self = shared_from_this();
		if (!error) {
			std::lock_guard<std::mutex> lock(_send_lock);
			//cout << "send data " << _send_que.front()->_data+HEAD_LENGTH << endl;
			_send_que.pop();
			if (!_send_que.empty()) {
				auto& msgnode = _send_que.front();
				boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
					std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_from_this()));
			}
		}
		else {
			std::cout << "handle write failed, error is " << error.what() << endl;
			Close();
			DealExceptionSession();
		}
	}
	catch (std::exception& e) {
		std::cerr << "Exception code : " << e.what() << endl;
	}
}


void CSession::SetUserId(int uid)
{
	_user_uid = uid;
}

int CSession::GetUserId()
{
	return _user_uid;
}

void CSession::Send(char* msg, short max_length, short msgid)
{
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _session_id << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg, max_length, msgid));
	if (send_que_size > 0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_from_this()));
}

void CSession::Send(std::string msg, short msgid)
{
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _session_id << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
	if (send_que_size > 0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_from_this()));
}

void CSession::AsyncReadHead(int total_len)
{
	auto self = shared_from_this();
	AsyncReadFull(HEAD_TOTAL_LEN, [self](const boost::system::error_code& ec, size_t bytes_transfered) {
		try
		{
			if (ec) {
				std::cout << "handle read failed, error is " << ec.what() << std::endl;
				self->Close();
				self->_server->ClearSession(self->_uuid);
				return;
			}

			//经过ReadLen里面的处理后，这里应该能确保长度会大于HEAD_TOTAL_LEN了，这里相当于双重校验了
			if (bytes_transfered < HEAD_TOTAL_LEN) {
				std::cout << "read length not match, read [" << bytes_transfered << "], total ["
					<< HEAD_TOTAL_LEN << "]" << std::endl;
				self->Close();
				self->_server->ClearSession(self->_uuid);
				return;
			}

			self->_recv_head_node->Clear();
			memcpy(self->_recv_head_node->_data, self->_data, bytes_transfered);
			//获取头部MSGID数据
			short msg_id = 0;
			memcpy(&msg_id, self->_recv_head_node->_data, HEAD_ID_LEN);
			//网络字节序转化为本地字节序
			msg_id = net::detail::socket_ops::host_to_network_short(msg_id);
			std::cout << "msg_id is " << msg_id << std::endl;

			if (msg_id > MAX_LENGTH) { //id非法
				std::cout << "invalid msg_id is" << msg_id << std::endl;
				self->_server->ClearSession(self->_uuid);
				return;
			}
			short msg_len = 0;
			//刚好读完头部数据
			memcpy(&msg_len, self->_recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
			msg_len = net::detail::socket_ops::network_to_host_short(msg_len);
			std::cout << "msg_len is " << msg_len << std::endl;

			if (msg_len > MAX_LENGTH) {
				std::cout << "invalid data length is " << msg_len << std::endl;
				self->_server->ClearSession(self->_uuid);
				return;
			}

			self->_recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);
			self->AsyncReadBody(msg_len);

		}
		catch (const std::exception& e)
		{
			std::cout << "Exception code is" << e.what() << std::endl;
		}
		});
}

