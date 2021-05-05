/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <fstream>

#include <QtWidgets>
#include <cctype>

#include <grpcpp/grpcpp.h>
#include "operationTransportation.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using operationTransportation::clientService;
using operationTransportation::editor_request;
using operationTransportation::empty;
using operationTransportation::file_from_server;
using operationTransportation::OP_type;
using operationTransportation::last_executed_operation;

#include "client.h"

// main class for client
class ClientService {
private:
    std::unique_ptr<clientService::Stub> stub_;
    std::string file_name;
    uint64_t last_op;
public:
    // default constructor with initialize of channel between server and client
    ClientService(std::shared_ptr<Channel> channel, std::string& file_path) :
            stub_(clientService::NewStub(channel)), file_name(file_path) {}

    // command to initialize file in client side
    void initialize() {
        empty e;
        file_from_server reply;
        ClientContext context;
        // call rpc in server side
        Status status = stub_->initialize(&context, e, &reply);
        std::fstream file(file_name);
        for(int i = 0; i < reply.file_size(); i++) {
            file << reply.file(i) << std::endl;
        }
        last_op = reply.op_id();
    }

    // main method to send operation to server
    void OPs(OP_type operation, uint64_t pos, uint64_t line, char sym, uint32_t user_id) {
        last_executed_operation op;
        editor_request request;
        request.set_op(operation);
        request.set_pos(pos);
        request.set_line(line);
        request.set_sym(sym);
        request.set_user_id(user_id);
        request.set_op_id(last_op);
        ClientContext context;
        Status status = stub_->sendOP(&context, request, &op);
        last_op = op.op_id();
    }

    // call to write file from vector of strings to file in server file
    void writeToFile() {
        empty e;
        last_executed_operation op;
        ClientContext context;
        Status status = stub_->writeToFile(&context, e, &op);
        last_op = op.op_id();
    }
};

void RunClient(std::string& file, std::string& server_address) {
    ClientService client(
            grpc::CreateChannel(server_address,
                                grpc::InsecureChannelCredentials()), file);
}

size_t receiveId() {
    return 0;
    // получать свой (нового пользователя) id с сервера
}

size_t receiveCursorsQuantity() {
    return 1;
    // получать начальное количество курсоров с сервера
}

static int POS=0, LINE=0, CLIENT_ID=receiveId(), CURSORS_QUANTITY = receiveCursorsQuantity();

void client::receiveCursorsPositions() {
    for (int i = 0; i < CURSORS_QUANTITY; ++i) {
        cursorsPositions[i] = 0; // менять на значение, полученное с сервера
    }
    // получать новые позицие курсоров (может быть в векторе, может быть числами)
}

client::client()
    : textEdit(new QPlainTextEdit), cursor(textEdit->textCursor())
{
    setCentralWidget(textEdit);

    createActions();
    createStatusBar();

    readSettings();

    connect(textEdit->document(), &QTextDocument::contentsChanged,
            this, &client::documentWasModified);

    textEdit->installEventFilter(this);

#ifndef QT_NO_SESSIONMANAGER
    QGuiApplication::setFallbackSessionManagementEnabled(false);
    connect(qApp, &QGuiApplication::commitDataRequest,
            this, &client::commitData);
#endif

    setCurrentFile(QString());
    setUnifiedTitleAndToolBarOnMac(true);
//  QMap с позициями других курсоров, свой уже в глобальной константе. Сначала все курсоры находятся в начале файла
    for (size_t i = 0; i < CURSORS_QUANTITY; ++i) {
        cursorsPositions[i] = 0;
    }
}

void client::highlightPosition(size_t pos, QColor color) {
    QTextCharFormat fmt;
    fmt.setBackground(color);

    QTextCursor cursor(textEdit->textCursor());
    int curPos = cursor.position();

    cursor.setPosition(pos, QTextCursor::MoveAnchor);
    cursor.setPosition(pos+1, QTextCursor::KeepAnchor);
    cursor.setCharFormat(fmt);

    cursor.setPosition(curPos);
}

void client::unhighlightPosition(size_t pos) {
    QTextCharFormat fmt;
    fmt.setBackground(Qt::white);

    QTextCursor cursor(textEdit->textCursor());
    int curPos = cursor.position();

    cursor.setPosition(pos, QTextCursor::MoveAnchor);
    cursor.setPosition(pos+1, QTextCursor::KeepAnchor);
    cursor.setCharFormat(fmt);

    cursor.setPosition(curPos);
}

QColor intToColor(int n) {
    return QColor(100+5*n, 100+10*n, 100+15*n);
    // менять цвет в зависимости от номера или сделать вектор цветов для курсоров
}

void client::markCursors() {
    for (int i = 0; i < cursorsPositions.size(); ++i) {
        unhighlightPosition(cursorsPositions[i]);
    }
    receiveCursorsPositions();
    for (int i = 0; i < cursorsPositions.size(); ++i) {
        highlightPosition(cursorsPositions[i], intToColor(i));
    }
}

QVector<QString> textToVector(const QString& text) {
    QVector<QString> vec;
    QString newLine;
    QTextStream ss(&newLine);
    int i = 0;
    while (i < text.size()) {
        if (text.at(i) == '\n') {
            vec.append(newLine);
            newLine = "";
        } else {
            ss << text.at(i);
        }
        i++;
    }
    return vec;
}

QString vecToText(const QVector<QString>& vec) {
    QString text;
    QTextStream ts(&text);
    for (auto s : vec) {
        ts << s << "\n";
    }
    return text;
}

