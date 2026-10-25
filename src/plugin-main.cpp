#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("stream-chat-obs", "en-US")

namespace {
constexpr const char *DockId = "stream-chat-obs.chat";

class ChatDock final : public QWidget {
public:
    ChatDock()
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);

        auto *status = new QHBoxLayout();
        auto *twitchStatus = new QLabel(tr("Twitch: disconnected"), this);
        auto *youtubeStatus = new QLabel(tr("YouTube: disconnected"), this);
        status->addWidget(twitchStatus);
        status->addWidget(youtubeStatus);
        layout->addLayout(status);

        auto *messages = new QTextEdit(this);
        messages->setReadOnly(true);
        messages->setPlaceholderText(tr("Chat messages will appear here after connecting accounts."));
        layout->addWidget(messages);

        auto *targets = new QHBoxLayout();
        auto *sendToTwitch = new QCheckBox(tr("Twitch"), this);
        auto *sendToYouTube = new QCheckBox(tr("YouTube"), this);
        sendToTwitch->setEnabled(false);
        sendToYouTube->setEnabled(false);
        targets->addWidget(sendToTwitch);
        targets->addWidget(sendToYouTube);
        targets->addStretch();
        layout->addLayout(targets);

        auto *composer = new QHBoxLayout();
        auto *input = new QLineEdit(this);
        input->setPlaceholderText(tr("Connect an account to send messages"));
        input->setEnabled(false);
        auto *send = new QPushButton(tr("Send"), this);
        send->setEnabled(false);
        composer->addWidget(input);
        composer->addWidget(send);
        layout->addLayout(composer);
    }

protected:
    bool event(QEvent *event) override
    {
        // OBS sends this event when the user closes a dock. Network sessions
        // will be stopped here once the platform adapters are connected.
        return QWidget::event(event);
    }
};

QPointer<ChatDock> dock;

void createDock()
{
    if (dock)
        return;

    auto *widget = new ChatDock();
    if (obs_frontend_add_dock_by_id(DockId, "Stream Chat", widget)) {
        dock = widget;
    } else {
        delete widget;
        blog(LOG_WARNING, "Stream Chat dock could not be added");
    }
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        createDock();
}
} // namespace

bool obs_module_load(void)
{
    obs_frontend_add_event_callback(onFrontendEvent, nullptr);
    blog(LOG_INFO, "Stream Chat OBS %s loaded", STREAM_CHAT_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
    if (dock)
        obs_frontend_remove_dock(DockId);
    dock = nullptr;
}
