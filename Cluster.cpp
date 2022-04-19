#include <fstream>
#include <sstream>

#include <QUndoStack>
#include <QAction>
#include <QMenu>
#include <QFileDialog>
#include <QByteArray>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QApplication>
#include <QDockWidget>
#include <QTextEdit>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QDebug>
#include <QMessageBox>

#include <map>
#include <memory>
#include <algorithm>


#include "Cluster.h"
#include "qt_ssh.h"
#include "timer.h"
#include "projectHash.h"
#include "sftpWrapper.h"


Cluster::Cluster()
    : Studio::Plugin()
{
}

Cluster::~Cluster()
{
}

void Cluster::load()
{
    qRegisterMetaType<QList<QString>>();
    // create undo stack
    m_undo_stack = new QUndoStack(this);
    getMainWindow()->addUndoStack(m_undo_stack);


    QDockWidget* console_doc = getMainWindow()->getDockWidget(getId(), Qt::BottomDockWidgetArea);
    console_doc->setWindowTitle(QString("Server console"));

    m_serverConsole = new Console;
    m_serverConsole->setPrompt(">>", true);
    connect(m_serverConsole, &Console::commandExecuted, this, &Cluster::execCommandChanged);


    console_doc->setWidget(m_serverConsole);

    // create editor
    m_editor = new QTSSH();

    connect(m_editor, &QTSSH::hostChanged, this, &Cluster::hostChanged);
    connect(m_editor, &QTSSH::usernameChanged, this, &Cluster::usernameChanged);
    connect(m_editor, &QTSSH::passwordChanged, this, &Cluster::passwordChanged);
    connect(m_editor, &QTSSH::extendedPathchanged, this, &Cluster::extendedPathchanged);
    connect(m_editor, &QTSSH::rootDirChanged, this, &Cluster::rootDirChanged);
    connect(m_editor, &QTSSH::sourceEnvChanged, this, &Cluster::sourceEnvChanged);

    connect(m_editor, &QTSSH::cdCheckBoxChanged, [this](bool data) {m_data->autoChangeDir = data; });
    connect(m_editor, &QTSSH::forcePushCheckBoxChanged, [this](bool data) {m_data->forcePush = data; });
    connect(m_editor, &QTSSH::cleanREstorePointsBoxChanged, [this](bool data) {m_data->cleanRestorePoints = data; });

    connect(m_editor, &QTSSH::execButton, this, &Cluster::pushHistoryCommand);
    connect(m_editor, &QTSSH::getNextCommandFromHistory, this, &Cluster::historyGetNext);
    connect(m_editor, &QTSSH::getPreviousCommandFromHistory, this, &Cluster::historyGetPrevious);



    connect(m_editor, &QTSSH::authButton, this, &Cluster::auth);
    connect(m_editor, &QTSSH::syncButton, this, &Cluster::sync);
    connect(m_editor, &QTSSH::execButton, this, &Cluster::exec);
    connect(m_editor, &QTSSH::checkExecutableButton, this, &Cluster::checkExecutionFiles);
    connect(m_editor, &QTSSH::pushBuildButton, this, &Cluster::pushBuild);
    connect(m_editor, &QTSSH::criticalDataChanged, this, &Cluster::terminateConnection);
    connect(m_editor, &QTSSH::terminateConnectionButton, this, &Cluster::terminateConnection);
    connect(m_editor, &QTSSH::getResultButton, this, &Cluster::getResults);
    connect(m_editor, &QTSSH::copyExecFilesButton, [this]() {m_client->copyExecToProject(); });
    //connect(m_editor, &QTSSH::getResultButton, this, &Cluster::test);
    
    
    getMainWindow()->addPropertiesWidget(m_editor);


    m_command_builder = new CommandBuilder();

    // create plugin node
    Studio::ProjectTreeItem plugin_item;
    plugin_item.name = QString("CLUSTER");
    plugin_item.icon = QIcon(":/cluster/images/command-line.png");

    getMainWindow()->getProjectTree()->setPlugin(getId(), plugin_item);

    // create actions
    m_create_file = new QAction(tr("Add cluster connection"), this);
    connect(m_create_file, &QAction::triggered, this, &Cluster::createConnection);
        
    m_remove_act = new QAction(tr("remove connection"), this);
    connect(m_remove_act, &QAction::triggered, this, &Cluster::removeSession);



}

