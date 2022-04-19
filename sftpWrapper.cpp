#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>



#include "sftpWrapper.h"

#define MAX_XFER_BUF_SIZE 16384


std::tuple<int, QList<QString>> SFTP_Wrapper::getListDirectory(std::string workingDirectory) {

    sftp_session sftp;
    sftp_dir dir;
    sftp_attributes attributes;
    QList<QString> dirs;
    int rc;

    sftp = sftp_new(session);
    rc = sftp_init(sftp);

    dir = sftp_opendir(sftp, workingDirectory.c_str());
    if (!dir)
    {
        emit sendMessage(QString("Directory not opened: %1\n").arg(QString(ssh_get_error(session))));
        return std::make_tuple(SSH_ERROR, dirs);
    }

    while ((attributes = sftp_readdir(sftp, dir)) != NULL)
    {
        dirs.append(QString(attributes->name));
        sftp_attributes_free(attributes);
    }

    if (!sftp_dir_eof(dir))
    {
        emit sendMessage(QString("Can't list directory: %1\n").arg(QString(ssh_get_error(session))));
        sftp_closedir(dir);
        return std::make_tuple(SSH_ERROR, dirs);
    }

    rc = sftp_closedir(dir);


    sftp_free(sftp);
    return std::make_tuple(SSH_OK, dirs);
}


std::tuple<int, QSet<QString>> SFTP_Wrapper::getHashSetDirectory(std::string workingDirectory) {

    sftp_session sftp;
    sftp_dir dir;
    sftp_attributes attributes;
    QSet<QString> dirs;
    int rc;

    sftp = sftp_new(session);
    rc = sftp_init(sftp);

    dir = sftp_opendir(sftp, workingDirectory.c_str());
    if (!dir)
    {
        emit sendMessage(QString("Directory not opened: %1\n").arg(QString(ssh_get_error(session))));
        return std::make_tuple(SSH_ERROR, dirs);
    }

    while ((attributes = sftp_readdir(sftp, dir)) != NULL)
    {
        dirs.insert(QString(attributes->name));
        sftp_attributes_free(attributes);
    }

    if (!sftp_dir_eof(dir))
    {
        emit sendMessage(QString("Can't list directory: %1\n").arg(QString(ssh_get_error(session))));
        sftp_closedir(dir);
        return std::make_tuple(SSH_ERROR, dirs);
    }

    rc = sftp_closedir(dir);


    sftp_free(sftp);
    return std::make_tuple(SSH_OK, dirs);
}


std::map<std::string, bool> SFTP_Wrapper::checkFilesInFolder(std::string workingDirectory, std::vector<std::string>& files) {

    auto res = getListDirectory(workingDirectory);
    int exit_code = std::get<0>(res);
    QList<QString> files_in_dir = std::get<1>(res);
    std::map<std::string, bool> output;

    for (auto& f : files) {
        output[f] = false;
    }

    for (auto& f : files_in_dir) {
        if (output.count(f.toStdString()) != 0) {
            output[f.toStdString()] = true;
        }
    }

    return output;
}


int SFTP_Wrapper::createDirectory(std::string dir) {
    sftp_session sftp;
    int rc;

    sftp = sftp_new(session);
    rc = sftp_init(sftp);
    rc = sftp_mkdir(sftp, dir.c_str(), S_IREAD | S_IWRITE | S_IEXEC);
    sftp_free(sftp);

    return rc;
}


std::tuple<int, bool> SFTP_Wrapper::checkFolders() {

    std::tuple<int, QList<QString>> res;
    int exit_code;
    QList<QString> dirs;
    std::string checkedDirectory = m_data->baseServerDir;

    auto searchDirs = QString::fromStdString(m_data->extendedDir).split("/");
    for (int i = 0; i < searchDirs.size(); i++) {

        res = getListDirectory(checkedDirectory);
        exit_code = std::get<int>(res);
        dirs = std::get<QList<QString>>(res);

        if (exit_code != SSH_OK) {
            sendProgressMessage(false,
                QString("error while check dir %s").arg(QString::fromStdString(checkedDirectory)));
            return std::make_tuple(exit_code, false);
        }

        bool a = std::find(dirs.begin(), dirs.end(), searchDirs[i]) != dirs.end();
        checkedDirectory += "/" + searchDirs[i].toStdString();

        if (!a) { //not exist
            createDirectory(checkedDirectory);
        }
        else if (a && i == searchDirs.size() - 1) { //found folder with project name
            return std::make_tuple(exit_code, true);
        }
        else { //exist
            continue;
        }
    }

    return std::make_tuple(exit_code, false);
}

