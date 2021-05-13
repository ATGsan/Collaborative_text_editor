#include <fstream>

#include <QtWidgets>
#include <cctype>

#include "client.h"

ClientService::ClientService(std::shared_ptr<Channel> channel, std::string& file_path) :
    stub_(clientService::NewStub(channel)), file_name(file_path) {}

ClientService::ClientService() {}

std::string ClientService::initialize() {
    empty e;
    file_from_server reply;
    ClientContext context;
    // call rpc in server side
    Status status = stub_->initialize(&context, e, &reply);
    std::string ret ="";
    std::fstream file(file_name);
    for(int i = 0; i < reply.file_size(); i++) {
        file << reply.file(i) << std::endl;
        ret += reply.file(i);
    }
    last_op = reply.op_id();
    return ret;
}

void ClientService::OPs(OP_type operation, uint64_t pos, uint64_t line, char sym, uint32_t user_id) {
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

void ClientService::writeToFile() {
    empty e;
    last_executed_operation op;
    ClientContext context;
    Status status = stub_->writeToFile(&context, e, &op);
    last_op = op.op_id();
}

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

QString client::askFilename() {
    // спросить имя файла, дефолт output.txt
    bool ok;
    QString file = QInputDialog::getText(this, tr("Filename"),
                                         tr("Filename:"), QLineEdit::Normal,
                                         QDir::home().dirName(), &ok);
    if (!ok || file.isEmpty()) {
        file = "output.txt";
    }
    statusBar()->showMessage(file);
    return file;
}

client::client(std::string server_address, std::string file)
    : textEdit(new QPlainTextEdit), cursor(textEdit->textCursor())
{    
    // создать объект ClientService, от которого будут отправляться на сервер операции
    file = askFilename().toStdString();
    setCurrentFile(QString::fromUtf8(file.c_str()));
    this->service = ClientService(
            grpc::CreateChannel(server_address,
                                grpc::InsecureChannelCredentials()), file);

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



void client::receiveCursorsPositions() {
    for (int i = 0; i < CURSORS_QUANTITY; ++i) {
        cursorsPositions[i] = 0; // менять на значение, полученное с сервера
    }
    // получать новые позицие курсоров (может быть в векторе, может быть числами)
}

void client::highlightPosition(size_t pos, QColor color) {
    if (POS == 1) return;
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
    if (POS == 1) return;
    QTextCharFormat fmt;
    fmt.setBackground(Qt::white);

    QTextCursor cursor(textEdit->textCursor());
    int curPos = cursor.position();

    cursor.setPosition(pos, QTextCursor::MoveAnchor);
    cursor.setPosition(pos+1, QTextCursor::KeepAnchor);
    cursor.setCharFormat(fmt);

    cursor.setPosition(curPos);
}

QColor client::intToColor(int n) {
    return cursorColors[n % 4];
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
            QString res = this->textEdit->toPlainText();
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            qDebug() << "user " << CLIENT_ID << " at position " << POS << " at line " << LINE;
            if (keyEvent->key() == Qt::Key_Backspace) {
                service.OPs(OP_type::DELETE, POS, LINE, '\0', CLIENT_ID);
                res = QString::fromUtf8(service.initialize().c_str());
                if (POS - 1 >= 0) POS--;
                else if (LINE -1 >= 0) POS = maintext[--LINE].size();
            }
            // if (isalpha(keyEvent->key()) || isdigit(keyEvent->key()) || isspace(keyEvent->key())) {
            else if (keyEvent->key() >= Qt::Key_Space && keyEvent->key() <= Qt::Key_Dead_Longsolidusoverlay && keyEvent->key() != Qt::Key_Backspace && keyEvent->key() != Qt::Key_Delete) {
                service.OPs(OP_type::INSERT, POS, LINE, keyEvent->key(), CLIENT_ID);
                res = QString::fromUtf8(service.initialize().c_str());
                POS++;
                qDebug() << "printed " << keyEvent->text();
            } else if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) { //?????????????????
            //} else if (keyEvent->key() == Qt::Key_Space) {
                qDebug() << "newline";
                LINE++;
                POS = 0;
            }

            markCursors();
            this->textEdit->setPlainText(res);
            std::cout << res.toStdString() << std::endl;
            std::cout << "set cursor pos to " << POS << std::endl;
            this->textEdit->textCursor().setPosition(POS);

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

void client::undo() {
    service.OPs(OP_type::UNDO, POS, LINE, '\0', CLIENT_ID);
    QString res = QString::fromUtf8(service.initialize().c_str());
    this->textEdit->setPlainText(res);
    this->textEdit->textCursor().setPosition(POS);
}

void client::redo() {
    service.OPs(OP_type::REDO, POS, LINE, '\0', CLIENT_ID);
    QString res = QString::fromUtf8(service.initialize().c_str());
    this->textEdit->setPlainText(res);
    this->textEdit->textCursor().setPosition(POS);
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

    const QIcon undoIcon = QIcon::fromTheme("document-undo", QIcon(":/images/undo.png"));
    QAction *undoAct = new QAction(undoIcon, tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    undoAct->setStatusTip(tr("Undo"));

    const QIcon redoIcon = QIcon::fromTheme("document-redo", QIcon(":/images/redo.png"));
    QAction *redoAct = new QAction(redoIcon, tr("&Redo"), this);
    redoAct->setShortcuts(QKeySequence::Redo);
    redoAct->setStatusTip(tr("Redo"));


    fileMenu->addSeparator();

    const QIcon exitIcon = QIcon::fromTheme("application-exit");
    QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QToolBar *editToolBar = addToolBar(tr("Edit"));

    connect(undoAct, &QAction::triggered, textEdit, &QPlainTextEdit::undo);
    editMenu->addAction(undoAct);
    editToolBar->addAction(undoAct);

    connect(redoAct, &QAction::triggered, textEdit, &QPlainTextEdit::redo);
    editMenu->addAction(redoAct);
    editToolBar->addAction(redoAct);

    menuBar()->addSeparator();

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &client::about);
    aboutAct->setStatusTip(tr("Show the application's About box"));

    QAction *aboutQtAct = helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
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
        shownName = "output.txt";
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
