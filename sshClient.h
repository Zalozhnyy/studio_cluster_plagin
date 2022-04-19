#ifndef SSH_CLIENT
#define SSH_CLIENT

#include <libssh/libssh.h>
#include <string>
#include <functional>
#include <QObject>
#include <QThread>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <memory>

#include "cluster_data.h"
#include "projectHash.h"
#include "commandConstructor.h"

class SshClient : public QObject {
    Q_OBJECT

public:
    SshClient(std::shared_ptr<SshData::Connection> data, CommandBuilder* builder) :
        m_data(data), m_command_builder(builder) {}

    SshClient(CommandBuilder* builder): m_command_builder(builder) {}

    ~SshClient();

    void copyExecToProject();
    void setData(std::shared_ptr<SshData::Connection> data) { m_data = data; }

    
    bool workInProcess = false;
    bool connected = false;

protected:
    std::shared_ptr<SshData::Connection> m_data;

    ssh_session session = nullptr;

    bool initSession();
    void finilizeSession();

private:
    CommandBuilder* const m_command_builder;
    ssh_scp scp = nullptr;

    QSet<QString> downloadList;

    int authenticate_kbdint();
    int authenticate_pubkey();
    int authenticate_password();

    int verify_knownhost();
    int exexRemote(std::string& command);
    int scpInit(const char* path);

    void transferFile(QFile file);
    void transferFolder(QDir path, bool createDir=true, bool checkHash=false);

    void abstractSync(QString& src_dir, const char* server_path);
    QSet<QString> createDownloadList();

public slots:
    bool authenticateUser();
    int exexCommand();
    void syncProject();
    void terminateSession();
    void buildREMP(QString src_dir);
    void getResults(QList<QString> datFiles);


signals:
    void activateBox(bool state);
    void sendMessage(QString message);
    void sendProgressMessage(bool state, QString message = NULL);
};



class Controller : public QObject
{
    Q_OBJECT


public:
    QThread workerThread;

    Controller(std::shared_ptr<SshClient> worker) {

        worker->moveToThread(&workerThread);

        connect(&workerThread, &QThread::finished, worker.get(), &QObject::deleteLater);

        connect(this, &Controller::auth, worker.get(), &SshClient::authenticateUser);
        connect(this, &Controller::exec, worker.get(), &SshClient::exexCommand);
        connect(this, &Controller::sync, worker.get(), &SshClient::syncProject);
        connect(this, &Controller::term, worker.get(), &SshClient::terminateSession);
        connect(this, &Controller::build, worker.get(), &SshClient::buildREMP);
        connect(this, &Controller::getResults, worker.get(), &SshClient::getResults);

        workerThread.start();
    }

    ~Controller() {
        workerThread.quit();
        workerThread.wait();
    }

    void interruptThread() { workerThread.requestInterruption(); }

signals:
    void auth();
    void exec();
    void sync();
    void term();
    void getResults(QList<QString> datFiles);
    void build(QString src_dir);



};


#endif
