#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "enums.h"
#include "gpconfig.h"
#include "storagemanager.h"

class ConfigManager {
public:
	ConfigManager(ConfigManager const&) = delete;
	void operator=(ConfigManager const&)  = delete;
    static ConfigManager& getInstance() {
		static ConfigManager instance;
		return instance;
	}
    void setup(ConfigType);
    void loop();
private:
    ConfigManager() {}
    void setupConfig(GPConfig*);
    ConfigType cType;
    GPConfig * config;
};

#endif
