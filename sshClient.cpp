
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "sshClient.h"
#include "sftpWrapper.h"
#include "plugin_cluster_data.h"
#include "timer.h"

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>
#include <functional>
#include <tuple>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>



#include <QDebug>
#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSet>



SshClient::~SshClient() {
    if (session != nullptr) {
        finilizeSession();
    }
}

bool SshClient::initSession() {

    int rc;
    const int log_level = SSH_LOG_PROTOCOL;
    session = ssh_new();

    if (session == NULL) {
        finilizeSession();
        return false;
    }

    ssh_options_set(session, SSH_OPTIONS_HOST, m_data->host.c_str());
    ssh_options_set(session, SSH_OPTIONS_USER, m_data->username.c_str());
    //ssh_options_set(session, SSH_OPTIONS_PORT_STR, "22");
    //ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &log_level);

    emit sendProgressMessage(false, QString("Connecting to server..."));
    rc = ssh_connect(session);
    if (rc != SSH_OK)
    {
        emit sendMessage(QString(
            "Error connecting to %1: %2\n"
        ).arg(QString(m_data->host.c_str())).arg(QString(ssh_get_error(session))));


        finilizeSession();
        return false;
    }

    if (verify_knownhost() < 0)
    {
        finilizeSession();
        return false;
    }

    rc = authenticate_password();

    if (rc != SSH_AUTH_SUCCESS) {
        rc = authenticate_kbdint();
    }

    if (rc != SSH_AUTH_SUCCESS)
    {
        emit sendMessage(QString(
            "Error authenticating with password: %1\n"
        ).arg(QString(ssh_get_error(session))));
        finilizeSession();
        return false;
    }

    connected = true;
    return true;
}

int SshClient::authenticate_kbdint()
{
    int rc;
    rc = ssh_userauth_kbdint(session, NULL, NULL);
    
    while (rc == SSH_AUTH_INFO)
    {
        const char* name, * instruction;
        int nprompts, iprompt;
        name = ssh_userauth_kbdint_getname(session);
        instruction = ssh_userauth_kbdint_getinstruction(session);
        nprompts = ssh_userauth_kbdint_getnprompts(session);

        for (iprompt = 0; iprompt < nprompts; iprompt++)
        {
            const char* prompt;
            char echo;
            prompt = ssh_userauth_kbdint_getprompt(session, iprompt, &echo);
            if (echo)
            {
                char buffer[128], * ptr;
                emit sendMessage(QString("%1").arg(QString(prompt)));
                if (fgets(buffer, sizeof(buffer), stdin) == NULL)
                    return SSH_AUTH_ERROR;
                buffer[sizeof(buffer) - 1] = '\0';
                if ((ptr = strchr(buffer, '\n')) != NULL)
                    *ptr = '\0';
                if (ssh_userauth_kbdint_setanswer(session, iprompt, buffer) < 0)
                    return SSH_AUTH_ERROR;
                memset(buffer, 0, strlen(buffer));
            }
            else
            {
                if (ssh_userauth_kbdint_setanswer(session, iprompt, m_data->password.c_str()) < 0)
                    return SSH_AUTH_ERROR;
            }
        }
        rc = ssh_userauth_kbdint(session, NULL, NULL);
    }
    return rc;
}

int SshClient::authenticate_pubkey()
{
    int rc;

    rc = ssh_userauth_publickey_auto(session, NULL, NULL);

    if (rc == SSH_AUTH_ERROR)
    {
        emit sendMessage(QString(
            "Error while pub key authentication: %1\n"
        ).arg(QString(ssh_get_error(session))));

        return SSH_AUTH_ERROR;
    }

    return rc;
}

int SshClient::authenticate_password()
{
    int rc;

    rc = ssh_userauth_password(session, NULL, m_data->password.c_str());
    if (rc == SSH_AUTH_ERROR)
    {
        emit sendMessage(QString(
            "Error while password authentication: %1\n"
        ).arg(QString(ssh_get_error(session))));

        return SSH_AUTH_ERROR;
    }

    return rc;
}

