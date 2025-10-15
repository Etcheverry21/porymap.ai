#include "extensions/scripts/ScriptEditorWidget.h"

#include <QLabel>
#include <QVBoxLayout>

#include "extensions/scripts/Services/ProjectPaths.h"

ScriptEditorWidget::ScriptEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_projectPaths(nullptr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *placeholder = new QLabel(tr("Script editor will appear here."), this);
    placeholder->setObjectName(QStringLiteral("scriptEditorPlaceholder"));
    placeholder->setAlignment(Qt::AlignCenter);

    layout->addWidget(placeholder);
}

void ScriptEditorWidget::setProjectPaths(ProjectPaths *paths)
{
    m_projectPaths = paths;
}

void ScriptEditorWidget::setProjectContext(const QString &projectRoot)
{
    Q_UNUSED(projectRoot);
}

void ScriptEditorWidget::setActiveMapContext(const QString &mapName)
{
    Q_UNUSED(mapName);
}

