
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QVBoxLayout>
#include <QApplication>
#include <QScrollBar>
#include <QDesktopWidget>
#include <QScreen>

#include "console.h"

namespace {
    /**
     * returns a common word of the given list
     *
     * @param list String list
     *
     * @return common word in the given string.
     */
    QString getCommonWord(const QStringList& list)
    {
        // get minimum length
        int min_len = 0;
        for (auto& it : list) {
            const int len = it.size();

            if (len < min_len) {
                min_len = len;
            }
        }

        // common part
        QString common;
        // check each char in strings
        int col = 0;
        while (col < min_len) {
            QChar ch = list.front()[col];
            bool cont = true;

            for (auto& it : list) {
                if (ch != it[col]) {
                    cont = false;
                    break;
                }
            }

            if (!cont) {
                break;
            }

            common.push_back(ch);
            ++col;
        }

        return common;
    }
}

Console::Console(QWidget* parent)
    : QTextEdit(parent)
{
    QPalette palette = QApplication::palette();
    setCmdColor(palette.text().color());

    //Disable undo/redo
    setUndoRedoEnabled(false);

    //Disable context menu
    //This menu is useful for undo/redo, cut/copy/paste, del, select all,
    // see function QConsole::contextMenuEvent(...)
    //setContextMenuPolicy(Qt::NoContextMenu);

    //resets the console
    reset();
    const int tabwidth = QFontMetrics(currentFont()).horizontalAdvance('a') * 4;
    setTabStopDistance(tabwidth);
}

void Console::clear()
{
    QTextEdit::clear();
}

void Console::reset()
{
    clear();

    append("");

    //init attributes
    m_historyIndex = 0;
    m_history.clear();
}

void Console::setPrompt(const QString& newPrompt, bool display)
{
    m_prompt = newPrompt;
    m_promptLength = m_prompt.length();
    //display the new prompt
    if (display) {
        displayPrompt();
    }
}

void Console::displayPrompt()
{
    //Prevent previous text displayed to be undone
    setUndoRedoEnabled(false);
    //displays the prompt
    setTextColor(m_cmdColor);
    QTextCursor cur = textCursor();
    cur.insertText(m_prompt);
    cur.movePosition(QTextCursor::EndOfLine);
    setTextCursor(cur);
    //Saves the paragraph number of the prompt
    m_promptParagraph = cur.blockNumber();

    //Enable undo/redo for the actual command
    setUndoRedoEnabled(true);
}

void Console::setFont(const QFont& f)
{
    QTextCharFormat format;
    QTextCursor oldCursor = textCursor();
    format.setFont(f);
    selectAll();
    textCursor().setBlockCharFormat(format);
    setCurrentFont(f);
    setTextCursor(oldCursor);
}

QStringList Console::suggestCommand(const QString&, QString& prefix)
{
    prefix = "";
    return QStringList();
}

void Console::handleTabKeyPress()
{

}

void Console::handleReturnKeyPress()
{
    //Get the command to validate
    QString command = getCurrentCommand();
    //execute the command and get back its text result and its return value
    if (isCommandComplete(command)) {
        execCommand(command, false, false);
    } else {
        append("");
        moveCursor(QTextCursor::EndOfLine);
    }
}

bool Console::handleBackspaceKeyPress()
{
    QTextCursor cur = textCursor();
    const int col = cur.columnNumber();
    const int blk = cur.blockNumber();

    if (blk == m_promptParagraph && col == m_promptLength) {
        return true;
    }

    return false;
}

void Console::handleUpKeyPress()
{
    if (!m_history.isEmpty()) {
        const QString command = getCurrentCommand();

        do {
            if (m_historyIndex) {
                m_historyIndex--;
            } else {
                break;
            }
        } while (m_history[m_historyIndex] == command);

        replaceCurrentCommand(m_history[m_historyIndex]);
    }
}

void Console::handleDownKeyPress()
{
    if (!m_history.isEmpty()) {
        const QString command = getCurrentCommand();

        do {
            if (++m_historyIndex >= m_history.size())
            {
                m_historyIndex = m_history.size() - 1;
                break;
            }
        } while (m_history[m_historyIndex] == command);

        replaceCurrentCommand(m_history[m_historyIndex]);
    }
}

void Console::setHome(bool select)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock, select ? QTextCursor::KeepAnchor :
        QTextCursor::MoveAnchor);
    if (textCursor().blockNumber() == m_promptParagraph)
    {
        cursor.movePosition(QTextCursor::Right, select ? QTextCursor::KeepAnchor :
            QTextCursor::MoveAnchor,
            m_promptLength);
    }
    setTextCursor(cursor);
}

