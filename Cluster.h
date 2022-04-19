#ifndef CLUSTER_H
#define CLUSTER_H

#include "plugin.h"
#include "sshClient.h"
#include "sftpWrapper.h"
#include "cluster_data.h"
#include "project_data.h"
#include "commandConstructor.h"
#include "cluster_console.h"


#include <QDebug>
#include <QTextEdit>
#include <QUndoCommand>
#include <map>
#include <memory>
#include <deque>


#define MAX_HISTORY_SIZE 20

QT_BEGIN_NAMESPACE
class QUndoStack;
class QTreeWidgetItem;
class QAction;
QT_END_NAMESPACE


struct SshWorker{

    std::shared_ptr<SshData::Connection> data;
    std::shared_ptr<SshClient> client;
    std::shared_ptr<Controller> thread;
    std::shared_ptr<std::deque<std::string>> history;

};

class QTSSH;
class ClusterCommand;

class Cluster final : public QObject, public Studio::Plugin {

	Q_OBJECT
	Q_PLUGIN_METADATA(IID "GridStudio.PluginInterface.cluster" FILE "cluster.json")
	Q_INTERFACES(Studio::Plugin)


public:
	Cluster();
	~Cluster() override;

    void selected() override;
    void checked(Qt::CheckState state) override;
    void menuRequested(QMenu* menu) override;

    void itemSelected(const QString& item_id) override;
    void itemChecked(const QString& item_id, Qt::CheckState state) override;
    void itemMenuRequested(const QString& item_id, QMenu* menu) override;

    void setDataset(const QVariant& data) override;
    QVariant getDataset() const override;
    void datasetUpdated(const QString& plugin_id) override;

    void openFile(const QString& path) override;
    void saveFiles() override;
    void closeFiles() override;
    QStringList getFileNames() const override;

    void unload() override;
    bool idle() override;

private:
    void load() override;
    int selectedIndex;

    std::map<size_t, SshWorker> m_workers;

    QAction* m_create_file = nullptr;
    QAction* m_remove_act = nullptr;
    

    // editor panel
    QTSSH* m_editor = nullptr;

    // undo stack
    QUndoStack* m_undo_stack = nullptr;


    Console* m_serverConsole = nullptr;

    std::shared_ptr<SshData::Connection> m_data = nullptr;
    std::shared_ptr<SshClient> m_client = nullptr;
    std::shared_ptr<Controller> m_thread = nullptr;
    std::shared_ptr<std::deque<std::string>> m_history = nullptr;
    std::deque<std::string>::const_reverse_iterator m_history_iterator;


    ProjectData::Data project;
    CommandBuilder* m_command_builder = nullptr;

    QString projectPath;
    QString projectFolderName;

    void setData(const SshData::Connection& data);
    bool checkBackgroundProcess();
    void saveSession();


private slots:
    void auth();
    void sync();
    void exec();
    void checkExecutionFiles();
    void pushMessageToServerAnswer(QString message);
    void pushBuild();
    void getResults();
    void terminateConnection();

    void createConnection();
    void removeSession();


    void hostChanged(QString data);
    void usernameChanged(QString data);
    void passwordChanged(QString data);
    void extendedPathchanged(QString data);
    void execCommandChanged(QString data);
    void rootDirChanged(QString data);
    void sourceEnvChanged(QString data);

    void pushHistoryCommand();
    void historyGetNext();
    void historyGetPrevious();


    void test();

};

class ClusterCommand : public QUndoCommand
{
public:
    ClusterCommand(const QString& text,
        const SshData::Connection& undo, const SshData::Connection& redo,
        const std::function<void(const SshData::Connection data)>& setData = nullptr)
        : QUndoCommand(text)
        , m_undo_data(undo)
        , m_redo_data(redo)
        , m_set_data(setData)
    {};

    void undo() override;
    void redo() override;

private:
    const SshData::Connection m_undo_data;
    const SshData::Connection m_redo_data;
    const std::function<void(SshData::Connection data)> m_set_data;

};



#endif