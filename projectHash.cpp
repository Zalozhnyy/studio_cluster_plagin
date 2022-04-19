
#include <QDir>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QDebug>

#include <json/json.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>


#include "projectHash.h"



void ProjectHash::calcProjectHash(QString projectDir) {

    QString file;
    QDir project_dir(projectDir);
    QDirIterator it(projectDir, QDir::Files, QDirIterator::Subdirectories);


    while (it.hasNext()) {
        file = project_dir.relativeFilePath(it.next());

        if (file.endsWith(".DAT")) {
            continue;
        }

        m_filesHash[file] = fileChecksum(file);
    }

}


QByteArray ProjectHash::fileChecksum(const QString& fileName)
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Sha1);
        if (hash.addData(&f)) {
            return hash.result().toHex();
        }
    }
    return QByteArray("warning empty hash").toHex();
}


void ProjectHash::readHashFile() {

    QString key;
    QString value;
    std::ifstream istrm("project_hash.json");
    Json::Value root;

    if (istrm.is_open()) {
        istrm >> root;

        for (auto it = root.begin(); it != root.end(); ++it) {
            key = QString(it.key().asCString());
            value = QString((*it).asCString());

            m_filesHash[key] = value.toUtf8();
        }
    }

}

void ProjectHash::writeHashFile(QString projectDir) {

    std::ofstream ostrm(projectDir.toStdString() + "/project_hash.json");
    Json::Value root;


    if (ostrm.is_open()) {

        std::map<QString, QByteArray>::iterator it = m_filesHash.begin();

        while (it != m_filesHash.end()) {
            root[it->first.toStdString()] = it->second.toStdString();
            it++;
        }

        ostrm << root;
        ostrm.close();
    }

}

void ProjectHash::readHashFromString(std::string& json) {
    Json::Value root;
    QString key;
    QString value;

    std::stringstream j;
    
    j << json;
    j >> root;

    for (auto it = root.begin(); it != root.end(); ++it) {
        key = QString(it.key().asCString());
        value = QString((*it).asCString());

        m_filesHash[key] = value.toUtf8();
    }


}

QList<QString> ProjectHash::compareFiles(std::map<QString, QByteArray>& secondHashFile) {

    QList<QString> output;

    std::map<QString, QByteArray>::iterator it = m_filesHash.begin();

    while (it != m_filesHash.end()) {
        
        if (secondHashFile.count(it->first) && it->second == secondHashFile[it->first]) {
        }

        else {
            output.append(it->first);
        }

        it++;
    }

    return output;
}

QList<QString> ProjectHash::getFilesList() {
    QList<QString> output;

    std::map<QString, QByteArray>::iterator it = m_filesHash.begin();
    while (it != m_filesHash.end()) {
        output.append(it->first);
        it++;
    }
    return output;
}