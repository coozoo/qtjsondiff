// Regression tests for QJsonContainer::findModelText.
// Locks in observable search behavior so upcoming ghost-row work
// doesn't silently break find.

#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QAction>
#include <QTest>

#include "qjsoncontainer.h"
#include "qjsonmodel.h"
#include "qjsonitem.h"

class TestSearch : public QObject
{
    Q_OBJECT

private:
    QWidget* host = nullptr;
    QJsonContainer* cont = nullptr;

    void load(const QByteArray& json)
    {
        cont->loadJson(QString::fromUtf8(json));
    }

    void setQuery(const QString& text, bool caseSensitive = false)
    {
        cont->find_lineEdit->setText(text);
        cont->findCaseSensitivity_toolbutton->setChecked(caseSensitive);
    }

    // Full "key/key/key" path from root down to idx (column 0).
    QStringList indexPath(const QModelIndex& idx) const
    {
        QStringList out;
        QModelIndex cur = idx;
        while (cur.isValid())
        {
            const QJsonTreeItem* item =
                static_cast<QJsonTreeItem*>(cur.internalPointer());
            out.prepend(item ? item->key() : QString());
            cur = cur.parent();
        }
        return out;
    }

    QStringList pathsOfMatches(const QList<QModelIndex>& matches) const
    {
        QStringList out;
        for (const QModelIndex& idx : matches)
        {
            out << indexPath(idx).join(QLatin1Char('/'));
        }
        return out;
    }

private slots:
    void init()
    {
        // QJsonContainer's ctor calls parent->setLayout(...) so parent
        // must be non-null.
        host = new QWidget;
        cont = new QJsonContainer(host);
    }

    void cleanup()
    {
        delete host;   // owns cont via child parenting
        host = nullptr;
        cont = nullptr;
    }

    void matchesKeyOnly()
    {
        load(R"({"alpha": "one", "beta": "two"})");
        setQuery("alpha");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
        QCOMPARE(indexPath(matches.first()), (QStringList{"alpha"}));
    }

    void matchesValueOnly()
    {
        load(R"({"alpha": "carrot", "beta": "banana"})");
        setQuery("banana");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
        QCOMPARE(indexPath(matches.first()), (QStringList{"beta"}));
    }

    void matchesSpanningKeyPipeValue()
    {
        // findModelText concatenates "key|value" internally, so a
        // query containing the pipe is a load-bearing search shape
        // and we want to preserve it.
        load(R"({"foo": "barbaz"})");
        setQuery("foo|bar");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
        QCOMPARE(indexPath(matches.first()), (QStringList{"foo"}));
    }

    void caseInsensitiveByDefault()
    {
        load(R"({"UPPER": "lower"})");
        setQuery("upper");   // caseSensitive=false
        auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);

        setQuery("LoWeR");   // still case-insensitive
        matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
    }

    void caseSensitiveMisses()
    {
        load(R"({"UPPER": "lower"})");
        setQuery("upper", /*caseSensitive=*/true);
        auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 0);

        setQuery("UPPER", /*caseSensitive=*/true);
        matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
    }

    void emptyOnNoMatches()
    {
        load(R"({"alpha": "one"})");
        setQuery("nothingmatches");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 0);
    }

    void recursesIntoNestedObjects()
    {
        load(R"({"outer": {"inner": "leaf"}})");
        setQuery("leaf");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
        QCOMPARE(indexPath(matches.first()),
                 (QStringList{"outer", "inner"}));
    }

    void recursesIntoArrayChildren()
    {
        // Array children have positional keys ("0", "1", ...).
        // Search should still find matches by their scalar value.
        load(R"({"tags": ["ready", "hiking", "quiet"]})");
        setQuery("hiking");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
        QCOMPARE(indexPath(matches.first()),
                 (QStringList{"tags", "1"}));
    }

    void preorderParentBeforeChild()
    {
        // Query hits both a parent (by key) and its child (by key).
        // Result list order is parent-first: current DFS is preorder,
        // and the alignment work must preserve that ordering.
        load(R"({"foo": {"foo": {"foo": "leaf"}}})");
        setQuery("foo");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        const QStringList paths = pathsOfMatches(matches);
        QCOMPARE(paths.size(), 3);
        QCOMPARE(paths[0], QStringLiteral("foo"));
        QCOMPARE(paths[1], QStringLiteral("foo/foo"));
        QCOMPARE(paths[2], QStringLiteral("foo/foo/foo"));
    }

    void multipleTopLevelMatchesInDocumentOrder()
    {
        // Sibling matches come out in row order (top to bottom).
        load(R"({"a_key": 1, "unrelated": 2, "b_key": 3, "c_key": 4})");
        setQuery("_key");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        const QStringList paths = pathsOfMatches(matches);
        QCOMPARE(paths.size(), 3);
        QCOMPARE(paths[0], QStringLiteral("a_key"));
        QCOMPARE(paths[1], QStringLiteral("b_key"));
        QCOMPARE(paths[2], QStringLiteral("c_key"));
    }

    // Find a top-level child by key.
    QJsonTreeItem* findTopLevel(const QString &key) const
    {
        const int n = cont->model->rowCount();
        for (int i = 0; i < n; ++i)
        {
            QModelIndex idx = cont->model->index(i, 0);
            QJsonTreeItem *item = static_cast<QJsonTreeItem*>(idx.internalPointer());
            if (item && item->key() == key)
                return item;
        }
        return nullptr;
    }

    void findModelTextSkipsPhantoms()
    {
        // If a matching row is flagged as phantom, it must NOT appear
        // in the search results — phantoms are alignment aids, not
        // user-visible content.
        load(R"({"real_hit": "yes", "ghost_hit": "yes"})");
        QJsonTreeItem *ghost = findTopLevel("ghost_hit");
        QVERIFY(ghost);
        ghost->setPhantom(true);

        setQuery("hit");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 1);
        QCOMPARE(indexPath(matches.first()), (QStringList{"real_hit"}));
    }

    void findModelTextSkipsChildrenUnderPhantomParent()
    {
        // A phantom parent hides its whole subtree from search (the
        // parent branch bails before recursing).
        load(R"({"outer": {"buried": "hit"}})");
        QJsonTreeItem *outer = findTopLevel("outer");
        QVERIFY(outer);
        outer->setPhantom(true);

        setQuery("hit");
        const auto matches = cont->findModelText(cont->model, QModelIndex());
        QCOMPARE(matches.size(), 0);
    }

    void fillGotoListSkipsPhantoms()
    {
        // fillGotoList collects non-Identical/None scalar rows for
        // next-diff navigation. Phantoms qualify by shape (NotPresent
        // scalar), so an explicit skip is needed.
        load(R"({"real": "x", "ghost": "y"})");
        QJsonTreeItem *real  = findTopLevel("real");
        QJsonTreeItem *ghost = findTopLevel("ghost");
        QVERIFY(real);
        QVERIFY(ghost);
        // Both start life color-less; paint them so they meet the
        // "diff worth visiting" predicate.
        real->setColorType(DiffColorType::Huge);
        ghost->setColorType(DiffColorType::NotPresent);
        ghost->setPhantom(true);

        const auto goList = cont->fillGotoList(cont->model, QModelIndex());
        QCOMPARE(goList.size(), 1);
        QCOMPARE(static_cast<QJsonTreeItem*>(goList.first().internalPointer())->key(),
                 QStringLiteral("real"));
    }
};

QTEST_MAIN(TestSearch)
#include "test_search.moc"
