#include "extensions/scripts/AiAssistantWidget.h"

#include <QLabel>
#include <QVBoxLayout>

#include "extensions/scripts/Services/ProjectPaths.h"

AiAssistantWidget::AiAssistantWidget(QWidget *parent)
    : QWidget(parent)
    , m_projectPaths(nullptr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *placeholder = new QLabel(tr("AI assistant will appear here."), this);
    placeholder->setObjectName(QStringLiteral("aiAssistantPlaceholder"));
    placeholder->setAlignment(Qt::AlignCenter);

    layout->addWidget(placeholder);
}

void AiAssistantWidget::setProjectPaths(ProjectPaths *paths)
{
    m_projectPaths = paths;
}

void AiAssistantWidget::setProjectContext(const QString &projectRoot)
{
    Q_UNUSED(projectRoot);
}

void AiAssistantWidget::setActiveMapContext(const QString &mapName)
{
    Q_UNUSED(mapName);
}