void Cluster::test() {

    Timer t = Timer();
    SFTP_Wrapper* worker = new SFTP_Wrapper(m_data);
    connect(worker, &SshClient::sendMessage, this, &Cluster::pushMessageToServerAnswer);
    worker->createRelatedDir("/asdasd");

    qDebug() << t.elapsed();

    delete worker;
}

void Cluster::createConnection() {
    size_t n = 0;
    for (; n < 10; n++) {
        if (m_workers.count(n)) continue;
        else if (n == 9) return;
        else break;        
    }

    m_workers[n].data = std::make_shared<SshData::Connection>();
    m_workers[n].client = std::make_shared<SshClient>(m_workers[n].data, m_command_builder);
    m_workers[n].thread = std::make_shared<Controller>(m_workers[n].client);
    m_workers[n].history = std::make_shared<std::deque<std::string>>();

    m_data = m_workers[n].data;
    m_history = m_workers[n].history;
    
    m_data->projectDir = projectPath.toStdString();
    m_data->extendedDir =m_data->default_folder_name + '/' + projectFolderName.toStdString();


    connect(m_workers[n].client.get(), &SshClient::activateBox, m_editor, &QTSSH::activateCommandLineBox);
    connect(m_workers[n].client.get(), &SshClient::sendProgressMessage, m_editor, &QTSSH::setProgressMessage);
    connect(m_workers[n].client.get(), &SshClient::sendMessage, this, &Cluster::pushMessageToServerAnswer);

    Studio::ProjectTreeItem item;
    // set name
    item.name = QString("Connection %1").arg(n + 1);
    item.icon = QIcon(":/cluster/images/black_dot.jpg");

    // add layer to project tree
    getMainWindow()->getProjectTree()->setPluginItem(getId(), QString::number(n), item);


}

void Cluster::unload()
{
}

bool Cluster::idle()
{
    if (checkBackgroundProcess()) {
        std::map<size_t, SshWorker>::iterator it;
        for (it = m_workers.begin(); it != m_workers.end(); it++) {
            it->second.client->terminateSession();            
        }
        return true;
    }

    else {
        return false;
    }
    return true;
}

bool Cluster::checkBackgroundProcess() {
    bool backgruundExist = false;
    std::map<size_t, SshWorker>::iterator it;
    for (it = m_workers.begin(); it != m_workers.end(); it++) {
        if (it->second.client->workInProcess == true) {
            backgruundExist = true;
            break;
        }
    }

    if (backgruundExist) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(m_editor, tr("Warning"),
            tr("Background process still running, do you want to stop it?"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            std::map<size_t, SshWorker>::iterator it;
            for (it = m_workers.begin(); it != m_workers.end(); it++) {
                if (it->second.client->workInProcess == true) {
                    it->second.thread->interruptThread();
                }
            }

            return true;

        }
        else {
            return false;
        }

    }

    return true;
}

void Cluster::selected()
{
    getMainWindow()->hidePropertiesWidget();
}

void Cluster::checked(Qt::CheckState state)
{

}

void Cluster::menuRequested(QMenu* menu)
{
    menu->addAction(m_create_file);
}

void Cluster::itemSelected(const QString& item_id)
{
    m_undo_stack->setActive();
    m_undo_stack->clear();

    selectedIndex = item_id.toInt();

    m_data = m_workers[selectedIndex].data;
    m_client = m_workers[selectedIndex].client;
    m_thread = m_workers[selectedIndex].thread;
    m_history = m_workers[selectedIndex].history;
    m_history_iterator = m_history->crbegin();
    
    getMainWindow()->showPropertiesWidget(m_editor);

    m_editor->setData(*m_data);
    m_command_builder->setData(m_data);
    
    if (m_history->size() > 0)
        m_editor->setCommand(*m_history_iterator);

    emit m_client->activateBox(m_client->connected);

}

