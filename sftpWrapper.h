#ifndef SFTP_WRAPPER_H
#define SFTP_WRAPPER_H



#include "sshClient.h"
#include "cluster_data.h"


class SFTP_Wrapper: public SshClient
{
public:
	SFTP_Wrapper::SFTP_Wrapper(std::shared_ptr<SshData::Connection> data) :
		SshClient(data, nullptr) {
	
		initSession();
	};


	std::tuple<int, QList<QString>> getListDirectory(std::string workingDirectory);
	std::tuple<int, QSet<QString>> getHashSetDirectory(std::string workingDirectory);

	std::map<std::string, bool> checkFilesInFolder(std::string workingDirectory, std::vector<std::string>& files);
	int createDirectory(std::string dir);
	std::tuple<int, bool> checkFolders();
	std::tuple<int, std::string> sftp_read_sync(std::string file_path);
	int sftpDownloadBinaryFile(std::string sFile, std::string localFile);

	bool isDirExist(std::string serverDirectory);
	bool createRelatedDir(std::string serverDirectory);

private:
	

};

#endif //SFTP_WRAPPER_H