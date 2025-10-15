#pragma once

#include <QWidget>

class ProjectPaths;

class ScriptEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScriptEditorWidget(QWidget *parent = nullptr);

    void setProjectPaths(ProjectPaths *paths);

signals:
    void scriptFilesChanged();

public slots:
    void setProjectContext(const QString &projectRoot);
    void setActiveMapContext(const QString &mapName);

private:
    ProjectPaths *m_projectPaths;
};

