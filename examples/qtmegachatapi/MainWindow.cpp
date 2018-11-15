#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QTMegaChatEvent.h>
#include "uiSettings.h"
#include "chatSettings.h"

using namespace mega;
using namespace megachat;

MainWindow::MainWindow(QWidget *parent, MegaLoggerApplication *logger, megachat::MegaChatApi *megaChatApi, mega::MegaApi *megaApi) :
    QMainWindow(0),
    ui(new Ui::MainWindow)
{
    mApp = (MegaChatApplication *) parent;
    nContacts = 0;
    activeChats = 0;
    archivedChats = 0;
    inactiveChats = 0;
    ui->setupUi(this);
    ui->contactList->setSelectionMode(QAbstractItemView::NoSelection);
    ui->chatList->setSelectionMode(QAbstractItemView::NoSelection);
    mMegaChatApi = megaChatApi;
    mMegaApi = megaApi;
    onlineStatus = NULL;
    mShowInactive = false;
    mShowArchived = false;
    mLogger = logger;
    mChatSettings = new ChatSettings();
    qApp->installEventFilter(this);

    megaChatListenerDelegate = new QTMegaChatListener(mMegaChatApi, this);
    mMegaChatApi->addChatListener(megaChatListenerDelegate);
#ifndef KARERE_DISABLE_WEBRTC
    megaChatCallListenerDelegate = new megachat::QTMegaChatCallListener(mMegaChatApi, this);
    mMegaChatApi->addChatCallListener(megaChatCallListenerDelegate);
#endif
}

MainWindow::~MainWindow()
{
    mMegaChatApi->removeChatListener(megaChatListenerDelegate);
#ifndef KARERE_DISABLE_WEBRTC
    mMegaChatApi->removeChatCallListener(megaChatCallListenerDelegate);
#endif

    delete megaChatListenerDelegate;
    delete megaChatCallListenerDelegate;
    delete mChatSettings;
    clearChatControllers();
    clearContactListControllers(false);
    delete ui;
}

void MainWindow::clearContactListControllers(bool onlyWidget)
{
    std::map<mega::MegaHandle, ContactListItemController *>::iterator it;
    for (it = contactControllers.begin(); it != contactControllers.end(); it++)
    {
        ContactListItemController * itemController = (ContactListItemController *)it->second;
        if(onlyWidget)
        {
            itemController->addOrUpdateWidget(nullptr);
        }
        else
        {
            delete itemController;
        }
    }
}

mega::MegaUserList * MainWindow::getUserContactList()
{
    return mMegaApi->getContacts();
}

std::string MainWindow::getAuthCode()
{
    bool ok;
    QString qCode;

    while (1)
    {
        qCode = QInputDialog::getText((QWidget *)this, tr("Login verification"),
                tr("Enter the 6-digit code generated by your authenticator app"), QLineEdit::Normal, "", &ok);

        if (ok)
        {
            if (qCode.size() == 6)
            {
                return qCode.toStdString();
            }
        }
        else
        {
            return "";
        }
    }
}

void MainWindow::onTwoFactorCheck(bool)
{
    mMegaApi->multiFactorAuthCheck(mMegaChatApi->getMyEmail());
}

void MainWindow::onTwoFactorGetCode()
{
    mMegaApi->multiFactorAuthGetCode();
}

void MainWindow::onTwoFactorDisable()
{
    std::string auxcode = getAuthCode();
    if (!auxcode.empty())
    {
        QString code(auxcode.c_str());
        mMegaApi->multiFactorAuthDisable(code.toUtf8().constData());
    }
}

void MainWindow::createFactorMenu(bool factorEnabled)
{
    QMenu menu(this);
    if(factorEnabled)
    {
        auto disableFA = menu.addAction("Disable 2FA");
        connect(disableFA, SIGNAL(triggered()), this, SLOT(onTwoFactorDisable()));
    }
    else
    {
        auto getFA = menu.addAction("Enable 2FA");
        connect(getFA, SIGNAL(triggered()), this, SLOT(onTwoFactorGetCode()));
    }

    menu.setLayoutDirection(Qt::RightToLeft);
    menu.adjustSize();
    menu.deleteLater();
}