void Cluster::itemChecked(const QString& item_id, Qt::CheckState state)
{
}

void Cluster::itemMenuRequested(const QString& item_id, QMenu* menu)
{
    menu->addAction(m_remove_act);
}

void Cluster::openFile(const QString& path)
{
    projectPath = QFileInfo(getMainWindow()->getProjectPath()).absolutePath();
    projectFolderName = QFileInfo(getMainWindow()->getProjectPath()).dir().dirName();

    std::vector<SshData::Session> data;
    std::map<size_t, SshWorker>::iterator it;

    //delete old plugin items on new project event
    for (it = m_workers.begin(); it != m_workers.end(); it++) {
        getMainWindow()->getProjectTree()->removePluginItem(getId(), QString::number(it->first));
    }

    //clear memory before open new project
    m_workers.clear();
    m_undo_stack->clear();
    m_command_builder->setData(nullptr);


    std::ifstream istrm("ConnectionData.json");
    if (istrm.is_open()) {
        istrm >> data;
        istrm.close();
    }

    for (int i = 0; i < data.size(); i++) {

        createConnection(); // create worker and set it to work (set m_data and m_history)
        m_data->projectDir = projectPath.toStdString();

        m_data->username = data[i].username;
        m_data->password = data[i].password;
        m_data->host = data[i].host;
        m_data->extendedDir = data[i].extendedDir;
        m_data->rootServerDir = data[i].rootServerDir;
        m_data->sourceEnvironmet = data[i].sourceEnvironmet;
        m_data->baseServerDir = '/' + m_data->rootServerDir + '/' + m_data->username;
        m_data->workingDirectory = m_data->baseServerDir + '/' + m_data->extendedDir;

        for (auto& c : data[i].commandHistory) {
            m_history->push_back(c);
        }


        m_editor->setData(*m_data);
    }


    // read data from a file
    QByteArray ba = getMainWindow()->getProjectPath().toLocal8Bit();
    std::filebuf fb;
    if (fb.open(ba.data(), std::ios::in)) {
        std::istream is(&fb);
        is >> project;
        fb.close();
    }
       
    m_undo_stack->clear();

}

void Cluster::saveFiles()
{
    saveSession();
}

void Cluster::closeFiles()
{

}

QStringList Cluster::getFileNames() const
{
    QStringList list;


    return list;
}

void Cluster::setDataset(const QVariant& data)
{
}

QVariant Cluster::getDataset() const
{
    return QVariant();
}

void Cluster::datasetUpdated(const QString& plugin_id)
{

}

void Cluster::auth() {

    if (m_client->workInProcess)
        return;
    
    emit m_thread->auth();
}

void Cluster::sync() {
    
    if (SFTP_Wrapper(m_data).isDirExist(m_data->workingDirectory)) {
        
        QMessageBox::StandardButton reply = QMessageBox::warning(m_editor,
            QString("Check folders"),
            QString("Folder %1 already exist. Use it to sync project with server?")
            .arg(QString::fromStdString(m_data->workingDirectory)),
            QMessageBox::Yes | QMessageBox::No);


        if (reply == QMessageBox::Yes) {
            emit m_thread->sync();
        }

    }

    else {
        SFTP_Wrapper* worker = new SFTP_Wrapper(m_data);
        connect(worker, &SshClient::sendMessage, this, &Cluster::pushMessageToServerAnswer);
        
        if (worker->createRelatedDir(m_data->workingDirectory)) {
            emit m_thread->sync();
        }

        delete worker;
    }

}

void Cluster::getResults() {
    if (m_client->workInProcess)
        return;

    QList<QString> datFiles;
    for (auto& df : project.output_dat_file_names)
        datFiles.append(QString::fromStdString(df));
    
    emit m_thread->getResults(datFiles);
}