std::tuple<int, std::string> SFTP_Wrapper::sftp_read_sync(std::string file_path)
{
    sftp_session sftp;
    int access_type;
    sftp_file file;
    char buffer[MAX_XFER_BUF_SIZE];
    int nbytes, nwritten, rc;
    std::stringstream ss;


    sftp = sftp_new(session);
    rc = sftp_init(sftp);


    access_type = O_RDONLY;
    file = sftp_open(sftp, file_path.c_str(), access_type, 0);
    if (file == NULL) {
        fprintf(stderr, "Can't open file for reading: %s\n",
            ssh_get_error(session));
        return std::make_tuple(SSH_ERROR, "");
    }

    while (true)
    {
        nbytes = sftp_read(file, buffer, sizeof(buffer));

        if (nbytes == 0) {
            break; // EOF
        }

        else if (nbytes < 0) {
            fprintf(stderr, "Error while reading file: %s\n",
                ssh_get_error(session));
            sftp_close(file);
            return std::make_tuple(SSH_ERROR, nullptr);
        }

        ss << std::string(QByteArray(buffer, nbytes));
    }

    rc = sftp_close(file);

    return std::make_tuple(SSH_OK, std::string(ss.str()));
}


int SFTP_Wrapper::sftpDownloadBinaryFile(std::string sFile, std::string localFile) {

    sftp_session sftp;
    int access_type;
    sftp_file file;
    char* buffer[MAX_XFER_BUF_SIZE];
    int nbytes, rc;


    sftp = sftp_new(session);
    rc = sftp_init(sftp);


    access_type = O_BINARY;
    file = sftp_open(sftp, sFile.c_str(), access_type, 0);
    if (file == NULL) {
        fprintf(stderr, "Can't open file for reading: %s\n",
            ssh_get_error(session));
        return SSH_ERROR;
    }

    std::ofstream ostrm;
    ostrm.open(localFile, std::ios_base::binary | std::fstream::trunc | std::fstream::out);

    if (!ostrm.is_open()) {
        qDebug() << "local file not opened";
        return SSH_ERROR;
    }

    while (true)
    {
        nbytes = sftp_read(file, buffer, sizeof(buffer));

        if (nbytes == 0) {
            break; // EOF
        }

        else if (nbytes < 0) {
            fprintf(stderr, "Error while reading file: %s\n",
                ssh_get_error(session));
            sftp_close(file);
            return SSH_ERROR;
        }

        ostrm.write((const char*)buffer, nbytes);
    }


    rc = sftp_close(file);
    ostrm.close();

    return SSH_OK;
}


bool SFTP_Wrapper::isDirExist(std::string serverDirectory) {
    std::tuple<int, QSet<QString>> res;
    
    auto searchDir = QDir(serverDirectory.c_str()).dirName();

    res = getHashSetDirectory(serverDirectory + "/..");
    auto exit_code = std::get<int>(res);
    auto dirs = std::get<QSet<QString>>(res);

    if (exit_code == SSH_OK) {
        if (dirs.contains(searchDir)) {
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }

}

bool SFTP_Wrapper::createRelatedDir(std::string serverDirectory) {
    std::tuple<int, QSet<QString>> res;
    
    QStringList searchDirs = QString(serverDirectory.c_str()).split('/');
    QStringList currentDir;
    QStringList prevDir;
    int exit_code;
    
    prevDir << "/";

    for (auto& d : searchDirs) {

        currentDir << d;

        if (d == "") continue;

        res = getHashSetDirectory(prevDir.join("/").toStdString());
        exit_code = std::get<int>(res);
        auto dirs = std::get<QSet<QString>>(res);

        if (exit_code != SSH_OK) return false;
        
        if (!dirs.contains(d)) {
            exit_code = createDirectory(currentDir.join("/").toStdString());
            if (exit_code != SSH_OK) {
                emit sendMessage(QString("Cant create %1 dir. Error: %2").arg(currentDir.join("/")).arg(QString(ssh_get_error(session))));
                return false;
            }
        }

        prevDir << d;

    }

    return true;
}
