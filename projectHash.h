#ifndef PROJECT_HASH_H
#define PROJECT_HASH_H


#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>



class ProjectHash : public QObject {

public:

	std::map<QString, QByteArray> m_filesHash;

	void calcProjectHash(QString projectDir);
	void readHashFile();
	void readHashFromString(std::string& json);

	void writeHashFile(QString projectDir);



	QList<QString> compareFiles(std::map<QString, QByteArray>& secondHashFile);
	QList<QString> ProjectHash::getFilesList();

private:

	
	QByteArray fileChecksum(const QString& fileName);

};



#endif