#ifndef KARERE_DISABLE_WEBRTC
void MainWindow::onChatCallUpdate(megachat::MegaChatApi */*api*/, megachat::MegaChatCall *call)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itWidgets = chatWidgets.find(call->getChatid());
    if(itWidgets == chatWidgets.end())
    {
        throw std::runtime_error("Incoming call from unknown contact");
    }

    ChatItemWidget *chatItemWidget = itWidgets->second;
    const MegaChatListItem *auxItem = getLocalChatListItem(call->getChatid());
    const char *chatWindowTitle = auxItem->getTitle();

    ChatWindow *auxChatWindow = NULL;

    if (!chatItemWidget->getChatWindow())
    {
        megachat::MegaChatRoom *chatRoom = mMegaChatApi->getChatRoom(call->getChatid());
        auxChatWindow = new ChatWindow(this, mMegaChatApi, chatRoom->copy(), chatWindowTitle);
        chatItemWidget->setChatWindow(auxChatWindow);
        auxChatWindow->show();
        auxChatWindow->openChatRoom();
        delete chatRoom;
    }
    else
    {
        auxChatWindow =chatItemWidget->getChatWindow();
        auxChatWindow->show();
        auxChatWindow->setWindowState(Qt::WindowActive);
    }

    switch(call->getStatus())
    {
        case megachat::MegaChatCall::CALL_STATUS_TERMINATING:
           {
               ChatItemWidget *chatItemWidget = this->getChatItemWidget(call->getChatid(),false);
               chatItemWidget->getChatWindow()->hangCall();
               return;
           }
           break;
        case megachat::MegaChatCall::CALL_STATUS_RING_IN:
           {
              ChatWindow *auxChatWindow =chatItemWidget->getChatWindow();
              if(auxChatWindow->getCallGui()==NULL)
              {
                 auxChatWindow->createCallGui(call->hasRemoteVideo());
              }
           }
           break;
        case megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS:
           {
               ChatWindow *auxChatWindow =chatItemWidget->getChatWindow();
               if ((auxChatWindow->getCallGui()) && !(auxChatWindow->getCallGui()->getCall()))
               {
                   auxChatWindow->connectCall();
               }

               if (call->hasChanged(MegaChatCall::CHANGE_TYPE_REMOTE_AVFLAGS))
               {
                    CallGui *callGui = auxChatWindow->getCallGui();
                    if(call->hasRemoteVideo())
                    {
                        callGui->ui->remoteRenderer->disableStaticImage();
                    }
                    else
                    {
                        callGui->setAvatarOnRemote();
                        callGui->ui->remoteRenderer->enableStaticImage();
                    }
               }
           }
           break;
    }
}
#endif

void MainWindow::clearContactWidgetList()
{
    ui->contactList->clear();
    clearContactListControllers(true);
}

void MainWindow::clearChatWidgetList()
{
    ui->chatList->clear();
    chatWidgets.clear();
}

void MainWindow::clearQtChatWidgetList()
{
    ui->chatList->clear();
}

void MainWindow::clearChatWidgets()
{
    std::map<megachat::MegaChatHandle, ChatListItemController *>::iterator it;
    for (it = chatControllers .begin(); it != chatControllers.end(); it++)
    {
        ChatListItemController * itemController = it->second;
        if(itemController)
        {
            itemController->addOrUpdateWidget(nullptr);
        }
    }
}

void MainWindow::clearChatControllers()
{
    std::map<megachat::MegaChatHandle, ChatListItemController *>::iterator it;
    for (it = chatControllers .begin(); it != chatControllers.end(); it++)
    {
        ChatListItemController * itemController = it->second;
        delete itemController;
    }
    chatControllers.clear();
}

void MainWindow::orderContactList()
{
    clearContactWidgetList();
    addContacts();
}

