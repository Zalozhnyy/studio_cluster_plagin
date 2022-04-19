
#ifndef CONSOLE_H
#define CONSOLE_H

#include <QStringList>
#include <QTextEdit>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenu>

class Console : public QTextEdit
{
    Q_OBJECT

    Q_PROPERTY(QColor cmdColor READ cmdColor WRITE setCmdColor)
    Q_PROPERTY(QColor errColor READ errColor WRITE setErrColor)
    Q_PROPERTY(QColor outColor READ outColor WRITE setOutColor)
    Q_PROPERTY(QColor completionColor READ completionColor WRITE setCompletionColor)

public:
    Console(QWidget* parent = nullptr);
    // set the prompt of the console
    void setPrompt(const QString& prompt, bool display = true);
    // executes the command and displays back its result
    bool execCommand(const QString& command, bool writeCommand = true, bool showPrompt = true, QString* result = nullptr);
    //clear & reset the console (useful sometimes)
    void clear();
    void reset();

    // type of console message
    enum class MsgType {
        Command,
        Output,
        Error
    };
    // show message in console
    void setMsgText(const QString& text, MsgType type = MsgType::Command, bool insert = false);

    // get/set command color
    QColor cmdColor() const { return m_cmdColor; }
    void setCmdColor(QColor c) { m_cmdColor = c; }

    // get/set error color
    QColor errColor() const { return m_errColor; }
    void setErrColor(QColor c) { m_errColor = c; }


    // get/set output color
    QColor outColor() const { return m_outColor; }
    void setOutColor(QColor c) { m_outColor = c; }

    // get/set completion color
    QColor completionColor() const { return m_completionColor; }
    void setCompletionColor(QColor c) { m_completionColor = c; }

    // get/set font
    void setFont(const QFont& f);
    QFont font() const { return currentFont(); }

    // get/set completer popup
    bool popupCompleter() const { return m_usePopupCompleter; }
    void setPopupCompleter(bool on) { m_usePopupCompleter = on; }

protected:
    //Implement paste with middle mouse button
    void mousePressEvent(QMouseEvent* ev) override;

    //execute a validated command (should be reimplemented and called at the end)
    //the return value of the function is the string result
    //res must hold back the return value of the command (0: passed; else: error)
    virtual QString interpretCommand(const QString& command, int* res);
    //give suggestions to autocomplete a command (should be reimplemented)
    //the return value of the function is the string list of all suggestions
    //the returned prefix is useful to complete "sub-commands"
    virtual QStringList suggestCommand(const QString& cmd, QString& prefix);

    //colors
    QColor m_cmdColor = QColor(0, 0, 0);
    QColor m_errColor = QColor(255, 0, 0);
    QColor m_outColor = QColor(1, 50, 32);
    QColor m_completionColor = QColor(0, 0, 255);

    // popup completer
    bool m_usePopupCompleter = true;
    // prev position
    int m_oldPosition = 0;
    // cached prompt length
    int m_promptLength = 0;
    // The prompt string
    QString m_prompt;
    // The commands history
    QStringList m_history;
    // Current history index (needed because afaik QStringList does not have such an index)
    int m_historyIndex = 0;
    //Holds the paragraph number of the prompt (useful for multi-line command handling)
    int m_promptParagraph = 0;

private:
    void dropEvent(QDropEvent* ev) override;
    void dragMoveEvent(QDragMoveEvent* ev) override;

    void keyPressEvent(QKeyEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;

    //Return false if the command is incomplete (e.g. unmatched braces)
    virtual bool isCommandComplete(const QString& command);
    //Get the command to validate
    QString getCurrentCommand();

    //Replace current command with a new one
    void replaceCurrentCommand(const QString& newCommand);

    //Test whether the cursor is in the edition zone
    bool isInEditionZone();
    bool isInEditionZone(const int& pos);

    //Test whether the selection is in the edition zone
    bool isSelectionInEditionZone();
    //Change paste behaviour
    void insertFromMimeData(const QMimeData* data);

public slots:
    //Contextual menu slots
    void cut();
    //void paste();
    void del();
    //displays the prompt
    void displayPrompt();

signals:
    //Signal emitted after that a command is executed
    void commandExecuted(const QString& command);

private:
    void handleTabKeyPress();
    void handleReturnKeyPress();
    bool handleBackspaceKeyPress();
    void handleUpKeyPress();
    void handleDownKeyPress();
    void setHome(bool select);
};

#endif // !CONSOLE_H
