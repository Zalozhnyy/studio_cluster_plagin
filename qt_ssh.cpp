#include "qt_ssh.h"
#include "sshClient.h"

#include <QDir>
#include <Qlabel>
#include <QKeyEvent>
#include <QDebug>

#include <QLineEdit>
#include <QRadioButton>


QTSSH::QTSSH(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    ui.execCommandLineEdit->installEventFilter(this);

    connect(ui.ExexCommandButton, &QPushButton::clicked, [this]() {
       // emit execCommandChanged(ui.execCommandLineEdit->text());
        emit execButton();});

    connect(ui.AuthButton, &QPushButton::clicked, this, &QTSSH::authButton);
    connect(ui.terminateConnectionButton, &QPushButton::clicked, this, &QTSSH::terminateConnectionButton);
    connect(ui.syncProjectWithServerButton, &QPushButton::clicked, this, &QTSSH::syncButton);
    connect(ui.checkExecutableButton, &QPushButton::clicked, this, &QTSSH::checkExecutableButton);
    connect(ui.copyExecFilesButton, &QPushButton::clicked, this, &QTSSH::copyExecFilesButton);
    connect(ui.pushBuildButton, &QPushButton::clicked, this, &QTSSH::pushBuildButton);
    connect(ui.getResultButton, &QPushButton::clicked, this, &QTSSH::getResultButton);
    


    connect(ui.ipLineEdit, &QLineEdit::editingFinished,
        [this]() {
            emit hostChanged(ui.ipLineEdit->text()); 
            emit criticalDataChanged();
        });

    connect(ui.usernameLineEdit, &QLineEdit::editingFinished,
        [this]() {
            emit usernameChanged(ui.usernameLineEdit->text()); 
            emit criticalDataChanged();
        });
    connect(ui.passwordLineEdit, &QLineEdit::editingFinished,
        [this]() {emit passwordChanged(ui.passwordLineEdit->text());
        emit criticalDataChanged();
        });
    
    connect(ui.extendedPathLineEdit, &QLineEdit::editingFinished,
        [this]() {emit extendedPathchanged(valideteSlash(ui.extendedPathLineEdit->text())); });

    //connect(ui.execCommandLineEdit, &QLineEdit::editingFinished,
    //    [this]() {emit execCommandChanged(ui.execCommandLineEdit->text()); });

    connect(ui.rootDirectoryLineEdit, &QLineEdit::editingFinished,
        [this]() {emit rootDirChanged(valideteSlash(ui.rootDirectoryLineEdit->text())); });

    connect(ui.sourceLineEdit, &QLineEdit::editingFinished,
        [this]() {emit sourceEnvChanged(ui.sourceLineEdit->text()); });

    connect(ui.cdRadioButton, &QCheckBox::stateChanged, [this]() {emit cdCheckBoxChanged(ui.cdRadioButton->isChecked()); });
    connect(ui.forcePushCheckBox, &QCheckBox::stateChanged, [this]() {emit forcePushCheckBoxChanged(ui.forcePushCheckBox->isChecked()); });
    connect(ui.cleanRPcheckBox, &QCheckBox::stateChanged, [this]() {emit cleanREstorePointsBoxChanged(ui.cleanRPcheckBox->isChecked()); });

}

void QTSSH::setCommand(std::string command) {
    m_data.command = command;

    ui.execCommandLineEdit->blockSignals(true);
    ui.execCommandLineEdit->setText(QString::fromStdString(m_data.command));
    ui.execCommandLineEdit->blockSignals(false);

}

void QTSSH::setData(SshData::Connection& data) {
    m_data = data;

    ui.ipLineEdit->blockSignals(true);
    ui.ipLineEdit->setText(QString::fromStdString(m_data.host));
    ui.ipLineEdit->blockSignals(false);
    
    ui.usernameLineEdit->blockSignals(true);
    ui.usernameLineEdit->setText(QString::fromStdString(m_data.username));
    ui.usernameLineEdit->blockSignals(false);
    
    ui.passwordLineEdit->blockSignals(true);
    ui.passwordLineEdit->setText(QString::fromStdString(m_data.password));
    ui.passwordLineEdit->blockSignals(false);
    
    ui.extendedPathLineEdit->blockSignals(true);
    ui.extendedPathLineEdit->setText(QString::fromStdString(m_data.extendedDir));
    ui.extendedPathLineEdit->blockSignals(false);
    
    ui.execCommandLineEdit->blockSignals(true);
    ui.execCommandLineEdit->setText(QString::fromStdString(m_data.command));
    ui.execCommandLineEdit->blockSignals(false);

    ui.sourceLineEdit->blockSignals(true);
    ui.sourceLineEdit->setText(QString::fromStdString(m_data.sourceEnvironmet));
    ui.sourceLineEdit->blockSignals(false);
    
    ui.rootDirectoryLineEdit->blockSignals(true);
    ui.rootDirectoryLineEdit->setText(QString::fromStdString(m_data.rootServerDir));
    ui.rootDirectoryLineEdit->blockSignals(false);

    ui.basePathLineEdit->blockSignals(true);
    ui.basePathLineEdit->setText(QString::fromStdString(m_data.baseServerDir));
    ui.basePathLineEdit->blockSignals(false);

    ui.cdRadioButton->blockSignals(true);
    ui.cdRadioButton->setChecked(m_data.autoChangeDir);
    ui.cdRadioButton->blockSignals(false);
    
    ui.forcePushCheckBox->blockSignals(true);
    ui.forcePushCheckBox->setChecked(m_data.forcePush);
    ui.forcePushCheckBox->blockSignals(false);
    
    ui.cleanRPcheckBox->blockSignals(true);
    ui.cleanRPcheckBox->setChecked(m_data.cleanRestorePoints);
    ui.cleanRPcheckBox->blockSignals(false);


}

void QTSSH::activateCommandLineBox(bool state){
    ui.basePathLineEdit->setDisabled(true);
    if (state) {
        ui.serverToolsBox->setEnabled(true);
    }
    else
    {
        ui.serverToolsBox->setDisabled(true);
    }
}

void QTSSH::setProgressMessage(bool state, QString message) {

    if (state) {
        ui.processLabel->setEnabled(true);
    }
    else {
        ui.processLabel->setDisabled(true);
    }

    if (message != NULL)
        ui.processLabel->setText(message);

}

void QTSSH::activateAuthBox(bool state) {

    if (state) {
        ui.AuthBox->setEnabled(true);
    }
    else {
        ui.AuthBox->setDisabled(true);
    }
}

QString QTSSH::valideteSlash(QString value) {
    value = value.replace(" ", "");
    
    if (value[0] == "/") {
        value = value.remove(0, 1);
    }
    return value;
}


bool QTSSH::eventFilter(QObject* obj, QEvent* event)
{
    //up arrow 16777235
    //down arrow 16777237
    if (obj == ui.execCommandLineEdit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        
        switch (key->key())
        {

        case Qt::Key_Up: // up arrow button
            emit getPreviousCommandFromHistory();
            break;
        
        case Qt::Key_Down: // down arrow button
            emit getNextCommandFromHistory();
            break;

        case Qt::Key_Enter:
        case Qt::Key_Return:
            //emit execCommandChanged(ui.execCommandLineEdit->text());
            emit execButton();
            break;

        default:
            break;
        }
        
    }

    return QObject::eventFilter(obj, event);
}