void MainWindow::orderChatList()
{
    mNeedReorder = false;
    auxChatWidgets = chatWidgets;
    clearChatWidgetList();

    //TODO: uncomment when revamping has finished
    //Clean chat Qt widgets container
    clearQtChatWidgetList();

    // Clean the ChatItemWidgets in ChatListItemController list
    //clearChatWidgets();

    if (mShowArchived)
    {
        addArchivedChats();
    }

    if(mShowInactive)
    {
        addInactiveChats();
    }
    addActiveChats();

    auxChatWidgets.clear();

    // prepare tag to indicate chatrooms shown
    QString text;
    if (mShowArchived && mShowInactive)
    {
        text.append(" Showing <all> chatrooms");
    }
    else if (mShowArchived)
    {
        text.append(" Showing <active+archived> chatrooms");
    }
    else if (mShowInactive)
    {
        text.append(" Showing <active+inactive> chatrooms");
    }
    else
    {
        text.append(" Showing <active> chatrooms");
    }
    ui->mOnlineStatusDisplay->setText(text);
}

void MainWindow::addContacts()
{
    ui->mContacsSeparator->setText(" Loading contacts");
    MegaUser *contact = NULL;
    MegaUserList *contactList = mMegaApi->getContacts();
    setNContacts(contactList->size());

    for (int i = 0; i < contactList->size(); i++)
    {
        contact = contactList->get(i);
        mega::MegaHandle userHandle = contact->getHandle();
        if (userHandle != this->mMegaChatApi->getMyUserHandle())
        {
            if (contact->getVisibility() == MegaUser::VISIBILITY_HIDDEN && mShowInactive != true)
            {
                continue;
            }
            addOrUpdateContactController(contact);
        }
    }

    if (contactList->size() > 0)
    {
        ui->mContacsSeparator->setText("Showing <active> contacts");
    }
    delete contactList;
}

void MainWindow::addArchivedChats()
{
    std::list<Chat> *archivedChatList = getLocalChatListItemsByStatus(chatArchivedStatus);
    archivedChatList->sort();
    for (Chat &chat : (*archivedChatList))
    {
        addChat(chat.chatItem);
    }
    delete archivedChatList;
}

void MainWindow::addInactiveChats()
{
    std::list<Chat> *inactiveChatList = getLocalChatListItemsByStatus(chatInactiveStatus);
    inactiveChatList->sort();
    for (Chat &chat : (*inactiveChatList))
    {
        addChat(chat.chatItem);
    }
    delete inactiveChatList;
}

void MainWindow::addActiveChats()
{
    std::list<Chat> *activeChatList = getLocalChatListItemsByStatus(chatActiveStatus);
    activeChatList->sort();
    for (Chat &chat : (*activeChatList))
    {
        addChat(chat.chatItem);
    }
    delete activeChatList;
}

bool MainWindow::eventFilter(QObject *, QEvent *event)
{
    if (this->mMegaChatApi->isSignalActivityRequired() && event->type() == QEvent::MouseButtonRelease)
    {
        this->mMegaChatApi->signalPresenceActivity();
    }
    return false;
}


