#include "extensions/scripts/Services/ProjectPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

ProjectPaths::ProjectPaths(QObject *parent)
    : QObject(parent)
{
    autoDetectCompiler();
}

QString ProjectPaths::projectRoot() const
{
    return m_projectRoot;
}

void ProjectPaths::setProjectRoot(const QString &rootPath)
{
    if (m_projectRoot == rootPath)
        return;

    m_projectRoot = rootPath;

    if (!m_projectRoot.isEmpty() && m_poryscriptCompilerPath.isEmpty()) {
        const QStringList projectCandidates = {
            QDir(m_projectRoot).filePath(QStringLiteral("tools/poryscript")),
            QDir(m_projectRoot).filePath(QStringLiteral("tools/poryscript.exe"))
        };
        for (const QString &candidate : projectCandidates) {
            const QFileInfo info(candidate);
            if (info.exists() && info.isFile()) {
                m_poryscriptCompilerPath = info.absoluteFilePath();
                break;
            }
        }
    }

    emit projectRootChanged(m_projectRoot);
}

QStringList ProjectPaths::scriptSearchRoots() const
{
    QStringList roots;
    if (!m_projectRoot.isEmpty()) {
        static const QStringList defaultSubdirs = {
            QStringLiteral("data/scripts"),
            QStringLiteral("src"),
            QStringLiteral("include"),
            QStringLiteral("include/constants"),
            QStringLiteral("data/text"),
            QStringLiteral("maps")
        };

        roots.reserve(defaultSubdirs.size());
        for (const QString &subdir : defaultSubdirs) {
            const QDir dir(m_projectRoot + QLatin1Char('/') + subdir);
            if (dir.exists())
                roots.append(dir.absolutePath());
        }
    }
    return roots;
}

QString ProjectPaths::poryscriptCompilerPath() const
{
    return m_poryscriptCompilerPath;
}

void ProjectPaths::setPoryscriptCompilerPath(const QString &path)
{
    if (m_poryscriptCompilerPath == path)
        return;

    m_poryscriptCompilerPath = path;
}

void ProjectPaths::autoDetectCompiler()
{
    const QStringList candidates = {
        QStringLiteral("poryscript"),
        QStringLiteral("poryscript.exe"),
        QStringLiteral("tools/poryscript"),
        QStringLiteral("tools/poryscript.exe"),
    };

    for (const QString &candidate : candidates) {
        const QString absolute = QFileInfo(candidate).exists()
            ? QFileInfo(candidate).absoluteFilePath()
            : QString();
        if (!absolute.isEmpty()) {
            m_poryscriptCompilerPath = absolute;
            return;
        }
    }

    // As a fallback, look in the user's PATH via common installation directories.
    const QStringList dataLocations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString &location : dataLocations) {
        const QFileInfo info(QDir(location).filePath(QStringLiteral("poryscript")));
        if (info.exists()) {
            m_poryscriptCompilerPath = info.absoluteFilePath();
            return;
        }
    }

    m_poryscriptCompilerPath.clear();
}

