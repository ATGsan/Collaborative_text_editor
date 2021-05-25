#include <fstream>

#include <QtWidgets>
#include <cctype>
#include "client.h"

EOpVector::EOpVector() : size(0), pos(0) {}

size_t EOpVector::get_size() const {
    return size;
}
size_t EOpVector::get_pos() const {
    return pos;
}
size_t EOpVector::set_size(size_t arg) {
    size = arg;
    return size;
}
size_t EOpVector::set_pos(size_t arg) {
    pos = arg;
    return pos;
}
void EOpVector::add(request op) {
    if(pos != size) {
        executed_operations.erase(executed_operations.begin() + pos, executed_operations.end());
        size = pos;
    }
    if(size != 64) {
        executed_operations.push_back(op);
        ++size;
        ++pos;
    } else {
        executed_operations.push_back(op);
        executed_operations.pop_front();
    }
}

request EOpVector::get_for_undo() {
    if(pos == 0) {
        request a;
        a.op = operationTransportation::INVALID;
        return a;
    }
    auto a = *(executed_operations.begin() + --pos);
    switch (a.op) {
        case operationTransportation::INSERT :
            a.op = operationTransportation::DELETE;
            break;
        case operationTransportation::DELETE :
            a.op = operationTransportation::INSERT;
            break;
        case operationTransportation::ADD_LINE :
            a.op = operationTransportation::DEL_LINE;
            break;
        case operationTransportation::DEL_LINE :
            a.op = operationTransportation::ADD_LINE;
            break;
        default:
            a.op = operationTransportation::INVALID;
            break;
    }
    return a;
}

request EOpVector::get_for_redo() {
    if(pos == size) {
        request a;
        a.op = operationTransportation::INVALID;
        return a;
    }
    auto a = *(executed_operations.begin() + pos++);
    return a;
}

ClientService::ClientService(std::shared_ptr<Channel> channel, std::string& file_path) :
    stub_(clientService::NewStub(channel)), file_name(file_path) {}

ClientService::ClientService() {}

std::string ClientService::initialize() {
    empty e;
    file_from_server reply;
    ClientContext context;
    // call rpc in server side
    Status status = stub_->initialize(&context, e, &reply);
    std::string ret = "";
    std::fstream file(file_name);
    text_vec.clear();
    for(int i = 0; i < reply.file_size() - 1 ; i++) {
        file << reply.file(i) << std::endl;
        ret += reply.file(i) + "\n";
        text_vec.push_back(reply.file(i));
    }
    ret += reply.file(reply.file_size() - 1);
    last_op = reply.op_id();
    return ret;
}

request ClientService::OPs(OP_type operation, uint64_t pos, uint64_t line, char sym, uint32_t user_id) {
    request req;
    last_executed_operation op;
    editor_request request;
    request.set_op(operation);
    request.set_pos(pos);
    request.set_line(line);
    request.set_sym(sym);
    request.set_user_id(user_id);
    request.set_op_id(last_op);
    req.op = operation;
    req.pos = pos;
    req.line = line;
    req.sym = sym;
    req.user_id = user_id;
    req.op_id = last_op;
    ClientContext context;
    Status status = stub_->sendOP(&context, request, &op);
    last_op = op.op_id();
    return req;
}

void ClientService::writeToFile() {
    empty e;
    last_executed_operation op;
    ClientContext context;
    Status status = stub_->writeToFile(&context, e, &op);
    last_op = op.op_id();
}

pos_line ClientService::get_pos() {
    ClientContext context;
    user_message user;
    user.set_user_id(u_id);
    pos_message pos;
    stub_->get_pos(&context, user, &pos);
    pos_line ret;
    ret.line = pos.line();
    ret.pos = pos.pos();
    return ret;
}

int ClientService::get_user_id() {
    ClientContext context;
    empty e;
    user_message um;
    stub_->get_u_id(&context, e, &um);
    u_id = um.user_id();
    return u_id;
}

static int POS=0, LINE=0, CLIENT_ID;

client::client(std::string server_address, std::string file)
    : textEdit(new QPlainTextEdit), cursor(textEdit->textCursor())
{    
    // создать объект ClientService, от которого будут отправляться на сервер операции
    this->service = ClientService(
            grpc::CreateChannel(server_address,
                                grpc::InsecureChannelCredentials()), file);
    CLIENT_ID = service.get_user_id();
    if (!CLIENT_ID) {
        service.OPs(OP_type::ADD_LINE, 0, 0, '\n', CLIENT_ID);
        POS = 0;
        service.OPs(OP_type::MOVE, 0, 0, '\0', CLIENT_ID);
        auto cursor = this->textEdit->textCursor();
        cursor.setPosition(0);
        this->textEdit->setTextCursor(cursor);
    }
    this->last_executed_operations = EOpVector();

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
    service.get_user_id();
}

int vecToTextPos(const std::vector<std::string>& vec) {
    int pos = 0;
    for (int i = 0; i < LINE; ++i) {
        pos += vec[i].size()+1;
    }
    pos += POS;
    return pos;
}