void MainWindow::on_bSettings_clicked()
{
    QMenu menu(this);

    menu.setAttribute(Qt::WA_DeleteOnClose);

    auto actInactive = menu.addAction(tr("Show inactive chats"));
    connect(actInactive, SIGNAL(triggered()), this, SLOT(onShowInactiveChats()));
    actInactive->setCheckable(true);
    actInactive->setChecked(mShowInactive);
    // TODO: adjust with new flags in chat-links branch

    auto actArchived = menu.addAction(tr("Show archived chats"));
    connect(actArchived, SIGNAL(triggered()), this, SLOT(onShowArchivedChats()));
    actArchived->setCheckable(true);
    actArchived->setChecked(mShowArchived);
    // TODO: adjust with new flags in chat-links branch

    menu.addSeparator();

    auto addAction = menu.addAction(tr("Add user to contacts"));
    connect(addAction, SIGNAL(triggered()), this, SLOT(onAddContact()));

    auto actPeerChat = menu.addAction(tr("Create 1on1 chat"));
    connect(actPeerChat, SIGNAL(triggered()), this, SLOT(onCreatePeerChat()));
    // TODO: connect to slot once chat-links branch is merged

    auto actGroupChat = menu.addAction(tr("Create group chat"));
    connect(actGroupChat, SIGNAL(triggered()), this, SLOT(onAddGroupChat()));
    // TODO: connect to slot once chat-links branch is merged

    auto actPubChat = menu.addAction(tr("Create public chat"));
    connect(actPubChat, SIGNAL(triggered()), this, SLOT(onAddPubChatGroup()));
    // TODO: connect to slot once chat-links branch is merged

    auto actLoadLink = menu.addAction(tr("Preview chat-link"));
    connect(actLoadLink, SIGNAL(triggered()), this, SLOT(loadChatLink()));
    // TODO: connect to slot once chat-links branch is merged

    menu.addSeparator();
    auto actTwoFactCheck = menu.addAction(tr("Enable/Disable 2FA"));
    connect(actTwoFactCheck, SIGNAL(clicked(bool)), this, SLOT(onTwoFactorCheck(bool)));
    actTwoFactCheck->setEnabled(mMegaApi->multiFactorAuthAvailable());

    menu.addSeparator();
    auto actWebRTC = menu.addAction(tr("Set audio/video input devices"));
    connect(actWebRTC, SIGNAL(triggered()), this, SLOT(onWebRTCsetting()));

    menu.addSeparator();
    auto actPrintMyInfo = menu.addAction(tr("Print my info"));
    connect(actPrintMyInfo, SIGNAL(triggered()), this, SLOT(onPrintMyInfo()));
    // TODO: connect to slot once chat-links branch is merged

    menu.addSeparator();
    MegaChatPresenceConfig *presenceConfig = mMegaChatApi->getPresenceConfig();
    auto actlastGreenVisible = menu.addAction("Enable/Disable Last-Green");
    connect(actlastGreenVisible, SIGNAL(triggered()), this, SLOT(onlastGreenVisibleClicked()));
    actlastGreenVisible->setCheckable(true);
    actlastGreenVisible->setChecked(presenceConfig->isLastGreenVisible());
    delete presenceConfig;

    QPoint pos = ui->bSettings->pos();
    pos.setX(pos.x() + ui->bSettings->width());
    pos.setY(pos.y() + ui->bSettings->height());
    menu.exec(mapToGlobal(pos));
}

void MainWindow::onWebRTCsetting()
{
    #ifndef KARERE_DISABLE_WEBRTC
        this->mMegaChatApi->loadAudioVideoDeviceList();
    #endif
}

void MainWindow::createSettingsMenu()
{
    ChatSettingsDialog *chatSettings = new ChatSettingsDialog(this, mChatSettings);
    chatSettings->exec();
    chatSettings->deleteLater();
}

