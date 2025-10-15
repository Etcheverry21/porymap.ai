#include "extensions/scripts/WorkspaceIndex.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QReadLocker>
#include <QRegularExpression>
#include <QTextStream>
#include <QWriteLocker>
#include <QtConcurrentRun>

#include "extensions/scripts/Services/ProjectPaths.h"

namespace {
const QStringList kFileExtensions = {
    QStringLiteral(".pory"), QStringLiteral(".c"), QStringLiteral(".h"), QStringLiteral(".inc"),
    QStringLiteral(".s"), QStringLiteral(".json")
};
}

WorkspaceIndex::WorkspaceIndex(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFutureWatcher<IndexData>())
{
    connect(m_watcher.data(), &QFutureWatcher<IndexData>::finished, this, [this]() {
        const IndexData data = m_watcher->future().result();
        {
            QWriteLocker locker(&m_lock);
            m_symbols = data.symbols;
            m_labels = data.labels;
        }
        emit indexingFinished();
    });
}

WorkspaceIndex::~WorkspaceIndex()
{
    if (m_watcher && m_watcher->isRunning()) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
    }
}

void WorkspaceIndex::setProjectPaths(ProjectPaths *paths)
{
    m_paths = paths;
}

void WorkspaceIndex::setRoots(const QStringList &roots)
{
    if (m_roots == roots)
        return;

    m_roots = roots;
    scheduleRebuild();
}

QList<WorkspaceIndex::Location> WorkspaceIndex::findSymbol(const QString &name) const
{
    QReadLocker locker(&m_lock);
    return m_symbols.value(name);
}

QList<WorkspaceIndex::Location> WorkspaceIndex::findLabel(const QString &label) const
{
    QReadLocker locker(&m_lock);
    return m_labels.value(label);
}

QList<WorkspaceIndex::SearchHit> WorkspaceIndex::search(const QString &query) const
{
    QList<SearchHit> hits;
    if (query.isEmpty())
        return hits;

    QStringList roots;
    {
        QReadLocker locker(&m_lock);
        roots = m_roots;
    }
    for (const QString &root : roots) {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            bool extensionMatches = false;
            for (const QString &ext : kFileExtensions) {
                if (path.endsWith(ext)) {
                    extensionMatches = true;
                    break;
                }
            }
            if (!extensionMatches)
                continue;

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream stream(&file);
            int lineNumber = 0;
            while (!stream.atEnd()) {
                ++lineNumber;
                const QString line = stream.readLine();
                if (line.contains(query, Qt::CaseInsensitive)) {
                    SearchHit hit;
                    hit.filePath = path;
                    hit.line = lineNumber;
                    hit.lineText = line.trimmed();
                    hits.append(hit);
                }
            }
        }
    }

    return hits;
}

void WorkspaceIndex::scheduleRebuild()
{
    if (!m_watcher)
        return;

    if (m_watcher->isRunning()) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
    }

    if (m_roots.isEmpty()) {
        QWriteLocker locker(&m_lock);
        m_symbols.clear();
        m_labels.clear();
        emit indexingFinished();
        return;
    }

    emit indexingStarted();
    const QStringList roots = m_roots;
    auto future = QtConcurrent::run([roots]() { return buildIndex(roots); });
    m_watcher->setFuture(future);
}

WorkspaceIndex::IndexData WorkspaceIndex::buildIndex(const QStringList &roots)
{
    IndexData data;
    for (const QString &root : roots) {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            bool matches = false;
            for (const QString &ext : kFileExtensions) {
                if (path.endsWith(ext)) {
                    matches = true;
                    break;
                }
            }
            if (!matches)
                continue;

            indexFile(path, data);
        }
    }
    return data;
}

void WorkspaceIndex::indexFile(const QString &filePath, IndexData &data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    int lineNumber = 0;

    static const QRegularExpression symbolRegex(QStringLiteral("\\b([A-Z][A-Z0-9_]+)\\b"));
    static const QRegularExpression labelRegex(QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*):"));

    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine();

        auto symbolIt = symbolRegex.globalMatch(line);
        while (symbolIt.hasNext()) {
            const QRegularExpressionMatch match = symbolIt.next();
            Location loc;
            loc.filePath = filePath;
            loc.line = lineNumber;
            loc.column = match.capturedStart(1) + 1;
            loc.preview = line.trimmed();
            data.symbols[match.captured(1)].append(loc);
        }

        const QRegularExpressionMatch labelMatch = labelRegex.match(line);
        if (labelMatch.hasMatch()) {
            Location loc;
            loc.filePath = filePath;
            loc.line = lineNumber;
            loc.column = labelMatch.capturedStart(1) + 1;
            loc.preview = line.trimmed();
            data.labels[labelMatch.captured(1)].append(loc);
        }
    }
}

