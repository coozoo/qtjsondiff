// Black-box tests for QJsonDiff comparison logic.
// Drives QJsonDiff end-to-end (so a future refactor that moves the
// compare engine into its own threadable class keeps passing as long
// as observable item colors and idxRelation cross-links stay intact).
//
// Asserts only on:
//   - QJsonTreeItem::colorType() at named paths
//   - QJsonTreeItem::idxRelation() cross-link validity
//
// Does NOT touch QJsonDiff::compareModels / comparePath / compareValue /
// jsonPathList / findIndexInModel directly.

#include <QApplication>
#include <QTest>
#include <QWidget>
#include <QCheckBox>
#include <QSignalSpy>

#include "qjsondiff.h"
#include "qjsoncontainer.h"
#include "qjsonmodel.h"
#include "qjsonitem.h"
#include "preferences/preferences.h"

class TestCompare : public QObject
{
    Q_OBJECT

private:
    QWidget* host = nullptr;
    QJsonDiff* diff = nullptr;

    QModelIndex resolvePath(QJsonModel* model, const QStringList& keys)
    {
        QModelIndex parent;
        for (const QString& key : keys)
        {
            bool found = false;
            for (int i = 0; i < model->rowCount(parent); ++i)
            {
                QModelIndex idx = model->index(i, 0, parent);
                if (model->itemFromIndex(idx)->key() == key)
                {
                    parent = idx;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return QModelIndex();
            }
        }
        return parent;
    }

    DiffColorType colorAt(QJsonModel* model, const QStringList& path)
    {
        QModelIndex idx = resolvePath(model, path);
        if (!idx.isValid())
        {
            return DiffColorType::None;
        }
        return model->itemFromIndex(idx)->colorType();
    }

    bool exists(QJsonModel* model, const QStringList& path)
    {
        return resolvePath(model, path).isValid();
    }

    void loadAndCompare(const QString& leftJson,
                        const QString& rightJson,
                        bool useFullPath)
    {
        diff->useFullPath_checkbox->setChecked(useFullPath);
        diff->loadJsonLeft(QJsonDocument::fromJson(leftJson.toUtf8()));
        diff->loadJsonRight(QJsonDocument::fromJson(rightJson.toUtf8()));
        diff->startComparison();
    }

private slots:

    void initTestCase()
    {
        host = new QWidget;
        diff = new QJsonDiff(host);
    }

    void cleanupTestCase()
    {
        delete host; // owns diff and the inner containers via Qt parentage
        host = nullptr;
        diff = nullptr;
    }

    // ------------------------------------------------------------------
    // Identical JSONs — both modes
    // ------------------------------------------------------------------

    void identicalFullPath()
    {
        loadAndCompare(R"({"a":1,"b":"x"})", R"({"a":1,"b":"x"})", true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"b"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"b"}), DiffColorType::Identical);
    }

    void identicalParentChild()
    {
        loadAndCompare(R"({"a":1,"b":"x"})", R"({"a":1,"b":"x"})", false);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"b"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"b"}), DiffColorType::Identical);
    }

    // ------------------------------------------------------------------
    // Single nested scalar diff — ancestors should be promoted to Moderate
    // ------------------------------------------------------------------

    void nestedScalarDiffFullPath()
    {
        loadAndCompare(R"({"obj":{"a":"old"}})",
                       R"({"obj":{"a":"new"}})",
                       true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"obj", "a"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"obj", "a"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"obj"}), DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"obj"}), DiffColorType::Moderate);
    }

    void nestedScalarDiffParentChild()
    {
        loadAndCompare(R"({"obj":{"a":"old"}})",
                       R"({"obj":{"a":"new"}})",
                       false);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"obj", "a"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"obj", "a"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"obj"}), DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"obj"}), DiffColorType::Moderate);
    }

    // ------------------------------------------------------------------
    // Key present only on one side
    // ------------------------------------------------------------------

    void missingKeyFullPath()
    {
        loadAndCompare(R"({"a":1,"b":2})", R"({"a":1})", true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"b"}), DiffColorType::NotPresent);
        QVERIFY(!exists(diff->getRightJsonModel(), {"b"}));
    }

    void missingKeyParentChild()
    {
        loadAndCompare(R"({"a":1,"b":2})", R"({"a":1})", false);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"b"}), DiffColorType::NotPresent);
    }

    // ------------------------------------------------------------------
    // Same key path, different value type.
    // Full-path mode treats this as missing on both sides (path includes
    // type — per README). Parent+child mode treats it as a Huge diff
    // because the second pass in findIndexInModel matches on key/parent
    // alone after the typed match fails.
    // These are the *currently observable* contracts; lock them in.
    // ------------------------------------------------------------------

    void sameKeyDifferentTypeFullPath()
    {
        loadAndCompare(R"({"k":1})", R"({"k":"1"})", true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"k"}), DiffColorType::NotPresent);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"k"}), DiffColorType::NotPresent);
    }

    void sameKeyDifferentTypeParentChild()
    {
        loadAndCompare(R"({"k":1})", R"({"k":"1"})", false);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"k"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"k"}), DiffColorType::Huge);
    }

    // ------------------------------------------------------------------
    // Array length difference
    // ------------------------------------------------------------------

    void arrayLengthDiffFullPath()
    {
        loadAndCompare(R"({"arr":[1,2,3]})", R"({"arr":[1,2]})", true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"arr"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr", "0"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr", "1"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr", "2"}), DiffColorType::NotPresent);
        QVERIFY(!exists(diff->getRightJsonModel(), {"arr", "2"}));
    }

    void arrayLengthDiffParentChild()
    {
        loadAndCompare(R"({"arr":[1,2,3]})", R"({"arr":[1,2]})", false);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"arr"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr", "0"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr", "1"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"arr", "2"}), DiffColorType::NotPresent);
    }

    // ------------------------------------------------------------------
    // Deeply nested structural difference — ancestor chain promoted
    // ------------------------------------------------------------------

    void deepNestedDiffFullPath()
    {
        loadAndCompare(R"({"a":{"b":{"c":"old"}}})",
                       R"({"a":{"b":{"c":"new"}}})",
                       true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a", "b", "c"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a", "b", "c"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a", "b"}), DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}),      DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a", "b"}), DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a"}),      DiffColorType::Moderate);
    }

    void deepNestedDiffParentChild()
    {
        loadAndCompare(R"({"a":{"b":{"c":"old"}}})",
                       R"({"a":{"b":{"c":"new"}}})",
                       false);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a", "b", "c"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a", "b", "c"}), DiffColorType::Huge);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a", "b"}), DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}),      DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a", "b"}), DiffColorType::Moderate);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"a"}),      DiffColorType::Moderate);
    }

    // ------------------------------------------------------------------
    // idxRelation cross-link
    // ------------------------------------------------------------------

    void idxRelationCrossLinkFullPath()
    {
        loadAndCompare(R"({"a":1})", R"({"a":1})", true);
        QJsonModel* L = diff->getLeftJsonModel();
        QJsonModel* R = diff->getRightJsonModel();

        QModelIndex leftA  = resolvePath(L, {"a"});
        QModelIndex rightA = resolvePath(R, {"a"});
        QVERIFY(leftA.isValid());
        QVERIFY(rightA.isValid());

        QJsonTreeItem* leftItem  = L->itemFromIndex(leftA);
        QJsonTreeItem* rightItem = R->itemFromIndex(rightA);

        QVERIFY(leftItem->idxRelation().isValid());
        QVERIFY(rightItem->idxRelation().isValid());
        QCOMPARE(R->itemFromIndex(leftItem->idxRelation())->key(),  QString("a"));
        QCOMPARE(L->itemFromIndex(rightItem->idxRelation())->key(), QString("a"));
    }

    void idxRelationCrossLinkParentChild()
    {
        loadAndCompare(R"({"a":1})", R"({"a":1})", false);
        QJsonModel* L = diff->getLeftJsonModel();
        QJsonModel* R = diff->getRightJsonModel();

        QJsonTreeItem* leftItem  = L->itemFromIndex(resolvePath(L, {"a"}));
        QJsonTreeItem* rightItem = R->itemFromIndex(resolvePath(R, {"a"}));

        QVERIFY(leftItem->idxRelation().isValid());
        QVERIFY(rightItem->idxRelation().isValid());
        QCOMPARE(R->itemFromIndex(leftItem->idxRelation())->key(),  QString("a"));
        QCOMPARE(L->itemFromIndex(rightItem->idxRelation())->key(), QString("a"));
    }

    // ------------------------------------------------------------------
    // Re-running compare on fresh data resets prior state
    // ------------------------------------------------------------------

    void rerunResetsState()
    {
        loadAndCompare(R"({"a":1})", R"({"b":2})", true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}), DiffColorType::NotPresent);

        loadAndCompare(R"({"a":1})", R"({"a":1})", true);
        QCOMPARE(colorAt(diff->getLeftJsonModel(),  {"a"}), DiffColorType::Identical);
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestCompare tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_compare.moc"