void MainWindow::on_bOnlineStatus_clicked()
{
    onlineStatus = new QMenu(this);
    auto actOnline = onlineStatus->addAction("Online");
    actOnline->setData(QVariant(MegaChatApi::STATUS_ONLINE));
    connect(actOnline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actAway = onlineStatus->addAction("Away");
    actAway->setData(QVariant(MegaChatApi::STATUS_AWAY));
    connect(actAway, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actDnd = onlineStatus->addAction("Busy");
    actDnd->setData(QVariant(MegaChatApi::STATUS_BUSY));
    connect(actDnd, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actOffline = onlineStatus->addAction("Offline");
    actOffline->setData(QVariant(MegaChatApi::STATUS_OFFLINE));
    connect(actOffline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    QPoint pos = ui->bOnlineStatus->pos();
    pos.setX(pos.x() + ui->bOnlineStatus->width());
    pos.setY(pos.y() + ui->bOnlineStatus->height());

    onlineStatus->setStyleSheet("QMenu {"
        "background-color: qlineargradient("
        "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}"
        "QMenu::item:!selected{"
            "color: white;"
        "}"
        "QMenu::item:selected{"
            "background-color: qlineargradient("
            "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}");
    onlineStatus->exec(mapToGlobal(pos));
    onlineStatus->deleteLater();
}

void MainWindow::onShowInactiveChats()
{
    mShowInactive = !mShowInactive;
    orderChatList();
}

void MainWindow::onAddGroupChat()
{
    onAddChatGroup();
}

void MainWindow::onShowArchivedChats()
{
    mShowArchived = !mShowArchived;
    orderChatList();
}

ChatItemWidget *MainWindow::getChatItemWidget(megachat::MegaChatHandle chatHandle, bool reorder)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    if (!reorder)
    {
        itChats = chatWidgets.find(chatHandle);
        if (itChats == chatWidgets.end())
        {
            return NULL;
        }
    }
    else
    {
        itChats = auxChatWidgets.find(chatHandle);
        if (itChats == auxChatWidgets.end())
        {
            return NULL;
        }
    }
    return itChats->second;
}

ContactItemWidget *MainWindow::addContactWidget(MegaUser *user)
{
    //Create widget and add to interface
    int index = -(archivedChats + nContacts);
    nContacts += 1;
    ContactItemWidget *widget = new ContactItemWidget(ui->contactList, this, mMegaChatApi, mMegaApi, user);
    widget->updateToolTip(user);
    QListWidgetItem *item = new QListWidgetItem();
    widget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    ui->contactList->insertItem(index, item);
    ui->contactList->setItemWidget(item, widget);
    return widget;
}

void MainWindow::addOrUpdateContactController(MegaUser *user)
{
    //Add contact widget
    ContactItemWidget * widget = addContactWidget(user);

    //If no controller exists we need to create
    std::map<mega::MegaHandle, ContactListItemController *>::iterator itContacts;
    itContacts = contactControllers.find(user->getHandle());
    ContactListItemController *itemController;
    if (itContacts == contactControllers.end())
    {
         itemController = new ContactListItemController(user, widget);
    }
    else
    {
         //If controller exists we need to update item and widget
         itemController = (ContactListItemController *) itContacts->second;
         itemController->addOrUpdateItem(user->copy());
         itemController->addOrUpdateWidget(widget);
    }
}

void MainWindow::addChat(const MegaChatListItem* chatListItem)
{
    int index = 0;
    if (chatListItem->isArchived())
    {
        index = -(archivedChats);
        archivedChats += 1;
    }
    else if (!chatListItem->isActive())
    {
        index = -(nContacts + archivedChats + inactiveChats);
        inactiveChats += 1;
    }
    else
    {
        index = -(activeChats + inactiveChats + archivedChats+nContacts);
        activeChats += 1;
    }

    megachat::MegaChatHandle chathandle = chatListItem->getChatId();
    ChatItemWidget *chatItemWidget = new ChatItemWidget(this, mMegaChatApi, chatListItem);
    chatItemWidget->updateToolTip(chatListItem, NULL);
    QListWidgetItem *item = new QListWidgetItem();
    chatItemWidget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    chatWidgets.insert(std::pair<megachat::MegaChatHandle, ChatItemWidget *>(chathandle,chatItemWidget));
    ui->chatList->insertItem(index, item);
    ui->chatList->setItemWidget(item, chatItemWidget);

    ChatItemWidget *auxChatItemWidget = getChatItemWidget(chathandle, true);
    if(auxChatItemWidget)
    {
        chatItemWidget->mChatWindow = auxChatItemWidget->mChatWindow;
        auxChatItemWidget->deleteLater();
    }
}

void MainWindow::onChatListItemUpdate(MegaChatApi *, MegaChatListItem *item)
{
    //Get a copy of old local chatListItem
    const MegaChatListItem * oldItem = getLocalChatListItem(item->getChatId());
    if (oldItem)
    {
        oldItem = oldItem->copy();
    }

    addOrUpdateLocalChatListItem(item);

    if (!allowOrder)
        return;

    ChatItemWidget * chatItemWidget;
    megachat::MegaChatHandle chatid = item->getChatId();
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    itChats = chatWidgets.find(chatid);
    if (itChats != chatWidgets.end())
    {
        chatItemWidget = itChats->second;
    }

    // If we don't need to reorder and chatItemwidget is rendered
    // we need to update the widget because not order actions requires
    // a live update of widget
    if (chatItemWidget && !needReorder(item, oldItem))
    {
        megachat::MegaChatHandle chatid = item->getChatId();
        std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
        itChats = chatWidgets.find(chatid);
        if (itChats != chatWidgets.end())
        {
            ChatItemWidget *chatItemWidget = itChats->second;

            //Last Message update
            if (item->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_LAST_MSG))
            {
                chatItemWidget->updateToolTip(item, NULL);
            }

            //Unread count update
            if (item->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_UNREAD_COUNT))
            {
                chatItemWidget->onUnreadCountChanged(item->getUnreadCount());
            }

            //Title update
            if (item->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_TITLE))
            {
                chatItemWidget->onTitleChanged(item->getTitle());
            }

            //Own priv update
            if (item->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_OWN_PRIV))
            {
                chatItemWidget->updateToolTip(item, NULL);
            }

            //Participants update
            if (item->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_PARTICIPANTS))
            {
                chatItemWidget->updateToolTip(item, NULL);
            }
        }
    }
    else
    {
        orderChatList();
    }
    delete oldItem;
}

