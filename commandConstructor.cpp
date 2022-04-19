
#include <sstream>
#include <string>

#include "commandConstructor.h"


using namespace std;

string CommandBuilder::getSourceEnvironment() {
	 stringstream ss;

	 ss << m_data->sourceEnvironmet << " && ";
	 
	 return ss.str();
}

string CommandBuilder::changeDir() {
	 stringstream ss;
	 ss << "cd " << m_data->workingDirectory << " && ";
	 return ss.str();
 }

string CommandBuilder::getDefaultDir() {
	stringstream ss;
	ss << m_data->baseServerDir << '/' << m_data->default_folder_name;
	return ss.str();
}

 string CommandBuilder::getCommand() {
	 return m_data->command;
 }

string CommandBuilder::getExecutableCommand() {
	 stringstream ss;

	 ss << getSourceEnvironment();
	 if (m_data->autoChangeDir) {
		 ss << changeDir();
	 }
	 ss << getCommand();
	 return ss.str();
 }

string CommandBuilder::getResultsCommand() {
	stringstream ss;

	ss << changeDir() << "./splicer.ex && ./jointer.ex";
	return ss.str();
 }

string CommandBuilder::getBuildCommand(std::string& faradayName) {
	stringstream ss;
	
	ss << getSourceEnvironment() << "cd " << getDefaultDir() << '/' << faradayName << " && ./buildFaraday.sh";
	return ss.str();

 }

string CommandBuilder::copyExecCommand() {
	stringstream ss;
	
	ss << "cp " << getDefaultDir() << "*.ex " << m_data->workingDirectory;

	return ss.str();
}

string CommandBuilder::cleanRestorePoints() {
	stringstream ss;
	
	ss << "rm " << m_data->workingDirectory << "/restore_points/*";

	return ss.str();
}

string CommandBuilder::renameFile(std::string oldName ,std::string newName) {
	stringstream ss;
	
	ss << "mv " << m_data->workingDirectory << "/" << oldName << " " 
		<< m_data->workingDirectory << "/" << newName;

	return ss.str();
}



