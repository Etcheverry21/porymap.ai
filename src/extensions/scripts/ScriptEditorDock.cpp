#include "extensions/scripts/ScriptEditorDock.h"

#include <QWidget>

#include "extensions/scripts/ScriptEditorWidget.h"
#include "extensions/scripts/Services/ProjectPaths.h"

ScriptEditorDock::ScriptEditorDock(QWidget *parent)
    : QDockWidget(parent)
    , m_editorWidget(new ScriptEditorWidget(this))
{
    setObjectName(QStringLiteral("ScriptEditorDock"));
    setWindowTitle(tr("Script Editor"));
    setWidget(m_editorWidget);
    setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_editorWidget, &ScriptEditorWidget::scriptFilesChanged,
            this, &ScriptEditorDock::scriptFilesChanged);
}

ScriptEditorWidget *ScriptEditorDock::editorWidget() const
{
    return m_editorWidget;
}

void ScriptEditorDock::setProjectPaths(ProjectPaths *paths)
{
    m_editorWidget->setProjectPaths(paths);
}

void ScriptEditorDock::setProjectRoot(const QString &rootPath)
{
    m_editorWidget->setProjectContext(rootPath);
}