bool MainWindow::needReorder(MegaChatListItem *newItem, const MegaChatListItem * oldItem)
{
    //The chatroom has been left by own user
    if(newItem->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_CLOSED)
         || newItem->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_LAST_TS)
         || newItem->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_ARCHIVE)
         || newItem->hasChanged(megachat::MegaChatListItem::CHANGE_TYPE_UNREAD_COUNT)
         || (newItem->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_RM && mShowInactive)
         || ((oldItem->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_RM)
             &&(newItem->getOwnPrivilege() > megachat::MegaChatRoom::PRIV_RM)))
    {
        mNeedReorder = true;
        return mNeedReorder;
    }
    return false;
}

void MainWindow::onAddChatGroup()
{
    mega::MegaUserList *list = mMegaApi->getContacts();
    ChatGroupDialog *chatDialog = new ChatGroupDialog(this, mMegaChatApi);
    chatDialog->createChatList(list);
    chatDialog->show();
}

void MainWindow::onAddContact()
{
    QString email = QInputDialog::getText(this, tr("Add contact"), tr("Please enter the email of the user to add"));
    if (email.isNull())
        return;

    char *myEmail = mMegaApi->getMyEmail();
    QString qMyEmail = myEmail;
    delete [] myEmail;

    if (email == qMyEmail)
    {
        QMessageBox::critical(this, tr("Add contact"), tr("You can't add your own email as contact"));
        return;
    }
    std::string emailStd = email.toStdString();
    mMegaApi->inviteContact(emailStd.c_str(),tr("I'd like to add you to my contact list").toUtf8().data(), MegaContactRequest::INVITE_ACTION_ADD);
}

void MainWindow::setOnlineStatus()
{
    auto action = qobject_cast<QAction*>(QObject::sender());
    assert(action);
    bool ok;
    auto pres = action->data().toUInt(&ok);
    if (!ok || (pres == MegaChatApi::STATUS_INVALID))
    {
        return;
    }
    this->mMegaChatApi->setOnlineStatus(pres);
}

void MainWindow::onChatConnectionStateUpdate(MegaChatApi *, MegaChatHandle chatid, int newState)
{
    if (chatid == megachat::MEGACHAT_INVALID_HANDLE)
    {
        // When we are connected to all chats we have to reorder the chatlist
        // we skip all reorders until we receive this event to avoid app overload
        allowOrder = true;
        updateLocalChatListItems();
        orderChatList();

        megachat::MegaChatPresenceConfig *presenceConfig = mMegaChatApi->getPresenceConfig();
        if (presenceConfig)
        {
            onChatPresenceConfigUpdate(mMegaChatApi, presenceConfig);
        }
        delete presenceConfig;
        return;
    }
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it;
    it = chatWidgets.find(chatid);

    if (it != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = it->second;
        chatItemWidget->onlineIndicatorUpdate(newState);
    }
}

