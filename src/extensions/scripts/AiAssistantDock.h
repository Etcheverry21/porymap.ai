#pragma once

#include <QDockWidget>

class ProjectPaths;
class AiAssistantWidget;

class AiAssistantDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit AiAssistantDock(QWidget *parent = nullptr);

    AiAssistantWidget *assistantWidget() const;
    void setProjectPaths(ProjectPaths *paths);

signals:
    void requestInsertToEditor(const QString &text);
    void requestAttachToEvent(const QString &text);

public slots:
    void setProjectRoot(const QString &rootPath);

private:
    AiAssistantWidget *m_assistantWidget;
};

