#pragma once
#include "const.h"
struct SectionInfo {
	SectionInfo() {}
	~SectionInfo() { _section_datas.clear(); }
	std::map<std::string, std::string> _section_datas;
	std::string operator[](const std::string& key) {
		auto it = _section_datas.find(key);
		if (it != _section_datas.end()) {
			return it->second;
		}
		return "";
	}

	SectionInfo(const SectionInfo& src) {
		_section_datas = src._section_datas;
	}

	SectionInfo& operator = (const SectionInfo& src) {
		if (this != &src) {
			_section_datas = src._section_datas;
		}
		return *this;
	}
	std::string GetValue(const std::string& key) {
		auto it = _section_datas.find(key);
		if (it != _section_datas.end()) {
			return it->second;
		}
		return "";
	}
};

class ConfigMgr
{
public:
	~ConfigMgr() {
		_config_map.clear();
	}

	SectionInfo operator[](const std::string& section) {
		auto it = _config_map.find(section);
		if (it != _config_map.end()) {
			return it->second;
		}
		return SectionInfo();
	}

	ConfigMgr& operator = (const ConfigMgr& src) {
		if (this != &src) {
			_config_map = src._config_map;
		}
		return *this;
	}

	ConfigMgr(const ConfigMgr& src) {
		_config_map = src._config_map;
	}

	static ConfigMgr& Inst() {
		static ConfigMgr config_mgr;
		return config_mgr;
	}

	std::string GetValue(const std::string& section, const std::string& key);
private:
	ConfigMgr();
	std::map<std::string, SectionInfo> _config_map;
};

