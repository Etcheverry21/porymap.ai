#pragma once

#include <QDockWidget>

class ProjectPaths;
class ScriptEditorWidget;

class ScriptEditorDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ScriptEditorDock(QWidget *parent = nullptr);

    ScriptEditorWidget *editorWidget() const;
    void setProjectPaths(ProjectPaths *paths);

signals:
    void scriptFilesChanged();

public slots:
    void setProjectRoot(const QString &rootPath);

private:
    ScriptEditorWidget *m_editorWidget;
};

