#pragma once

#include <QObject>
#include <QStringList>

#include "extensions/scripts/ProblemsView.h"

class ProjectPaths;
class QProcess;

class BuildRunner : public QObject
{
    Q_OBJECT

public:
    explicit BuildRunner(QObject *parent = nullptr);
    ~BuildRunner() override;

    void setProjectPaths(ProjectPaths *paths);
    bool isRunning() const;

public slots:
    void buildPoryOnly();
    void buildAll();
    void cancel();

signals:
    void buildStarted();
    void buildFinished(int exitCode);
    void outputLine(const QString &line);
    void diagnosticsReady(const QList<ProblemsView::Problem> &diagnostics);
    void errorOccurred(const QString &message);

private slots:
    void processReadyRead();
    void processErrorOccurred(QProcess::ProcessError error);
    void processFinished(int exitCode, QProcess::ExitStatus status);

private:
    enum class BuildType {
        PoryOnly,
        Full,
    };

    ProjectPaths *m_paths = nullptr;
    QProcess *m_process = nullptr;
    BuildType m_currentType = BuildType::Full;
    QString m_buffer;

    void startBuild(BuildType type);
    QStringList buildCommand(BuildType type) const;
    void parseOutputLine(const QString &line, QList<ProblemsView::Problem> &diagnostics) const;
};

