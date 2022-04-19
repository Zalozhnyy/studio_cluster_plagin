#ifndef QT_SSH
#define QT_SSH

#include <QtWidgets/QDialog>
#include "ui_qt_ssh.h"
#include <QString>

#include "cluster_data.h"
#include "sshClient.h"
#include "Cluster.h"


class QTSSH : public QWidget
{
    Q_OBJECT

public:
    QTSSH(QWidget *parent = Q_NULLPTR);

    void setData(SshData::Connection& data);
    void setCommand(std::string command);


private:
    Ui::QTSSHClass ui;

    SshData::Connection m_data;


    QString valideteSlash(QString value);

    bool eventFilter(QObject* obj, QEvent* event);

public slots:
    void activateCommandLineBox(bool state);
    void activateAuthBox(bool state);
    void setProgressMessage(bool state, QString message = NULL);


signals:
    void authButton();
    void execButton();
    void syncButton();
    void checkExecutableButton();
    void pushBuildButton();
    void copyExecFilesButton();
    void terminateConnectionButton();
    void getResultButton();
    
    void testButton();

    void criticalDataChanged();


    void hostChanged(QString data);
    void usernameChanged(QString data);
    void passwordChanged(QString data);
    void extendedPathchanged(QString data);
    void rootDirChanged(QString data);
    void sourceEnvChanged(QString data);
    
    void cdCheckBoxChanged(bool data);
    void forcePushCheckBoxChanged(bool data);
    void cleanREstorePointsBoxChanged(bool data);

    void getPreviousCommandFromHistory();
    void getNextCommandFromHistory();


};

#endif