void Cluster::exec() {

    emit m_thread->exec();
}

void Cluster::pushMessageToServerAnswer(QString message) {
    m_serverConsole->setMsgText(message);

    m_serverConsole->append("");
    // move cursor
    m_serverConsole->moveCursor(QTextCursor::End);
    // display the prompt again
    m_serverConsole->displayPrompt();

}

void Cluster::terminateConnection() {
    m_editor->activateCommandLineBox(false);
    emit m_thread->term();
}

void Cluster::saveSession() {

    std::vector<SshData::Session> data;
    
    std::map<size_t, SshWorker>::iterator it;
    for (it = m_workers.begin(); it != m_workers.end(); it++) {
        SshData::Session s;
        
        s.username = it->second.data->username;
        s.password = it->second.data->password;
        s.host = it->second.data->host;
        s.extendedDir = it->second.data->extendedDir;
        s.rootServerDir = it->second.data->rootServerDir;
        s.sourceEnvironmet = it->second.data->sourceEnvironmet;
        
        s.commandHistory = *it->second.history.get();

        data.push_back(s);
    }


    std::ofstream ostrm("ConnectionData.json");

    if (ostrm.is_open()) {

        ostrm << data;
        ostrm.close();
    }
}

void Cluster::removeSession()
{
    m_workers[selectedIndex].client->terminateSession();
    
    m_workers[selectedIndex].thread.reset();
    m_workers[selectedIndex].client.reset();
    m_workers[selectedIndex].data.reset();

    m_workers.erase(selectedIndex);
    
    getMainWindow()->hidePropertiesWidget();
    getMainWindow()->getProjectTree()->removePluginItem(getId(), QString::number(selectedIndex));
}

void Cluster::checkExecutionFiles() {

    std::map<std::string, bool> filesExist;
    std::string message = "";
    
    if (!SFTP_Wrapper(m_data).isDirExist(m_data->workingDirectory)) {
        pushMessageToServerAnswer(
            QString("Directory %1 do not exists").arg(QString(m_data->workingDirectory.c_str())));
        return ;
    }

    std::vector<std::string> exucutableFiles{ "faraday.ex", "gursa.ex", "splicer.ex", "jointer.ex" };

    filesExist = SFTP_Wrapper(m_data).checkFilesInFolder(m_data->workingDirectory, exucutableFiles);

    std::map<std::string, bool>::iterator it = filesExist.begin();
    while (it != filesExist.end()) {
        message = it->first + ": " + ((it->second) ? "found" : "not found");
        pushMessageToServerAnswer(QString::fromStdString(message));
        it++;
    }


}