void Console::keyPressEvent(QKeyEvent* ev)
{
    //If the user wants to copy or cut outside
    //the editing area we perform a copy
    if (textCursor().hasSelection())
    {
        if (ev->modifiers() == Qt::CTRL)
        {
            if (ev->matches(QKeySequence::Cut))
            {
                ev->accept();
                if (!isInEditionZone())
                {
                    copy();
                } else
                {
                    cut();
                }
                return;
            } else if (ev->matches(QKeySequence::Copy))
            {
                ev->accept();
                copy();
            } else
            {
                QTextEdit::keyPressEvent(ev);
                return;
            }
        }
    }
    // control is pressed
    if ((ev->modifiers() & Qt::ControlModifier) && (ev->key() == Qt::Key_C))
    {
        if (isSelectionInEditionZone())
        {
            //If Ctrl + C pressed, then undo the current commant
            //append("");
            //displayPrompt();

            //(Thierry Belair:)I humbly suggest that ctrl+C copies the text, as is expected,
            //and indicated in the contextual menu
            copy();
            return;
        }

    } else {
        switch (ev->key()) {
        case Qt::Key_Tab:
            if (isSelectionInEditionZone())
            {
                handleTabKeyPress();
            }
            return;

        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (isSelectionInEditionZone())
            {
                handleReturnKeyPress();
            }
            // ignore return key
            return;

        case Qt::Key_Backspace:
            if (handleBackspaceKeyPress() || !isSelectionInEditionZone())
                return;
            break;

        case Qt::Key_Home:
            setHome(ev->modifiers() & Qt::ShiftModifier);
        case Qt::Key_Down:
            if (isInEditionZone())
            {
                handleDownKeyPress();
            }
            return;
        case Qt::Key_Up:
            if (isInEditionZone())
            {
                handleUpKeyPress();
            }
            return;

            //Default behaviour
        case Qt::Key_End:
        case Qt::Key_Left:
        case Qt::Key_Right:
            break;

        default:
            if (textCursor().hasSelection()) {
                if (!isSelectionInEditionZone())
                {
                    moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
                }
                break;
            } else
            { //no selection
                //when typing normal characters,
                //make sure the cursor is positionned in the
                //edition zone
                if (!isInEditionZone())
                {
                    moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
                }
            }
        } //end of switch
    } //end of else : no control pressed

    QTextEdit::keyPressEvent(ev);
}

QString Console::getCurrentCommand()
{
    QTextCursor cursor = textCursor();    //Get the current command: we just remove the prompt
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, m_promptLength);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString command = cursor.selectedText();
    cursor.clearSelection();
    return command;
}

void Console::replaceCurrentCommand(const QString& newCommand)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, m_promptLength);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.insertText(newCommand);
}

bool Console::isCommandComplete(const QString&)
{
    return true;
}

bool Console::isInEditionZone()
{
    const int para = textCursor().blockNumber();
    const int index = textCursor().columnNumber();
    return (para > m_promptParagraph) || ((para == m_promptParagraph) && (index >= m_promptLength));
}

bool Console::isInEditionZone(const int& pos)
{
    QTextCursor cur = textCursor();
    cur.setPosition(pos);
    const int para = cur.blockNumber();
    const int index = cur.columnNumber();
    return (para > m_promptParagraph) || ((para == m_promptParagraph) && (index >= m_promptLength));
}

bool Console::isSelectionInEditionZone()
{
    QTextCursor cursor(document());
    int range[2];

    range[0] = textCursor().selectionStart();
    range[1] = textCursor().selectionEnd();
    for (int i = 0; i < 2; i++)
    {
        cursor.setPosition(range[i]);
        int para = cursor.blockNumber();
        int index = cursor.columnNumber();
        if ((para <= m_promptParagraph) && ((para != m_promptParagraph) || (index < m_promptLength)))
        {
            return false;
        }
    }
    return true;
}

QString Console::interpretCommand(const QString& command, int* res)
{
    //update the history and its index
    QString modifiedCommand = command;
    modifiedCommand.replace("\n", "\\n");
    m_history.append(modifiedCommand);
    m_historyIndex = m_history.size();
    //emit the commandExecuted signal
    emit commandExecuted(modifiedCommand);
    return "";
}

