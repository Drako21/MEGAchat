#include <QMenu>
#include <QClipboard>
#include "reaction.h"
#include "ui_reaction.h"

Reaction::Reaction(ChatMessage *parent, const char *reactionString, int count) :
    QWidget(static_cast<QWidget *>(parent)),
    ui(new Ui::Reaction)
{        
    mChatMessage = parent;
    ui->setupUi(this);
    mCount = count;
    if (reactionString)
    {
        mReactionString = reactionString;
    }

    ui->mReaction->setText((mReactionString + " " + std::to_string(count)).c_str());
    setAttribute(::Qt::WA_Hover, true);
}

Reaction::~Reaction()
{
    delete ui;
}

void Reaction::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);    
    auto actAdd = menu.addAction(tr("React to this message"));
    connect(actAdd, SIGNAL(triggered()), this, SLOT(onAddReact()));
    auto actRemove = menu.addAction(tr("Del reaction"));
    connect(actRemove, SIGNAL(triggered()), this, SLOT(onRemoveReact()));
    auto actCopy = menu.addAction(tr("Copy UTF-8"));
    connect(actCopy, SIGNAL(triggered()), this, SLOT(onCopyReact()));

    QPoint pos = ui->mReaction->pos();
    pos.setX(pos.x() + ui->mReaction->width());
    pos.setY(pos.y() + ui->mReaction->height());
    menu.exec(mapToGlobal(pos));
    menu.deleteLater();
}

std::string Reaction::getReactionString() const
{
    return mReactionString;
}

void Reaction::updateReactionCount(int count)
{
    assert(count);  // it should not be called with count == 0
    mCount = count;
    ui->mReaction->setText((mReactionString + " " + std::to_string(count)).c_str());
}

void Reaction::onCopyReact()
{
    QApplication::clipboard()->setText(mReactionString.c_str());
}

void Reaction::onRemoveReact()
{
    mChatMessage->onManageReaction(true, mReactionString.c_str());
}

void Reaction::onAddReact()
{
    mChatMessage->onManageReaction(false, mReactionString.c_str());
}

void Reaction::enterEvent(QEvent *event)
{
    megachat::MegaChatApi *megachatApi = mChatMessage->getMegaChatApi();
    mega::unique_ptr <::mega::MegaHandleList> users(megachatApi->getReactionUsers(mChatMessage->getChatId(), mChatMessage->getMessage()->getMsgId(), mReactionString.c_str()));
    mega::unique_ptr<megachat::MegaChatRoom> chatRoom(megachatApi->getChatRoom(mChatMessage->getChatId()));
    if (!users || !chatRoom)
    {
        return;
    }

    QString text;
    mega::unique_ptr<const char[]> autorizationToken(chatRoom->getAuthorizationToken());
    for (unsigned int i = 0; i < users->size(); i++)
    {
        mega::unique_ptr<const char[]>firstName(mChatMessage->getChatWindow()->getMainWin()->getApp()->getFirstname(users->get(i), autorizationToken.get()));
        mega::unique_ptr<const char[]>b64handle(::mega::MegaApi::userHandleToBase64(users->get(i)));
        if (firstName)
        {
            text.append(firstName.get()).append(" ");
        }
        text.append("(").append(b64handle.get()).append(")\n");
    }
    ui->mReaction->setToolTip(text);
}