void MainWindow::onChatInitStateUpdate(megachat::MegaChatApi *, int newState)
{
    if (newState == MegaChatApi::INIT_ERROR)
    {
        QMessageBox msgBox;
        msgBox.setText("Critical error in MEGAchat. The application will close now. If the problem persists, you can delete your cached sessions.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Ok)
        {
            deleteLater();
            return;
        }
    }

    if (newState == MegaChatApi::INIT_ONLINE_SESSION || newState == MegaChatApi::INIT_OFFLINE_SESSION)
    {
        if(!isVisible())
        {
            mApp->resetLoginDialog();
            show();
            updateLocalChatListItems();
        }

        if (newState == MegaChatApi::INIT_ONLINE_SESSION)
        {
            // contacts are not loaded until MegaApi::login completes
            orderContactList();
        }

        QString auxTitle(mMegaChatApi->getMyEmail());
        if (mApp->sid() && newState == MegaChatApi::INIT_OFFLINE_SESSION)
        {
            auxTitle.append(" [OFFLINE MODE]");

            //In case we are in offline mode we need to reorder the contacts and chats
            orderChatList();
            orderContactList();
        }
        setWindowTitle(auxTitle);
    }
}

void MainWindow::onChatOnlineStatusUpdate(MegaChatApi *, MegaChatHandle userhandle, int status, bool inProgress)
{
    if (status == megachat::MegaChatApi::STATUS_INVALID)
    {
        // If we don't receive our presence we'll skip all chats reorders
        // when we are connected to all chats this flag will be set true
        // and chatlist will be reordered
        allowOrder = false;
        status = 0;
    }

    if (this->mMegaChatApi->getMyUserHandle() == userhandle && !inProgress)
    {
        ui->bOnlineStatus->setText(kOnlineSymbol_Set);
        if (status >= 0 && status < NINDCOLORS)
            ui->bOnlineStatus->setStyleSheet(kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
    }
    else
    {
        std::map<mega::MegaHandle, ContactListItemController *>::iterator itContacts;
        itContacts = this->contactControllers.find((mega::MegaHandle) userhandle);
        if (itContacts != contactControllers.end())
        {
            ContactListItemController * itemController = itContacts->second;
            assert(!inProgress);
            itemController->getWidget()->updateOnlineIndicator(status);
        }
    }
}

void MainWindow::onChatPresenceConfigUpdate(MegaChatApi *, MegaChatPresenceConfig *config)
{
    int status = config->getOnlineStatus();
    if (status == megachat::MegaChatApi::STATUS_INVALID)
        status = 0;

    ui->bOnlineStatus->setText(config->isPending()
        ? kOnlineSymbol_InProgress
        : kOnlineSymbol_Set);

    ui->bOnlineStatus->setStyleSheet(
                kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
}

void MainWindow::onChatPresenceLastGreen(MegaChatApi */*api*/, MegaChatHandle userhandle, int lastGreen)
{
    const char *firstname = mApp->getFirstname(userhandle);
    if (!firstname)
    {
        firstname = mMegaApi->userHandleToBase64(userhandle);
    }

    std::string str;
    str.append("User: ");
    str.append(firstname);
    str.append("\nLast time green: ");
    str.append(std::to_string(lastGreen));
    str.append(" minutes ago");

    QMessageBox* msgBox = new QMessageBox(this);
    msgBox->setIcon( QMessageBox::Information );
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setWindowTitle( tr("Last time green"));
    msgBox->setText(str.c_str());
    msgBox->setModal(false);
    msgBox->show();
    delete firstname;
}

int MainWindow::getNContacts() const
{
    return nContacts;
}

void MainWindow::setNContacts(int nContacts)
{
    this->nContacts = nContacts;
}

void MainWindow::updateMessageFirstname(MegaChatHandle contactHandle, const char *firstname)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it;
    for (it = chatWidgets.begin(); it != chatWidgets.end(); it++)
    {
        const MegaChatListItem *chatListItem  = getLocalChatListItem(it->first);
        ChatItemWidget *chatItemWidget = it->second;

        if (chatListItem->getLastMessageSender() == contactHandle)
        {
            chatItemWidget->updateToolTip(chatListItem, firstname);
        }

        ChatWindow *chatWindow = chatItemWidget->getChatWindow();
        if (chatWindow)
        {
            chatWindow->updateMessageFirstname(contactHandle, firstname);
        }
    }
}

void MainWindow::updateLocalChatListItems()
{
    for (auto it = mLocalChatListItems.begin(); it != mLocalChatListItems.end(); it++)
    {
        delete it->second;
    }
    mLocalChatListItems.clear();

    //Add all active chatListItems
    MegaChatListItemList *chatList = mMegaChatApi->getActiveChatListItems();
    for (unsigned int i = 0; i < chatList->size(); i++)
    {
        addOrUpdateLocalChatListItem(chatList->get(i));
    }
    delete chatList;

    //Add inactive chatListItems
    chatList = mMegaChatApi->getInactiveChatListItems();
    for (unsigned int i = 0; i < chatList->size(); i++)
    {
        addOrUpdateLocalChatListItem(chatList->get(i));
    }
    delete chatList;

    //Add archived chatListItems
    chatList = mMegaChatApi->getArchivedChatListItems();
    for (unsigned int i = 0; i < chatList->size(); i++)
    {
        addOrUpdateLocalChatListItem(chatList->get(i));
    }
    delete chatList;
}

ChatListItemController* MainWindow::getChatControllerById(MegaChatHandle chatId)
{
    std::map<mega::MegaHandle, ChatListItemController *> ::iterator it;
    it = this->chatControllers.find(chatId);
    if (it != chatControllers.end())
    {
        return it->second;
    }

    return nullptr;
}

void MainWindow::addOrUpdateLocalChatListItem(const MegaChatListItem *item)
{
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator itItem;
    itItem = mLocalChatListItems.find(item->getChatId());
    if (itItem != mLocalChatListItems.end())
    {
        const megachat::MegaChatListItem *auxItem = itItem->second;
        mLocalChatListItems.erase(itItem);
        delete auxItem;
    }

    mLocalChatListItems.insert(std::pair<megachat::MegaChatHandle, const MegaChatListItem *>(item->getChatId(),item->copy()));
}

void MainWindow::removeLocalChatListItem(MegaChatListItem *item)
{
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator itItem;
    itItem = mLocalChatListItems.find(item->getChatId());
    if (itItem != mLocalChatListItems.end())
    {
        const megachat::MegaChatListItem *auxItem = itItem->second;
        mLocalChatListItems.erase(itItem);
        delete auxItem;
    }
}

const megachat::MegaChatListItem *MainWindow::getLocalChatListItem(megachat::MegaChatHandle chatId)
{
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator itItem;
    itItem = mLocalChatListItems.find(chatId);
    if (itItem != mLocalChatListItems.end())
    {
        return itItem->second;
    }
    return NULL;
}

std::list<Chat> *MainWindow::getLocalChatListItemsByStatus(int status)
{
    std::list<Chat> *chatList = new std::list<Chat>;
    std::map<megachat::MegaChatHandle, const MegaChatListItem *>::iterator it;

    for (it = mLocalChatListItems.begin(); it != mLocalChatListItems.end(); it++)
    {
        const megachat::MegaChatListItem *item = it->second;
        switch (status)
        {
            case chatActiveStatus:
                if (item->isActive() && !item->isArchived())
                {
                    chatList->push_back(Chat(item));
                }
                break;

            case chatInactiveStatus:
                if (!item->isActive() && !item->isArchived())
                {
                    chatList->push_back(Chat(item));
                }
                break;

            case chatArchivedStatus:
                if (item->isArchived())
                {
                    chatList->push_back(Chat(item));
                }
                break;
        }
    }
    return chatList;
}


void MainWindow::updateContactFirstname(MegaChatHandle contactHandle, const char * firstname)
{
    std::map<mega::MegaHandle, ContactListItemController *>::iterator itContacts;
    itContacts = contactControllers.find(contactHandle);

    if (itContacts != contactControllers.end())
    {
        ContactListItemController *itemController = itContacts->second;
        itemController->getWidget()->updateTitle(firstname);
    }
}

void MainWindow::on_mLogout_clicked()
{
    QMessageBox msgBox;
    msgBox.setText("Do you want to logout?");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Ok)
    {
        mMegaApi->logout();
    }
}

void MainWindow::onlastGreenVisibleClicked()
{
    MegaChatPresenceConfig *presenceConfig = mMegaChatApi->getPresenceConfig();
    mMegaChatApi->setLastGreenVisible(!presenceConfig->isLastGreenVisible());
    delete presenceConfig;
}