bool client::eventFilter(QObject *obj, QEvent *event) {
    if (obj == textEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            qDebug() << "user " << CLIENT_ID << " at position " << POS << " at line " << LINE;
            // POS = textEdit->textCursor().position();
            if (keyEvent->key() == Qt::Key_Backspace) {
                //OPs(OP_type::DELETE, POS, LINE, NULL, CLIENT_ID);
                if (POS - 1 >= 0) POS--;
                else if (LINE -1 >= 0) POS = maintext[--LINE].size();
            }
            // if (isalpha(keyEvent->key()) || isdigit(keyEvent->key()) || isspace(keyEvent->key())) {
            else if (keyEvent->key() > Qt::Key_Space && keyEvent->key() <= Qt::Key_Dead_Longsolidusoverlay && keyEvent->key() != Qt::Key_Backspace && keyEvent->key() != Qt::Key_Delete) {
                POS++;
                qDebug() << "printed " << keyEvent->text();
                // OPs(OP_type::INSERT, POS, LINE, keyEvent->text(), CLIENT_ID)
            } else if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) { //?????????????????
            //} else if (keyEvent->key() == Qt::Key_Space) {
                qDebug() << "newline";
                LINE++;
                POS = 0;
                maintext = textToVector(textEdit->toPlainText());
                for (auto s : maintext) {
                    qDebug() << s;
                }
                //QString text = vecToText(maintext);
                //qDebug() << text;
            }

            markCursors();

            return QMainWindow::eventFilter(obj, event);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void client::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void client::newFile()
{
    if (maybeSave()) {
        textEdit->clear();
        setCurrentFile(QString());
    }
}

bool client::save()
{
    if (curFile.isEmpty()) {
        return saveAs();
    } else {
        return saveFile(curFile);
    }
}

bool client::saveAs()
{
    QFileDialog dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return saveFile(dialog.selectedFiles().first());
}

void client::about()
{
   QMessageBox::about(this, tr("About Application"),
            tr("This is a collaborative text editor that allows to edit a textfile simultaniuously."
               "The editor uses RPCs to communicate with the server."));
}

void client::documentWasModified()
{
    setWindowModified(textEdit->document()->isModified());   
}

void client::createActions()
{

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QToolBar *fileToolBar = addToolBar(tr("File"));
    const QIcon newIcon = QIcon::fromTheme("document-new", QIcon(":/images/new.png"));
    QAction *newAct = new QAction(newIcon, tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Create a new file"));
    connect(newAct, &QAction::triggered, this, &client::newFile);
    fileMenu->addAction(newAct);
    fileToolBar->addAction(newAct);

    const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":/images/save.png"));
    QAction *saveAct = new QAction(saveIcon, tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, &QAction::triggered, this, &client::save);
    fileMenu->addAction(saveAct);
    fileToolBar->addAction(saveAct);

    const QIcon saveAsIcon = QIcon::fromTheme("document-save-as");
    QAction *saveAsAct = fileMenu->addAction(saveAsIcon, tr("Save &As..."), this, &client::saveAs);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));


    fileMenu->addSeparator();

    const QIcon exitIcon = QIcon::fromTheme("application-exit");
    QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QToolBar *editToolBar = addToolBar(tr("Edit"));
#ifndef QT_NO_CLIPBOARD
    const QIcon cutIcon = QIcon::fromTheme("edit-cut", QIcon(":/images/cut.png"));
    QAction *cutAct = new QAction(cutIcon, tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));
    connect(cutAct, &QAction::triggered, textEdit, &QPlainTextEdit::cut);
    editMenu->addAction(cutAct);
    editToolBar->addAction(cutAct);

    const QIcon copyIcon = QIcon::fromTheme("edit-copy", QIcon(":/images/copy.png"));
    QAction *copyAct = new QAction(copyIcon, tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(copyAct, &QAction::triggered, textEdit, &QPlainTextEdit::copy);
    editMenu->addAction(copyAct);
    editToolBar->addAction(copyAct);

    const QIcon pasteIcon = QIcon::fromTheme("edit-paste", QIcon(":/images/paste.png"));
    QAction *pasteAct = new QAction(pasteIcon, tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(pasteAct, &QAction::triggered, textEdit, &QPlainTextEdit::paste);
    editMenu->addAction(pasteAct);
    editToolBar->addAction(pasteAct);

    menuBar()->addSeparator();

#endif // !QT_NO_CLIPBOARD

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &client::about);
    aboutAct->setStatusTip(tr("Show the application's About box"));


    QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));

#ifndef QT_NO_CLIPBOARD
    cutAct->setEnabled(false);
    copyAct->setEnabled(false);
    connect(textEdit, &QPlainTextEdit::copyAvailable, cutAct, &QAction::setEnabled);
    connect(textEdit, &QPlainTextEdit::copyAvailable, copyAct, &QAction::setEnabled);
#endif // !QT_NO_CLIPBOARD
}

void client::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void client::readSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
}

void client::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
}

bool client::maybeSave()
{
    if (!textEdit->document()->isModified())
        return true;
    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("The document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
    case QMessageBox::Save:
        return save();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

bool client::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(fileName),
                                  file.errorString()));
        return false;
    }

    QTextStream out(&file);
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    out << textEdit->toPlainText();
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif

    setCurrentFile(fileName);
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

void client::setCurrentFile(const QString &fileName)
{
    curFile = fileName;
    textEdit->document()->setModified(false);
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty())
        shownName = "untitled.txt";
    setWindowFilePath(shownName);
}

QString client::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

void client::commitData(QSessionManager &manager)
{
    if (manager.allowsInteraction()) {
        if (!maybeSave())
            manager.cancel();
    } else {
        // Non-interactive: save without asking
        if (textEdit->document()->isModified())
            save();
    }
}
