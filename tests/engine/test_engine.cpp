// Engine-direct tests for JsonDiffEngine.
//
// Drives the engine on hand-built DiffNode snapshots — no QJsonDiff
// widget, no QApplication, no QtNetwork. These exist so the algorithm
// has a fast, no-GUI test surface independent of the widget tests.
//
// Snapshot and apply paths are also exercised against a real QJsonModel
// since those are the model-touching halves of the engine API.

#include <QTest>
#include <QSignalSpy>
#include <QThread>

#include "jsondiffengine.h"
#include "qjsonmodel.h"
#include "qjsonitem.h"

namespace
{
// RAII guard: stops a worker QThread on scope exit. Used by the async
// tests so a QVERIFY-early-return path can't leave the thread running —
// otherwise the QThread destructor would QFATAL with "Destroyed while
// thread is still running" and trash the next test.
struct ThreadStopper
{
    QThread &t;
    ~ThreadStopper() { t.quit(); t.wait(); }
};
} // namespace

class TestEngine : public QObject
{
    Q_OBJECT

private:
    static DiffNode makeRoot(QJsonValue::Type type = QJsonValue::Object)
    {
        DiffNode n;
        n.key  = "root";
        n.type = type;
        return n;
    }

    static DiffNode makeScalar(const QString &key, QJsonValue::Type type, const QString &value)
    {
        DiffNode n;
        n.key   = key;
        n.type  = type;
        n.value = value;
        return n;
    }

    static DiffNode makeContainer(const QString &key, QJsonValue::Type type)
    {
        DiffNode n;
        n.key  = key;
        n.type = type;
        return n;
    }

    // Find a child by key. Returns nullptr if not found.
    static DiffNode* childByKey(DiffNode &parent, const QString &key)
    {
        for (DiffNode &c : parent.children)
        {
            if (c.key == key) return &c;
        }
        return nullptr;
    }

private slots:

    // ------------------------------------------------------------------
    // snapshot() from a real QJsonModel
    // ------------------------------------------------------------------

    void snapshotEmptyObject()
    {
        QJsonModel m;
        m.loadJson(R"({})");
        DiffNode s = JsonDiffEngine::snapshot(&m);
        QCOMPARE(s.key,  QString("root"));
        QCOMPARE(s.type, QJsonValue::Object);
        QCOMPARE(s.children.size(), 0);
    }

    void snapshotArrayRoot()
    {
        QJsonModel m;
        m.loadJson(R"([1,2,3])");
        DiffNode s = JsonDiffEngine::snapshot(&m);
        QCOMPARE(s.type, QJsonValue::Array);
        QCOMPARE(s.children.size(), 3);
        QCOMPARE(s.children[0].key,   QString("0"));
        QCOMPARE(s.children[0].type,  QJsonValue::Double);
        QCOMPARE(s.children[0].value, QString("1"));
    }

    void snapshotNested()
    {
        QJsonModel m;
        m.loadJson(R"({"obj":{"a":"old","n":42},"arr":[true,null]})");
        DiffNode s = JsonDiffEngine::snapshot(&m);

        QCOMPARE(s.children.size(), 2);

        DiffNode *obj = childByKey(s, "obj");
        QVERIFY(obj);
        QCOMPARE(obj->type, QJsonValue::Object);
        QCOMPARE(obj->children.size(), 2);
        DiffNode *a = childByKey(*obj, "a");
        QVERIFY(a);
        QCOMPARE(a->type,  QJsonValue::String);
        QCOMPARE(a->value, QString("old"));
        DiffNode *n = childByKey(*obj, "n");
        QVERIFY(n);
        QCOMPARE(n->type,  QJsonValue::Double);
        QCOMPARE(n->value, QString("42"));

        DiffNode *arr = childByKey(s, "arr");
        QVERIFY(arr);
        QCOMPARE(arr->type, QJsonValue::Array);
        QCOMPARE(arr->children.size(), 2);
        QCOMPARE(arr->children[0].type, QJsonValue::Bool);
        QCOMPARE(arr->children[1].type, QJsonValue::Null);
    }

