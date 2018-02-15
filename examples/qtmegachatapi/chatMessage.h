#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QWidget>
#include <QDateTime>
#include <QListWidgetItem>
#include "megachatapi.h"
#include "ui_chatMessageWidget.h"
#include "chatWindow.h"
class ChatWindow;
namespace Ui {
class ChatMessageWidget;
}

class ChatMessage: public QWidget
{
    Q_OBJECT
    protected:
        Ui::ChatMessageWidget *ui;
        megachat::MegaChatHandle chatId;
        megachat::MegaChatMessage *message;
        megachat::MegaChatApi* megaChatApi;
        QListWidgetItem * widgetItem;
        void updateToolTip();
        ChatWindow *chatWin;
        bool edited;
        friend class ChatWindow;
    public:
        ChatMessage(ChatWindow *parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg);
        virtual ~ChatMessage();
        std::string managementInfoToString() const;
        void setMessageContent(const char * content);
        void setTimestamp(int64_t ts);
        void setStatus(int status);
        void setAuthor();
        bool isMine() const;
        void markAsEdited();
        void startEditingMsgWidget();
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);
        megachat::MegaChatMessage *getMessage() const;
        void setMessage(megachat::MegaChatMessage *message);
        ChatMessage* clearEdit();
        void setManualMode(bool manualMode);


    public slots:
        void onDiscardManualSending();
        void onManualSending();
        void cancelMsgEdit(bool clicked);
        void saveMsgEdit(bool clicked);
        void onMessageCtxMenu(const QPoint& point);
        void onMessageDelAction();
        void onMessageEditAction();
};
#endif // CHATMESSAGE_H