//execCommand(QString) executes the command and displays back its result
bool Console::execCommand(const QString& command, bool writeCommand,
    bool showPrompt, QString* result)
{
    //Display the prompt with the command first
    if (writeCommand)
    {
        if (getCurrentCommand() != "")
        {
            append("");
            displayPrompt();
        }
        textCursor().insertText(command);
    }
    //execute the command and get back its text result and its return value
    int res = 0;
    QString strRes = interpretCommand(command, &res);
    //According to the return value, display the result either in red or in blue
    if (res == 0)
        setTextColor(m_outColor);
    else
        setTextColor(m_errColor);

    if (result) {
        *result = strRes;
    }

    if (!(strRes.isEmpty() || strRes.endsWith("\n"))) {
        strRes.append("\n");
    }

    append(strRes);
    moveCursor(QTextCursor::End);
    //Display the prompt again
    if (showPrompt) {
        displayPrompt();
    }

    return !res;
}

void Console::setMsgText(const QString& text, MsgType type, bool insert)
{
    // set text color
    switch (type) {
    case MsgType::Command:
        setTextColor(m_cmdColor);
        break;

    case MsgType::Output:
        setTextColor(m_outColor);
        break;

    case MsgType::Error:
        setTextColor(m_errColor);
        break;
    }

    // set text
    if (insert) {
        // move cursor
        moveCursor(QTextCursor::End);
        insertPlainText(text);
    } else {
        append(text);
    }
    // move cursor
    moveCursor(QTextCursor::End);
}

//Change paste behaviour
void Console::insertFromMimeData(const QMimeData* source)
{
    if (isSelectionInEditionZone())
    {
        QTextEdit::insertFromMimeData(source);
    }
}

//Implement paste with middle mouse button
void Console::mousePressEvent(QMouseEvent* event)
{
    m_oldPosition = textCursor().position();
    if (event->button() == Qt::MidButton)
    {
        copy();
        QTextCursor cursor = cursorForPosition(event->pos());
        setTextCursor(cursor);
        paste();
        return;
    }

    QTextEdit::mousePressEvent(event);
}

//Redefinition of the dropEvent to have a copy paste
//instead of a cut paste when copying out of the
//editable zone
void Console::dropEvent(QDropEvent* event)
{
    if (!isInEditionZone())
    {
        //Execute un drop a drop at the old position
        //if the drag started out of the editable zone
        QTextCursor cur = textCursor();
        cur.setPosition(m_oldPosition);
        setTextCursor(cur);
    }
    //Execute a normal drop
    QTextEdit::dropEvent(event);
}

void Console::dragMoveEvent(QDragMoveEvent* event)
{
    //Get a cursor for the actual mouse position
    QTextCursor cur = textCursor();
    cur.setPosition(cursorForPosition(event->pos()).position());

    if (!isInEditionZone(cursorForPosition(event->pos()).position()))
    {
        //Ignore the event if out of the editable zone
        event->ignore(cursorRect(cur));
    } else
    {
        //Accept the event if out of the editable zone
        event->accept(cursorRect(cur));
    }
}

void Console::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = new QMenu(this);

    QAction* undo = new QAction(tr("Undo"), this);
    undo->setShortcut(QKeySequence::Undo);
    QAction* redo = new QAction(tr("Redo"), this);
    redo->setShortcut(QKeySequence::Redo);
    QAction* cut = new QAction(tr("Cut"), this);
    cut->setShortcut(QKeySequence::Cut);
    QAction* copy = new QAction(tr("Copy"), this);
    copy->setShortcut(QKeySequence::Copy);
    QAction* paste = new QAction(tr("Paste"), this);
    paste->setShortcut(QKeySequence::Paste);
    QAction* del = new QAction(tr("Delete"), this);
    del->setShortcut(QKeySequence::Delete);
    QAction* selectAll = new QAction(tr("Select All"), this);
    selectAll->setShortcut(QKeySequence::SelectAll);

    menu->addAction(undo);
    menu->addAction(redo);
    menu->addSeparator();
    menu->addAction(cut);
    menu->addAction(copy);
    menu->addAction(paste);
    menu->addAction(del);
    menu->addSeparator();
    menu->addAction(selectAll);

    connect(undo, &QAction::triggered, this, &Console::undo);
    connect(redo, &QAction::triggered, this, &Console::redo);
    connect(cut, &QAction::triggered, this, &Console::cut);
    connect(copy, &QAction::triggered, this, &Console::copy);
    connect(paste, &QAction::triggered, this, &Console::paste);
    connect(del, &QAction::triggered, this, &Console::del);
    connect(selectAll, &QAction::triggered, this, &Console::selectAll);

    menu->exec(event->globalPos());

    delete menu;
}

void Console::cut()
{
    //Cut only in the editing zone,
    //perfom a copy otherwise
    if (isInEditionZone())
    {
        QTextEdit::cut();
        return;
    }

    QTextEdit::copy();
}

void Console::del()
{
    //Delete only in the editing zone
    if (isInEditionZone())
    {
        textCursor().movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        textCursor().deleteChar();
    }
}
