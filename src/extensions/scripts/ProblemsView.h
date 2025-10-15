#pragma once

#include <QWidget>

#include <QList>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

class ProblemsView : public QWidget
{
    Q_OBJECT

public:
    enum class Severity {
        Info,
        Warning,
        Error,
    };

    struct Problem {
        Severity severity = Severity::Info;
        QString file;
        int line = -1;
        int column = -1;
        QString message;
    };

    explicit ProblemsView(QWidget *parent = nullptr);

    void setProblems(const QList<Problem> &problems);
    void addProblem(const Problem &problem);
    void clear();

signals:
    void problemActivated(const QString &file, int line, int column);

private slots:
    void onItemActivated(QTreeWidgetItem *item, int column);

private:
    QTreeWidget *m_tree;

    static QString severityLabel(Severity severity);
    static QString severityIconName(Severity severity);
    static void applyProblemToItem(const Problem &problem, QTreeWidgetItem *item);
};

