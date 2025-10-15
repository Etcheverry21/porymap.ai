#pragma once

#include <QWidget>

class ProjectPaths;

class AiAssistantWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AiAssistantWidget(QWidget *parent = nullptr);

    void setProjectPaths(ProjectPaths *paths);

signals:
    void requestInsertToEditor(const QString &text);
    void requestAttachToEvent(const QString &text);

public slots:
    void setProjectContext(const QString &projectRoot);
    void setActiveMapContext(const QString &mapName);

private:
    ProjectPaths *m_projectPaths;
};

