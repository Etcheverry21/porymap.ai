#include "extensions/scripts/BuildRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include "extensions/scripts/Services/ProjectPaths.h"

BuildRunner::BuildRunner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &BuildRunner::processReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &BuildRunner::processReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, &BuildRunner::processErrorOccurred);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildRunner::processFinished);
}

BuildRunner::~BuildRunner() = default;

void BuildRunner::setProjectPaths(ProjectPaths *paths)
{
    m_paths = paths;
}

bool BuildRunner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void BuildRunner::buildPoryOnly()
{
    startBuild(BuildType::PoryOnly);
}

void BuildRunner::buildAll()
{
    startBuild(BuildType::Full);
}

void BuildRunner::cancel()
{
    if (!isRunning())
        return;

    m_process->kill();
    m_process->waitForFinished(1000);
}

void BuildRunner::processReadyRead()
{
    m_buffer.append(QString::fromLocal8Bit(m_process->readAllStandardOutput()));

    QList<ProblemsView::Problem> diagnostics;
    int newlineIndex = -1;
    while ((newlineIndex = m_buffer.indexOf(QLatin1Char('\n'))) >= 0) {
        const QString line = m_buffer.left(newlineIndex);
        m_buffer.remove(0, newlineIndex + 1);

        emit outputLine(line);
        parseOutputLine(line, diagnostics);
    }

    if (!diagnostics.isEmpty())
        emit diagnosticsReady(diagnostics);
}

void BuildRunner::processErrorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_process->errorString());
}

void BuildRunner::processFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_buffer.isEmpty()) {
        emit outputLine(m_buffer);
        QList<ProblemsView::Problem> diagnostics;
        parseOutputLine(m_buffer, diagnostics);
        if (!diagnostics.isEmpty())
            emit diagnosticsReady(diagnostics);
        m_buffer.clear();
    }

    emit buildFinished(status == QProcess::NormalExit ? exitCode : -1);
}

void BuildRunner::startBuild(BuildType type)
{
    if (!m_paths) {
        emit errorOccurred(tr("No project configured for builds."));
        return;
    }

    if (isRunning())
        return;

    const QStringList command = buildCommand(type);
    if (command.isEmpty()) {
        emit errorOccurred(tr("Unable to determine build command."));
        return;
    }

    emit buildStarted();
    m_currentType = type;
    m_buffer.clear();

    QString program = command.first();
    QStringList arguments = command.mid(1);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(m_paths->projectRoot());
    m_process->start(program, arguments);
}

QStringList BuildRunner::buildCommand(BuildType type) const
{
    if (!m_paths)
        return {};

    const QString root = m_paths->projectRoot();
    if (root.isEmpty())
        return {};

    const QFileInfo ninjaFile(QDir(root).filePath(QStringLiteral("build.ninja")));
    const QFileInfo makefile(QDir(root).filePath(QStringLiteral("Makefile")));
    const QFileInfo gnumakefile(QDir(root).filePath(QStringLiteral("GNUmakefile")));

    const bool hasNinja = ninjaFile.exists();
    const bool hasMake = makefile.exists() || gnumakefile.exists();

    QStringList command;
    if (hasNinja) {
        command << QStringLiteral("ninja") << QStringLiteral("-C") << root;
        if (type == BuildType::PoryOnly)
            command << QStringLiteral("poryscript");
    } else if (hasMake) {
        command << QStringLiteral("make");
        command << QStringLiteral("-C") << root;
        if (type == BuildType::PoryOnly)
            command << QStringLiteral("poryscript");
    } else {
        // Fallback to invoking make in the root regardless.
        command << QStringLiteral("make");
        if (type == BuildType::PoryOnly)
            command << QStringLiteral("poryscript");
    }

    return command;
}

void BuildRunner::parseOutputLine(const QString &line, QList<ProblemsView::Problem> &diagnostics) const
{
    static const QRegularExpression regex(
        QStringLiteral("^([^:\\n]+):(\\d+)(?::(\\d+))?:\\s*(warning|error):\\s*(.+)$"));
    const QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch())
        return;

    ProblemsView::Problem problem;
    problem.file = match.captured(1);
    problem.line = match.captured(2).toInt();
    problem.column = match.captured(3).toInt();
    const QString severity = match.captured(4);
    if (severity.compare(QStringLiteral("warning"), Qt::CaseInsensitive) == 0)
        problem.severity = ProblemsView::Severity::Warning;
    else
        problem.severity = ProblemsView::Severity::Error;
    problem.message = match.captured(5);
    diagnostics.append(problem);
}

