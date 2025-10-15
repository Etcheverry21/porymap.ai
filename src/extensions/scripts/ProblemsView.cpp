#include "extensions/scripts/ProblemsView.h"

#include <QHeaderView>
#include <QIcon>
#include <QStringList>
#include <QTreeWidget>
#include <QVBoxLayout>

ProblemsView::ProblemsView(QWidget *parent)
    : QWidget(parent)
    , m_tree(new QTreeWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);

    m_tree->setObjectName(QStringLiteral("problemsView"));
    m_tree->setColumnCount(4);
    QStringList headers;
    headers << tr("Severity") << tr("File") << tr("Line") << tr("Message");
    m_tree->setHeaderLabels(headers);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tree->setUniformRowHeights(true);
    m_tree->setRootIsDecorated(false);

    connect(m_tree, &QTreeWidget::itemActivated, this, &ProblemsView::onItemActivated);
    connect(m_tree, &QTreeWidget::itemClicked, this, &ProblemsView::onItemActivated);
}

void ProblemsView::setProblems(const QList<Problem> &problems)
{
    m_tree->clear();
    for (const Problem &problem : problems)
        addProblem(problem);
}

void ProblemsView::addProblem(const Problem &problem)
{
    auto *item = new QTreeWidgetItem(m_tree);
    applyProblemToItem(problem, item);
}

void ProblemsView::clear()
{
    m_tree->clear();
}

void ProblemsView::onItemActivated(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item)
        return;

    const QString file = item->data(0, Qt::UserRole + 1).toString();
    const int line = item->data(0, Qt::UserRole + 2).toInt();
    const int columnNumber = item->data(0, Qt::UserRole + 3).toInt();
    if (!file.isEmpty())
        emit problemActivated(file, line, columnNumber);
}

QString ProblemsView::severityLabel(Severity severity)
{
    switch (severity) {
    case Severity::Info:
        return QObject::tr("Info");
    case Severity::Warning:
        return QObject::tr("Warning");
    case Severity::Error:
        return QObject::tr("Error");
    }
    return QString();
}

QString ProblemsView::severityIconName(Severity severity)
{
    switch (severity) {
    case Severity::Info:
        return QStringLiteral("dialog-information");
    case Severity::Warning:
        return QStringLiteral("dialog-warning");
    case Severity::Error:
        return QStringLiteral("dialog-error");
    }
    return QString();
}

void ProblemsView::applyProblemToItem(const Problem &problem, QTreeWidgetItem *item)
{
    if (!item)
        return;

    item->setText(0, severityLabel(problem.severity));
    item->setText(1, problem.file);
    if (problem.line > 0)
        item->setText(2, QString::number(problem.line));
    else
        item->setText(2, QString());
    item->setText(3, problem.message);

    const QString iconName = severityIconName(problem.severity);
    if (!iconName.isEmpty())
        item->setIcon(0, QIcon::fromTheme(iconName));

    item->setData(0, Qt::UserRole + 1, problem.file);
    item->setData(0, Qt::UserRole + 2, problem.line);
    item->setData(0, Qt::UserRole + 3, problem.column);
}