bool client::eventFilter(QObject *obj, QEvent *event) {
    pos_line cur = service.get_pos();
    POS = cur.pos;
    LINE = cur.line;

    if (obj == textEdit) {
        if (event->type() == QEvent::KeyPress) {
            QString res = this->textEdit->toPlainText();
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            std::cout << "user " << CLIENT_ID << " at position " << POS << " at line " << LINE << std::endl;
            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
                 std::cout << "newline" << std::endl;

                 auto cursor = this->textEdit->textCursor();

                 last_executed_operations.add(service.OPs(OP_type::ADD_LINE, POS, LINE, '\n', CLIENT_ID));
                 LINE++;
                 POS = 0;
                 service.OPs(OP_type::MOVE, POS, LINE, '\0', CLIENT_ID);
                 cursor.setPosition(vecToTextPos(service.text_vec));
                 this->textEdit->setTextCursor(cursor);

                 res = QString::fromUtf8(service.initialize().c_str());
            } else if (keyEvent->key() == Qt::Key_Backspace) {
                if (LINE == 0) {
                    if (POS != 0) {
                        last_executed_operations.add(service.OPs(OP_type::DELETE, POS, LINE, this->textEdit->toPlainText().toStdString()[POS - 1], CLIENT_ID));
                        res = QString::fromUtf8(service.initialize().c_str());
                        POS--;
                    }
                } else {
                    if (POS == 0) {
                        last_executed_operations.add(service.OPs(OP_type::DEL_LINE, POS, LINE, '\n', CLIENT_ID));
                        LINE--;
                        POS = service.text_vec.back().size();
                    } else {
                        last_executed_operations.add(service.OPs(OP_type::DELETE, POS, LINE, this->textEdit->toPlainText().toStdString()[POS - 1], CLIENT_ID));
                        POS--;
                    }
                }
                res = QString::fromUtf8(service.initialize().c_str());
            } else if (keyEvent->key() == Qt::Key_Delete) {
                if (LINE == service.text_vec.size()) {

                } else if (LINE == service.text_vec.size()-1) {
                    if (POS < service.text_vec.back().size()) {
                        last_executed_operations.add(service.OPs(OP_type::DELETE, POS+1, LINE, this->textEdit->toPlainText().toStdString()[POS+1], CLIENT_ID));
                    }
                } else {
                    if (POS < service.text_vec.back().size()) {
                        last_executed_operations.add(service.OPs(OP_type::DELETE, POS+1, LINE, this->textEdit->toPlainText().toStdString()[POS+1], CLIENT_ID));
                    } else {
                        last_executed_operations.add(service.OPs(OP_type::DEL_LINE, 0, LINE+1, '\n', CLIENT_ID));
                    }
                }
                res = QString::fromUtf8(service.initialize().c_str());
            } else if (keyEvent->key() == Qt::Key_Left) {
                if (LINE == 0) {
                    if (POS != 0) {
                        POS--;
                    }
                } else {
                    if (POS == 0) {
                        LINE--;
                        POS = service.text_vec[LINE].size();
                    } else {
                        POS--;
                    }
                }
                service.OPs(OP_type::MOVE, POS, LINE, '\0', CLIENT_ID);

                auto cursor = this->textEdit->textCursor();
                cursor.setPosition(vecToTextPos(service.text_vec));
                this->textEdit->setTextCursor(cursor);
                std::cout << res.toStdString() << std::endl;
                std::cout << "cursor pos = " << this->textEdit->textCursor().position() << std::endl;

                return true;
            } else if (keyEvent->key() == Qt::Key_Right) {
                if (this->textEdit->toPlainText().isEmpty()) {
                    return true;
                }

                if (LINE == service.text_vec.size()-1) {
                    if (POS < service.text_vec[LINE].size()) {
                        POS++;
                    }

                } else {
                    if (POS == service.text_vec[LINE].size()) {
                        LINE++;
                        POS = 0;
                    } else if (POS < service.text_vec[LINE].size()) {
                        POS++;
                    }
                }
                service.OPs(OP_type::MOVE, POS, LINE, '\0', CLIENT_ID);

                auto cursor = this->textEdit->textCursor();
                cursor.setPosition(vecToTextPos(service.text_vec));
                this->textEdit->setTextCursor(cursor);
                std::cout << res.toStdString() << std::endl;
                std::cout << "cursor pos = " << this->textEdit->textCursor().position() << std::endl;

                return true;
            } else if (keyEvent->key() == Qt::Key_Down) {
                if (LINE < service.text_vec.size()) {
                    LINE++;
                    if (service.text_vec[LINE].size() < POS) {
                        POS = service.text_vec[LINE].size();
                    }
                }
                service.OPs(OP_type::MOVE, POS, LINE, '\0', CLIENT_ID);

                auto cursor = this->textEdit->textCursor();
                cursor.setPosition(vecToTextPos(service.text_vec));
                this->textEdit->setTextCursor(cursor);
                std::cout << res.toStdString() << std::endl;
                std::cout << "cursor pos = " << this->textEdit->textCursor().position() << std::endl;

                return true;
            } else if (keyEvent->key() == Qt::Key_Up) {
                if (LINE != 0) {
                    LINE--;
                    if (service.text_vec[LINE].size() < POS) {
                        POS = service.text_vec[LINE].size();
                    }
                }
                service.OPs(OP_type::MOVE, POS, LINE, '\0', CLIENT_ID);

                auto cursor = this->textEdit->textCursor();
                cursor.setPosition(vecToTextPos(service.text_vec));
                this->textEdit->setTextCursor(cursor);
                std::cout << res.toStdString() << std::endl;
                std::cout << "cursor pos = " << this->textEdit->textCursor().position() << std::endl;

                return true;
            } else if (keyEvent->key() >= Qt::Key_Space && keyEvent->key() <= Qt::Key_Dead_Longsolidusoverlay && keyEvent->key() != Qt::Key_Backspace && keyEvent->key() != Qt::Key_Delete) {
                last_executed_operations.add(service.OPs(OP_type::INSERT, POS, LINE, keyEvent->key(), CLIENT_ID));
                res = QString::fromUtf8(service.initialize().c_str());
                POS++;
                std::cout << "printed " << keyEvent->text().toStdString() << std::endl;
            }
            service.OPs(OP_type::MOVE, POS, LINE, '\0', CLIENT_ID);

            this->textEdit->clear();
            this->textEdit->textCursor().insertText(res);
            auto cursor = this->textEdit->textCursor();
            cursor.setPosition(vecToTextPos(service.text_vec));
            this->textEdit->setTextCursor(cursor);
            std::cout << res.toStdString() << std::endl;
            std::cout << "cursor pos = " << this->textEdit->textCursor().position() << std::endl;

            return true;
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
    try {
        std::cout << "undo signal received" << std::endl;
        auto a = last_executed_operations.get_for_undo();
        OP_type op_type = a.op;
        uint64_t pos = a.pos;
        uint64_t line = a.line;
        char sym = a.sym;
        uint32_t user_id = a.user_id;
        uint64_t last_op = a.op_id;
        switch (op_type) {
            case operationTransportation::INSERT: {
                service.OPs(OP_type::INSERT, POS, LINE, sym, CLIENT_ID);
                POS++;
                break;
            }
            case operationTransportation::DELETE: {
                service.OPs(OP_type::DELETE, POS, LINE, sym, CLIENT_ID);
                POS--;
                break;
            }
            case operationTransportation::ADD_LINE: {
                service.OPs(OP_type::ADD_LINE, POS, LINE, sym, CLIENT_ID);
                LINE++;
                POS = 0;
                break;
            }
            case operationTransportation::DEL_LINE: {
                if (LINE != 0) {
                    service.OPs(OP_type::DEL_LINE, POS, LINE, sym, CLIENT_ID);
                    LINE--;
                    POS = service.text_vec.back().size();
                }
                break;
            }
            default:
                return;
        }
        QString res = QString::fromUtf8(service.initialize().c_str());
        this->textEdit->textCursor().setPosition(POS);
        this->textEdit->clear();
        this->textEdit->textCursor().insertText(res);
        std::cout << res.toStdString() << std::endl;
    }
    catch (int e) {
        std::cout << "undo failed" << std::endl;
    }
}

void client::redo() {
    try {
        std::cout << "redo signal received" << std::endl;
        auto a = last_executed_operations.get_for_redo();
        OP_type op_type = a.op;
        uint64_t pos = a.pos;
        uint64_t line = a.line;
        char sym = a.sym;
        uint32_t user_id = a.user_id;
        uint64_t last_op = a.op_id;
        switch (op_type) {
            case operationTransportation::INSERT: {
                service.OPs(OP_type::INSERT, POS, LINE, sym, CLIENT_ID);
                POS++;
                break;
            }
            case operationTransportation::DELETE: {
                service.OPs(OP_type::DELETE, POS, LINE, sym, CLIENT_ID);
                POS--;
                break;
            }
            case operationTransportation::ADD_LINE: {
                service.OPs(OP_type::ADD_LINE, POS, LINE, sym, CLIENT_ID);
                LINE++;
                POS = 0;
                break;
            }
            case operationTransportation::DEL_LINE: {
                if (LINE != 0) {
                    service.OPs(OP_type::DEL_LINE, POS, LINE, sym, CLIENT_ID);
                    LINE--;
                    POS = service.text_vec.back().size();
                }
                break;
            }
            default:
                return;
        }
        QString res = QString::fromUtf8(service.initialize().c_str());
        this->textEdit->textCursor().setPosition(POS);
        this->textEdit->clear();
        this->textEdit->textCursor().insertText(res);
        std::cout << res.toStdString() << std::endl;
    }
    catch (int e) {
        std::cout << "redo failed" << std::endl;
    }
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

    connect(undoAct, &QAction::triggered, this, &client::undo);
    editMenu->addAction(undoAct);
    editToolBar->addAction(undoAct);

    connect(redoAct, &QAction::triggered, this, &client::redo);
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
        if (textEdit->document()->isModified())
            save();
    }
}