int SshClient::verify_knownhost()
{
    enum ssh_known_hosts_e state;
    unsigned char* hash = NULL;
    ssh_key srv_pubkey = NULL;
    size_t hlen;
    char* hexa;
    int rc;

    rc = ssh_get_server_publickey(session, &srv_pubkey);
    if (rc < 0) {
        return -1;
    }

    rc = ssh_get_publickey_hash(srv_pubkey,
        SSH_PUBLICKEY_HASH_SHA1,
        &hash,
        &hlen);
    ssh_key_free(srv_pubkey);
    if (rc < 0) {
        return -1;
    }

    state = ssh_session_is_known_server(session);
    switch (state) {
    case SSH_KNOWN_HOSTS_OK:
        /* OK */

        break;

    case SSH_KNOWN_HOSTS_CHANGED:
        //fprintf(stderr, "Host key for server changed: it is now:\n");
        ssh_print_hexa("Public key hash", hash, hlen);
        //fprintf(stderr, "For security reasons, connection will be stopped\n");
        emit sendMessage(QString(
            "Host key for server changed\n"
            "For security reasons, connection will be stopped\n"));
        ssh_clean_pubkey_hash(&hash);
        return -1;

    case SSH_KNOWN_HOSTS_OTHER:
        emit sendMessage(QString(
            "The host key for this server was not found but an other"
            "type of key exists.\n"
            "An attacker might change the default server key to"
            "confuse your client into thinking the key does not exist\n"));
        ssh_clean_pubkey_hash(&hash);
        return -1;

    case SSH_KNOWN_HOSTS_NOT_FOUND:
        emit sendMessage(QString("Could not find known host file.\n"));

        /* FALL THROUGH to SSH_SERVER_NOT_KNOWN behavior */

    case SSH_KNOWN_HOSTS_UNKNOWN:
        hexa = ssh_get_hexa(hash, hlen);
        emit sendMessage(QString(
            "The server is unknown.\n"
            "Adding server to host file.\n"
            "Public key hash: %1\n").arg(QString(hexa)));

        ssh_string_free_char(hexa);
        ssh_clean_pubkey_hash(&hash);

        rc = ssh_session_update_known_hosts(session);
        if (rc < 0) {
            emit sendMessage(QString("Error %1\n").arg(QString(strerror(errno))));
            return -1;
        }

        break;
    case SSH_KNOWN_HOSTS_ERROR:
        emit sendMessage(QString("Error %1\n").arg(QString(ssh_get_error(session))));
        ssh_clean_pubkey_hash(&hash);
        return -1;
    }

    ssh_clean_pubkey_hash(&hash);
    return 0;
}

void SshClient::finilizeSession() {
    ssh_disconnect(session);
    ssh_free(session);
    session = nullptr;
    emit activateBox(false);
}

int SshClient::exexRemote(std::string& command)
{
    ssh_channel channel;
    int rc;
    char buffer[256];
    int nbytes;
    QString message;

    channel = ssh_channel_new(session);
    if (channel == NULL)
        return SSH_ERROR;

    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK)
    {
        emit sendMessage(
            QString("Session not opened: %1\n")
            .arg(QString(ssh_get_error(session)))
        );
        ssh_channel_free(channel);
        return rc;
    }

    qDebug() << QString("exexRemote command: ") +  QString::fromStdString(command);
    rc = ssh_channel_request_exec(channel, command.c_str());
    
    if (rc != SSH_OK)
    {
        emit sendMessage(
            QString("Fail to execute command: %1\n")
            .arg(QString(ssh_get_error(session)))
        );
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return rc;
    }

    nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    while (nbytes > 0)
    {
        message = QString(QByteArray(buffer, nbytes));
        emit sendMessage(message);
        nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    }

    if (nbytes < 0)
    {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return SSH_ERROR;
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

    return SSH_OK;
}

int SshClient::scpInit(const char* path)
{
    int rc;
    qDebug() << QString("scp init in %1").arg(QString(path));

    scp = ssh_scp_new(session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, path);
    if (scp == NULL)
    {
        qDebug() << QString("Error allocating scp session: %1\n").arg(QString(ssh_get_error(session)));
        return SSH_ERROR;
    }

    rc = ssh_scp_init(scp);

    if (rc != SSH_OK)
    {
        qDebug() << QString("Error initializing scp session: %1\n").arg(QString(ssh_get_error(session)));
        ssh_scp_free(scp);
        return rc;
    }


    return SSH_OK;
}

bool SshClient::authenticateUser() {

    emit sendProgressMessage(true, QString("Auth process"));
    
    if (initSession()) {
        emit activateBox(true);
        emit sendProgressMessage(true, QString("Auth done"));
        emit sendMessage(QString("Auth done"));

        return true;
    }
    emit sendProgressMessage(true, QString("Auth failed"));
    emit sendMessage(QString("Auth failed"));


    return false;
}