    // ------------------------------------------------------------------
    // compare() — hand-built snapshots, both modes
    // ------------------------------------------------------------------

    void identicalScalars_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("full")        << int(JsonDiffEngine::Mode::FullPath);
        QTest::newRow("parentchild") << int(JsonDiffEngine::Mode::ParentChildPair);
    }

    void identicalScalars()
    {
        QFETCH(int, mode);
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::Double, "1"));
        DiffNode R = L;

        JsonDiffEngine::compare(L, R, static_cast<JsonDiffEngine::Mode>(mode));
        QCOMPARE(L.children[0].color, DiffColorType::Identical);
        QCOMPARE(R.children[0].color, DiffColorType::Identical);
        QVERIFY(!L.children[0].relationPath.isEmpty());
        QVERIFY(!R.children[0].relationPath.isEmpty());
    }

    void scalarValueDiff_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("full")        << int(JsonDiffEngine::Mode::FullPath);
        QTest::newRow("parentchild") << int(JsonDiffEngine::Mode::ParentChildPair);
    }

    void scalarValueDiff()
    {
        QFETCH(int, mode);
        DiffNode L = makeRoot();
        L.children.append(makeScalar("k", QJsonValue::String, "old"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("k", QJsonValue::String, "new"));

        JsonDiffEngine::compare(L, R, static_cast<JsonDiffEngine::Mode>(mode));
        QCOMPARE(L.children[0].color, DiffColorType::Huge);
        QCOMPARE(R.children[0].color, DiffColorType::Huge);
    }

    void missingKey()
    {
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::Double, "1"));
        L.children.append(makeScalar("b", QJsonValue::Double, "2"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("a", QJsonValue::Double, "1"));

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color, DiffColorType::Identical);
        QCOMPARE(L.children[1].color, DiffColorType::NotPresent);
        QCOMPARE(R.children[0].color, DiffColorType::Identical);
    }

    // Full-path: type mismatch -> NotPresent (path includes type).
    // ParentChild: type mismatch -> Huge (second-stage key-only match).
    // Both behaviours are locked in here so a future refactor can't
    // silently change them.
    void sameKeyDifferentType_full()
    {
        DiffNode L = makeRoot();
        L.children.append(makeScalar("k", QJsonValue::Double, "1"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("k", QJsonValue::String, "1"));

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color, DiffColorType::NotPresent);
        QCOMPARE(R.children[0].color, DiffColorType::NotPresent);
    }

    void sameKeyDifferentType_parentchild()
    {
        DiffNode L = makeRoot();
        L.children.append(makeScalar("k", QJsonValue::Double, "1"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("k", QJsonValue::String, "1"));

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::ParentChildPair);
        QCOMPARE(L.children[0].color, DiffColorType::Huge);
        QCOMPARE(R.children[0].color, DiffColorType::Huge);
    }

    void arrayLengthDiff_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("full")        << int(JsonDiffEngine::Mode::FullPath);
        QTest::newRow("parentchild") << int(JsonDiffEngine::Mode::ParentChildPair);
    }

    void arrayLengthDiff()
    {
        QFETCH(int, mode);
        // {"arr":[1,2,3]} vs {"arr":[1,2]}
        DiffNode L = makeRoot();
        DiffNode arrL = makeContainer("arr", QJsonValue::Array);
        arrL.children.append(makeScalar("0", QJsonValue::Double, "1"));
        arrL.children.append(makeScalar("1", QJsonValue::Double, "2"));
        arrL.children.append(makeScalar("2", QJsonValue::Double, "3"));
        L.children.append(arrL);

        DiffNode R = makeRoot();
        DiffNode arrR = makeContainer("arr", QJsonValue::Array);
        arrR.children.append(makeScalar("0", QJsonValue::Double, "1"));
        arrR.children.append(makeScalar("1", QJsonValue::Double, "2"));
        R.children.append(arrR);

        JsonDiffEngine::compare(L, R, static_cast<JsonDiffEngine::Mode>(mode));

        QCOMPARE(L.children[0].color,             DiffColorType::Huge);  // arr
        QCOMPARE(R.children[0].color,             DiffColorType::Huge);
        QCOMPARE(L.children[0].children[0].color, DiffColorType::Identical);
        QCOMPARE(L.children[0].children[1].color, DiffColorType::Identical);
        QCOMPARE(L.children[0].children[2].color, DiffColorType::NotPresent);
    }

    void deepNestedDiff_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("full")        << int(JsonDiffEngine::Mode::FullPath);
        QTest::newRow("parentchild") << int(JsonDiffEngine::Mode::ParentChildPair);
    }

    void deepNestedDiff()
    {
        QFETCH(int, mode);
        // {"a":{"b":{"c":"old"}}} vs same with "new"
        auto build = [](const QString &leafVal) {
            DiffNode root = makeRoot();
            DiffNode a = makeContainer("a", QJsonValue::Object);
            DiffNode b = makeContainer("b", QJsonValue::Object);
            b.children.append(makeScalar("c", QJsonValue::String, leafVal));
            a.children.append(b);
            root.children.append(a);
            return root;
        };
        DiffNode L = build("old");
        DiffNode R = build("new");

        JsonDiffEngine::compare(L, R, static_cast<JsonDiffEngine::Mode>(mode));

        // c (leaf) -> Huge, ancestors -> Moderate.
        QCOMPARE(L.children[0].children[0].children[0].color, DiffColorType::Huge);
        QCOMPARE(L.children[0].children[0].color,             DiffColorType::Moderate);
        QCOMPARE(L.children[0].color,                         DiffColorType::Moderate);
        QCOMPARE(R.children[0].children[0].children[0].color, DiffColorType::Huge);
        QCOMPARE(R.children[0].children[0].color,             DiffColorType::Moderate);
        QCOMPARE(R.children[0].color,                         DiffColorType::Moderate);
    }

    void relationPathPointsAcross()
    {
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::Double, "1"));
        L.children.append(makeScalar("b", QJsonValue::String, "x"));
        DiffNode R = L;

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);

        QList<int> expectedA{0};
        QList<int> expectedB{1};
        QCOMPARE(L.children[0].relationPath, expectedA);  // left "a" -> right [0]
        QCOMPARE(L.children[1].relationPath, expectedB);  // left "b" -> right [1]
        QCOMPARE(R.children[0].relationPath, expectedA);
        QCOMPARE(R.children[1].relationPath, expectedB);
    }

    // ------------------------------------------------------------------
    // apply() — pushes snapshot state back onto a real QJsonModel
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // Async path: engine on a worker QThread, queued signal/slot.
    // Verifies cross-thread plumbing — DiffNode marshalled across thread
    // boundary, finished/cancelled/progressed routed via Qt::Queued.
    // ------------------------------------------------------------------

    // Build a small {root: {a: 1}} snapshot on the heap.
    static QSharedPointer<DiffNode> makeSharedRoot()
    {
        auto root = QSharedPointer<DiffNode>::create();
        root->key  = "root";
        root->type = QJsonValue::Object;
        root->children.append(makeScalar("a", QJsonValue::Double, "1"));
        return root;
    }


    void asyncCompareEmitsFinished()
    {
        JsonDiffEngine engine;
        QThread thread;
        ThreadStopper stopper{thread};
        engine.moveToThread(&thread);
        thread.start();

        auto L = makeSharedRoot();
        auto R = makeSharedRoot();

        QSignalSpy finishedSpy(&engine, &JsonDiffEngine::finished);
        QSignalSpy progressSpy(&engine, &JsonDiffEngine::progressed);

        QMetaObject::invokeMethod(&engine, "compareAsync", Qt::QueuedConnection,
            Q_ARG(QSharedPointer<DiffNode>, L),
            Q_ARG(QSharedPointer<DiffNode>, R),
            Q_ARG(JsonDiffEngine::Mode, JsonDiffEngine::Mode::FullPath));

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 1);

        // Same shared object came back — the trees were mutated in place,
        // not copied. Verify the colour reached our local pointer too.
        QCOMPARE(L->children[0].color, DiffColorType::Identical);

        // The signal's first argument is the same shared pointer.
        const QList<QVariant> args = finishedSpy.takeFirst();
        auto resultLeft = args[0].value<QSharedPointer<DiffNode>>();
        QVERIFY(resultLeft);
        QCOMPARE(resultLeft.data(), L.data());  // pointer equality

        QVERIFY(progressSpy.count() >= 1);
    }

    void asyncCancelEmitsCancelledNotFinished()
    {
        JsonDiffEngine engine;
        QThread thread;
        ThreadStopper stopper{thread};
        engine.moveToThread(&thread);
        thread.start();

        // Pre-set the cancel flag. compareAsync does NOT reset cancel
        // on entry; the engine sees it at the first tick() and aborts
        // via cancelled().
        engine.requestCancel();

        auto L = makeSharedRoot();
        auto R = makeSharedRoot();

        QSignalSpy finishedSpy(&engine,  &JsonDiffEngine::finished);
        QSignalSpy cancelledSpy(&engine, &JsonDiffEngine::cancelled);

        QMetaObject::invokeMethod(&engine, "compareAsync", Qt::QueuedConnection,
            Q_ARG(QSharedPointer<DiffNode>, L),
            Q_ARG(QSharedPointer<DiffNode>, R),
            Q_ARG(JsonDiffEngine::Mode, JsonDiffEngine::Mode::FullPath));

        QVERIFY(cancelledSpy.wait(5000));
        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);
    }

    void resetCancelAllowsFreshRun()
    {
        JsonDiffEngine engine;
        QThread thread;
        ThreadStopper stopper{thread};
        engine.moveToThread(&thread);
        thread.start();

        engine.requestCancel();
        engine.resetCancel();
        QVERIFY(!engine.cancelRequested());

        auto L = makeSharedRoot();
        auto R = makeSharedRoot();

        QSignalSpy finishedSpy(&engine, &JsonDiffEngine::finished);
        QMetaObject::invokeMethod(&engine, "compareAsync", Qt::QueuedConnection,
            Q_ARG(QSharedPointer<DiffNode>, L),
            Q_ARG(QSharedPointer<DiffNode>, R),
            Q_ARG(JsonDiffEngine::Mode, JsonDiffEngine::Mode::FullPath));

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 1);
    }

    void applyWritesColorAndRelation()
    {
        QJsonModel ml;
        ml.loadJson(R"({"a":1,"b":2})");
        QJsonModel mr;
        mr.loadJson(R"({"a":1})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        QModelIndex aL = ml.index(0, 0);
        QModelIndex bL = ml.index(1, 0);
        QCOMPARE(ml.itemFromIndex(aL)->colorType(), DiffColorType::Identical);
        QCOMPARE(ml.itemFromIndex(bL)->colorType(), DiffColorType::NotPresent);

        // Cross-link: left "a" -> right "a", round-trips.
        QModelIndex relA = ml.itemFromIndex(aL)->idxRelation();
        QVERIFY(relA.isValid());
        QCOMPARE(mr.itemFromIndex(relA)->key(), QString("a"));

        // Right "a" cross-links back to left "a".
        QModelIndex aR = mr.index(0, 0);
        QModelIndex backToA = mr.itemFromIndex(aR)->idxRelation();
        QVERIFY(backToA.isValid());
        QCOMPARE(ml.itemFromIndex(backToA)->key(), QString("a"));
    }
};

QTEST_GUILESS_MAIN(TestEngine)
#include "test_engine.moc"
