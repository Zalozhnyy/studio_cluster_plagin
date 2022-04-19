#ifndef COMMAND_BUILDER
#define COMMAND_BUILDER

#include "cluster_data.h"


class CommandBuilder {

public:

	void setData(std::shared_ptr<SshData::Connection> connection_data) {
		m_data = connection_data;
	};

	std::string getExecutableCommand();
	std::string getResultsCommand();
	std::string getBuildCommand(std::string& faradayName);
	std::string getDefaultDir();
	std::string copyExecCommand();
	std::string cleanRestorePoints();
	std::string renameFile(std::string oldName, std::string newName);


private:
	std::shared_ptr<SshData::Connection> m_data;

	std::string getSourceEnvironment();
	std::string changeDir();
	std::string getCommand();
};




#endif // COMMAND_BUILDER
