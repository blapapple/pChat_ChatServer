#include "AsioIOServicePool.h"
#include <iostream>
using namespace std;
AsioIOServicePool::AsioIOServicePool(std::size_t size) :_ioServices(size),
_workGuards(size), _nextIOService(0) {
	// 创建多个 io_service 对象，每个对象对应一个线程，用于处理异步操作，使用Workguard来保持 io_service 的生命周期，否则无信息会导致 io_service 被销毁
	for (std::size_t i = 0; i < size; ++i) {
		_workGuards[i] = std::make_unique<WorkGuard>(boost::asio::make_work_guard(_ioServices[i]));
	}

	//遍历多个ioservice，创建多个线程，每个线程内部启动ioservice
	for (std::size_t i = 0; i < _ioServices.size(); ++i) {
		_threads.emplace_back([this, i]() {
			_ioServices[i].run();
			});
	}
}

AsioIOServicePool::~AsioIOServicePool() {
	Stop();//这里在asio tcp服务器这一章节并没有使用，后面看情况改
	std::cout << "AsioIOServicePool destruct" << endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
	auto& service = _ioServices[_nextIOService++];
	if (_nextIOService == _ioServices.size()) {
		_nextIOService = 0;
	}
	return service;
}

void AsioIOServicePool::Stop() {
	// 释放 work guards
	for (auto& guard : _workGuards) {
		guard->get_executor().context().stop();
		guard.reset();
	}

	// 等待所有线程完成
	for (auto& t : _threads) {
		if (t.joinable()) {
			t.join();
		}
	}

	// 清空容器
	_threads.clear();
	_workGuards.clear();
}