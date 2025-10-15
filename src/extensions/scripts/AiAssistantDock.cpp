#include "extensions/scripts/AiAssistantDock.h"

#include "extensions/scripts/AiAssistantWidget.h"
#include "extensions/scripts/Services/ProjectPaths.h"

AiAssistantDock::AiAssistantDock(QWidget *parent)
    : QDockWidget(parent)
    , m_assistantWidget(new AiAssistantWidget(this))
{
    setObjectName(QStringLiteral("AiAssistantDock"));
    setWindowTitle(tr("AI Assistant"));
    setWidget(m_assistantWidget);
    setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_assistantWidget, &AiAssistantWidget::requestInsertToEditor,
            this, &AiAssistantDock::requestInsertToEditor);
    connect(m_assistantWidget, &AiAssistantWidget::requestAttachToEvent,
            this, &AiAssistantDock::requestAttachToEvent);
}

AiAssistantWidget *AiAssistantDock::assistantWidget() const
{
    return m_assistantWidget;
}

void AiAssistantDock::setProjectPaths(ProjectPaths *paths)
{
    m_assistantWidget->setProjectPaths(paths);
}

void AiAssistantDock::setProjectRoot(const QString &rootPath)
{
    m_assistantWidget->setProjectContext(rootPath);
}