void Cluster::pushBuild() {

    QString dir = QFileDialog::getExistingDirectory(m_editor, tr("Open remp src directory"),
        getMainWindow()->getProjectPath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (dir.isEmpty())
        return;
    

    emit m_thread->build(dir);
    
}

void Cluster::setData(const SshData::Connection& data) {
    m_workers[selectedIndex].data = std::make_shared<SshData::Connection>(data);
    m_data = m_workers[selectedIndex].data;
    m_workers[selectedIndex].client->setData(m_data);
    m_command_builder->setData(m_data);
    m_editor->setData(*m_data);
}

void Cluster::hostChanged(QString data) {

    SshData::Connection redo = *m_data;
    redo.host = data.toStdString();

    // create undo command
    ClusterCommand* cmd = new ClusterCommand(tr("Host changed"),
        *m_data, redo,
        std::bind(&Cluster::setData, this, std::placeholders::_1));
    // push in stack executes it and apples new data via a callback
    m_undo_stack->push(cmd);

    getMainWindow()->notifyUpdate(getId());
    getMainWindow()->setProjectChanged(getId());

}

void Cluster::usernameChanged(QString data) {
    SshData::Connection redo = *m_data;
    redo.username = data.toStdString();
    redo.baseServerDir = '/' + redo.rootServerDir + '/' + redo.username;
    redo.workingDirectory = redo.baseServerDir + '/' + redo.extendedDir;

    // create undo command
    ClusterCommand* cmd = new ClusterCommand(tr("Username changed"),
        *m_data, redo,
        std::bind(&Cluster::setData, this, std::placeholders::_1));
    // push in stack executes it and apples new data via a callback
    m_undo_stack->push(cmd);

    getMainWindow()->notifyUpdate(getId());
    getMainWindow()->setProjectChanged(getId());

}

void Cluster::passwordChanged(QString data) {
    m_data->password = data.toStdString();
    }

void Cluster::execCommandChanged(QString data) {
    if (data == "") return;
    m_data->command = data.toStdString();
    exec();
}

void Cluster::rootDirChanged(QString data) {
    SshData::Connection redo = *m_data;
    redo.rootServerDir = data.toStdString();
    redo.baseServerDir = '/' + redo.rootServerDir + '/' + redo.username;
    redo.workingDirectory = redo.baseServerDir + '/' + redo.extendedDir;


    // create undo command
    ClusterCommand* cmd = new ClusterCommand(tr("Root server dir changed"),
        *m_data, redo,
        std::bind(&Cluster::setData, this, std::placeholders::_1));
    // push in stack executes it and apples new data via a callback
    m_undo_stack->push(cmd);

    getMainWindow()->notifyUpdate(getId());
    getMainWindow()->setProjectChanged(getId());


}

void Cluster::sourceEnvChanged(QString data) {
    SshData::Connection redo = *m_data;
    redo.sourceEnvironmet = data.toStdString();

    // create undo command
    ClusterCommand* cmd = new ClusterCommand(tr("Environment command changed"),
        *m_data, redo,
        std::bind(&Cluster::setData, this, std::placeholders::_1));
    // push in stack executes it and apples new data via a callback
    m_undo_stack->push(cmd);

    getMainWindow()->notifyUpdate(getId());
    getMainWindow()->setProjectChanged(getId());

}

void Cluster::pushHistoryCommand() {

    if (m_data->command == "") return;

    if (m_history->size() > MAX_HISTORY_SIZE) {
        m_history->push_back(m_data->command);
        m_history->pop_front();
    }

    else {
        m_history->push_back(m_data->command);
    }
   
    m_history_iterator = m_history->crbegin();

}

void Cluster::historyGetNext(){

    if (m_history->size() == 0) return;


    if (m_history_iterator != m_history->crbegin()) {
        m_history_iterator--;
    }
    else {
        m_history_iterator = m_history->crbegin();
    }

    m_editor->setCommand(*m_history_iterator);
    m_data->command = *m_history_iterator;

}

void Cluster::historyGetPrevious(){

    if (m_history->size() == 0) return;

    if (m_history_iterator != m_history->crend() - 1) {
        m_history_iterator++;

    }
    else {
        m_history_iterator = m_history->crend() - 1;
    }

    m_editor->setCommand(*m_history_iterator);
    m_data->command = *m_history_iterator;

}

void Cluster::extendedPathchanged(QString data) {
    SshData::Connection redo = *m_data;
    redo.extendedDir = data.toStdString();
    redo.workingDirectory = redo.baseServerDir + '/' + redo.extendedDir;

    // create undo command
    ClusterCommand* cmd = new ClusterCommand(tr("Extended path changed"),
        *m_data, redo,
        std::bind(&Cluster::setData, this, std::placeholders::_1));
    // push in stack executes it and apples new data via a callback
    m_undo_stack->push(cmd);

    getMainWindow()->notifyUpdate(getId());
    getMainWindow()->setProjectChanged(getId());
}

void ClusterCommand::undo()
{
    if (m_set_data) {
        m_set_data(m_undo_data);
    }
}

void ClusterCommand::redo()
{
    if (m_set_data) {
        m_set_data(m_redo_data);
    }
}