QSet<QString> SshClient::createDownloadList() {
    int rc, exit_code;
    QList<QString> downloadList;
    QSet<QString> output;
    QString projectDir = QString::fromStdString(m_data->projectDir);

    ProjectHash* local_project = new ProjectHash();
    ProjectHash* server_project = new ProjectHash();

    local_project->calcProjectHash(QString::fromStdString(m_data->projectDir));
    local_project->writeHashFile(QString::fromStdString(m_data->projectDir));

    if (m_data->forcePush) {
        downloadList = local_project->getFilesList();
        for (auto& l : downloadList)
            output.insert(l);

        return output;
    }

    auto worker = new SFTP_Wrapper(m_data);
    auto res = worker->sftp_read_sync(m_data->workingDirectory + "/project_hash.json");
    exit_code = std::get<0>(res);


    if (exit_code == SSH_OK) {
        std::string ss = std::get<1>(res);
        server_project->readHashFromString(ss);
        downloadList = local_project->compareFiles(server_project->m_filesHash);
    }
    else {
        downloadList = local_project->getFilesList();
    }

    for (auto& l : downloadList){
        output.insert(l);
    }

    delete local_project, server_project, worker;
    return output;
}

void SshClient::syncProject() {
    
    int rc;
    QString projectDir = QString::fromStdString(m_data->projectDir);
    workInProcess = true;
    
    Timer t = Timer();
    emit activateBox(false);
    emit sendProgressMessage(true, QString("Sync starts"));

    QDir prPath = QDir(projectDir);

    rc = scpInit((m_data->workingDirectory).c_str());
    
    if (rc == 0)
    {
        emit sendProgressMessage(true, QString("SCP STARTS"));
    }
    else
    {
        emit sendProgressMessage(true, QString("SCP connection failed"));
        workInProcess = false;
        return;
    }
   
    downloadList = createDownloadList();
    emit sendMessage(QString("Ready to transfer %1 files.").arg(downloadList.size()));
    
    transferFolder(prPath, false,  true);

    if (rc == 0) {
        emit sendProgressMessage(true, QString("Project was synchronized"));
    }
    else { 
        emit sendProgressMessage(true, QString("Project was not synchronized"));
        workInProcess = false;
        return; 
    }


    ssh_scp_close(scp);
    ssh_scp_free(scp);
    emit activateBox(true);
    emit sendMessage(QString("Files transer time %1 seconds.").arg(t.elapsed()));

    copyExecToProject();

    downloadList.clear();
    workInProcess = false;
    QDir::setCurrent(projectDir);

    auto worker = new SFTP_Wrapper(m_data);
    worker->createDirectory(m_data->workingDirectory + "/restore_points");
    worker->createDirectory(m_data->workingDirectory + "/result");
    delete worker;
}

void SshClient::transferFolder(QDir path, bool createDir,  bool checkHash) {
    int rc = SSH_OK;

    QString relativeFile;
    QDir projectPath = QDir(QString::fromStdString(m_data->projectDir));
    QDir::setCurrent(path.absolutePath());

    if (createDir) {
        qDebug() << path.dirName();
        rc = ssh_scp_push_directory(scp,
            path.dirName().toStdString().c_str(),
            S_IREAD | S_IWRITE | S_IEXEC);
    }
    

    if (rc != SSH_OK) { 
        qDebug() << "push dir failed";
        sendProgressMessage(true, QString("push dir failed"));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        return; 
    }

    for (auto& d : path.entryList(QDir::Files | QDir::NoDot | QDir::NoDotDot | QDir::Readable)) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            workInProcess = false;
            return;
        }

        relativeFile = path.filePath(d);
        QDir rel = projectPath.relativeFilePath(relativeFile);
        
        if (!checkHash) {
            transferFile(d);
            sendProgressMessage(true, QString("send file: ") + d);
        }
        else {
            if (downloadList.contains(rel.path())) {
                transferFile(d);
                sendProgressMessage(true, QString("send file: ") + d);
            }
        }
    }

    for (auto& d : path.entryList(QDir::Dirs | QDir::NoDot | QDir::NoDotDot)) {
        
        if (QThread::currentThread()->isInterruptionRequested()) {
            workInProcess = false;
            return;
        }

        transferFolder(path.filePath(d), checkHash);
    }

    if (createDir) {
        ssh_scp_leave_directory(scp);
    }
}

void SshClient::transferFile(QFile file) {
    int rc;

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << file << "  " << "not opened";
        return;
    }

    rc = ssh_scp_push_file(scp,
        file.fileName().toStdString().c_str(),
        file.size(), S_IREAD | S_IWRITE | S_IEXEC);

    QByteArray blob = file.readAll();
    rc = ssh_scp_write(scp, blob, blob.size());
}

int SshClient::exexCommand() {

    auto c = m_command_builder->getExecutableCommand();

    if (m_data->autoChangeDir == true && !SFTP_Wrapper(m_data).isDirExist(m_data->workingDirectory)) {
        emit sendMessage(
            QString("Directory %1 do not exists").arg(QString(m_data->workingDirectory.c_str()))
        );
        return SSH_ERROR;
    }

    emit activateBox(false);
    emit sendProgressMessage(true, QString("Exex command"));

    int exit_code = -2;

    if (m_data->cleanRestorePoints) {
        exexRemote(m_command_builder->cleanRestorePoints());
    }

    emit sendMessage(QString("execution command: ") + QString::fromStdString(c));
    qDebug() << QString("execution command: ") + QString::fromStdString(c);
    exit_code = exexRemote(c);

    emit activateBox(true);
    emit sendProgressMessage(true, QString(""));

    return exit_code;
}

void SshClient::terminateSession() {
    if (session != nullptr) {
        finilizeSession();
        connected = false;
        emit sendMessage(QString("Connection terminated"));
    }

}

void SshClient::buildREMP(QString src_dir) {
    int exit_code;
    std::string faradayDirName = QDir(src_dir).dirName().toStdString();

    abstractSync(src_dir, m_command_builder->getDefaultDir().c_str());
    exit_code = exexRemote(m_command_builder->getBuildCommand(faradayDirName));

}

void SshClient::abstractSync(QString& src_dir, const char* server_path) {
    int rc;

    workInProcess = true;
    Timer t = Timer();
    emit activateBox(false);
    emit sendProgressMessage(true, QString("Sync starts"));


    rc = scpInit(server_path);
    if (rc == 0)
    {
        emit sendProgressMessage(true, QString("SCP STARTS"));
    }
    else
    {
        emit sendProgressMessage(true, QString("SCP connection failed"));
        workInProcess = false;
        return;
    }

    transferFolder(src_dir);

    if (rc == 0) {
        emit sendProgressMessage(true, QString("Project was synchronized"));
    }
    else {
        emit sendProgressMessage(true, QString("Project was not synchronized"));
        workInProcess = false;
        return;
    }


    ssh_scp_close(scp);
    ssh_scp_free(scp);
    emit activateBox(true);
    workInProcess = false;
    emit sendMessage(QString("Files transer time %1 seconds.").arg(t.elapsed()));

}

void SshClient::copyExecToProject() {
    int exit_code;

    if (!SFTP_Wrapper(m_data).isDirExist(m_data->workingDirectory)) {
        emit sendMessage(
            QString("Directory %1 do not exists").arg(QString(m_data->workingDirectory.c_str())));
        return;
    }

    std::string copy_command = m_command_builder->copyExecCommand();
    exit_code = exexRemote(copy_command);

    if (exit_code == SSH_OK) {
        sendMessage(QString("Execution files copied to project"));
    }
    else {
        sendMessage(QString("Copy failed"));
    }
}

void SshClient::getResults(QList<QString> datFiles) {
    int exit_code;
    std::tuple<int, QList<QString>> dir_res;
    auto dowloadFiles = datFiles;

    if (!SFTP_Wrapper(m_data).isDirExist(m_data->workingDirectory)) {
        emit sendMessage(
            QString("Directory %1 do not exists").arg(QString(m_data->workingDirectory.c_str())));
        return ;
    }

    sendMessage(QString("Starting to collect results"));

    std::string command = m_command_builder->getResultsCommand();
    exit_code = exexRemote(command);

    if (exit_code == SSH_OK) {
        sendMessage(QString("Collecting successful"));
    }
    else {
        sendMessage(QString("Collecting failed"));
        return;
    }

    auto worker = new SFTP_Wrapper(m_data);
    dir_res = worker->getListDirectory(m_data->workingDirectory);
    exit_code = std::get<int>(dir_res);

    if (exit_code != SSH_OK) {
        sendMessage(QString("Not found %1 dir").arg(QString::fromStdString(m_data->workingDirectory)));
        return;
    }

    QList<QString> dirs = std::get<QList<QString>>(dir_res);

    for (auto& f : dowloadFiles) {

        if (std::find(dirs.begin(), dirs.end(), f) != dirs.end()) {
            QString file = QDir(QString::fromStdString(m_data->projectDir)).filePath(f);
            exit_code = worker->sftpDownloadBinaryFile(
                m_data->workingDirectory + '/' + f.toStdString(),
                m_data->projectDir + '/' + f.toStdString()
            );

            if (exit_code != SSH_OK) {
                sendMessage(QString("Cant open file %1").arg(f));
                continue;
            }
            sendMessage(QString("File %1 have loaded").arg(f));
        }
        else {
            sendMessage(QString("File %1 not found").arg(f));
        }

    }


    delete worker;
}