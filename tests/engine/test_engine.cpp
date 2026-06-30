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
    // Both behaviors are locked in here so a future refactor can't
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


    void asyncCompareEmitsPhases()
    {
        // compareAsync should emit phaseChanged at every algorithm
        // boundary. FullPath: Building paths, Matching paths,
        // Comparing values, Resolving colors. ParentChild: Indexing
        // right tree, Pairing items, Resolving colors. Verify the
        // FullPath path here — exact set + ordering.
        QJsonModel left, right;
        left.loadJson(R"({"a":1,"b":2})");
        right.loadJson(R"({"a":1,"b":3})");
        auto L = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(&left));
        auto R = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(&right));

        QThread t;
        ThreadStopper stop{t};
        JsonDiffEngine eng;
        eng.moveToThread(&t);
        t.start();

        QList<JsonDiffEngine::Phase> phases;
        QObject::connect(&eng, &JsonDiffEngine::phaseChanged,
                         [&](JsonDiffEngine::Phase p) { phases.append(p); });

        QSignalSpy finishedSpy(&eng, &JsonDiffEngine::finished);
        QMetaObject::invokeMethod(&eng, "compareAsync", Qt::QueuedConnection,
            Q_ARG(QSharedPointer<DiffNode>, L),
            Q_ARG(QSharedPointer<DiffNode>, R),
            Q_ARG(JsonDiffEngine::Mode, JsonDiffEngine::Mode::FullPath));
        QVERIFY(finishedSpy.wait(2000));

        QCOMPARE(phases, QList<JsonDiffEngine::Phase>({
            JsonDiffEngine::Phase::BuildingPaths,
            JsonDiffEngine::Phase::MatchingPaths,
            JsonDiffEngine::Phase::ComparingValues,
            JsonDiffEngine::Phase::ResolvingColors
        }));
    }

    void asyncCompareParentChildPhases()
    {
        // ParentChildPair: Indexing right tree, Pairing items, Resolving colors.
        QJsonModel left, right;
        left.loadJson(R"({"a":1})");
        right.loadJson(R"({"a":1})");
        auto L = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(&left));
        auto R = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(&right));

        QThread t;
        ThreadStopper stop{t};
        JsonDiffEngine eng;
        eng.moveToThread(&t);
        t.start();

        QList<JsonDiffEngine::Phase> phases;
        QObject::connect(&eng, &JsonDiffEngine::phaseChanged,
                         [&](JsonDiffEngine::Phase p) { phases.append(p); });

        QSignalSpy finishedSpy(&eng, &JsonDiffEngine::finished);
        QMetaObject::invokeMethod(&eng, "compareAsync", Qt::QueuedConnection,
            Q_ARG(QSharedPointer<DiffNode>, L),
            Q_ARG(QSharedPointer<DiffNode>, R),
            Q_ARG(JsonDiffEngine::Mode, JsonDiffEngine::Mode::ParentChildPair));
        QVERIFY(finishedSpy.wait(2000));

        QCOMPARE(phases, QList<JsonDiffEngine::Phase>({
            JsonDiffEngine::Phase::IndexingRightTree,
            JsonDiffEngine::Phase::PairingItems,
            JsonDiffEngine::Phase::ResolvingColors
        }));
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
        // not copied. Verify the color reached our local pointer too.
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

    // ------------------------------------------------------------------
    // Phase A: copyPeer + recomparePair
    // Targeted edit-and-recompare — what the diff-edit overlay button
    // will drive. Tests the snapshot-side logic in isolation; the
    // widget-side glue lands in test_compare later.
    // ------------------------------------------------------------------

    void toJsonValueRoundTripsContainersAndScalars()
    {
        DiffNode root = makeRoot();
        root.children.append(makeScalar("s", QJsonValue::String, "hi"));
        root.children.append(makeScalar("n", QJsonValue::Double, "3.5"));
        root.children.append(makeScalar("b", QJsonValue::Bool,   "true"));
        DiffNode arr = makeContainer("arr", QJsonValue::Array);
        arr.children.append(makeScalar("0", QJsonValue::Double, "1"));
        arr.children.append(makeScalar("1", QJsonValue::Double, "2"));
        root.children.append(arr);

        QJsonValue v = JsonDiffEngine::toJsonValue(root);
        QVERIFY(v.isObject());
        QJsonObject obj = v.toObject();
        QCOMPARE(obj.value("s").toString(), QString("hi"));
        QCOMPARE(obj.value("n").toDouble(), 3.5);
        QCOMPARE(obj.value("b").toBool(),   true);
        QCOMPARE(obj.value("arr").toArray().size(), 2);
        QCOMPARE(obj.value("arr").toArray()[0].toDouble(), 1.0);
    }

    void copyPeerOverwritesScalarKeepsKey()
    {
        DiffNode dst = makeScalar("k", QJsonValue::String, "old");
        dst.color = DiffColorType::Huge;
        DiffNode src = makeScalar("ignored", QJsonValue::Double, "42");

        JsonDiffEngine::copyPeer(dst, src);

        // Position-identifying key is preserved.
        QCOMPARE(dst.key,   QString("k"));
        // Everything else came from the peer.
        QCOMPARE(dst.type,  QJsonValue::Double);
        QCOMPARE(dst.value, QString("42"));
        // color is deliberately untouched — recomparePair sets it.
        QCOMPARE(dst.color, DiffColorType::Huge);
    }

    void recomparePairFlipsHugeLeafToIdenticalAfterCopy()
    {
        // {"a": "old"} vs {"a": "new"} — "a" is Huge on both sides.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::String, "old"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("a", QJsonValue::String, "new"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color, DiffColorType::Huge);

        // Simulate the "copy from peer" click on the left side.
        JsonDiffEngine::copyPeer(L.children[0], R.children[0]);
        QList<int> path{0};
        JsonDiffEngine::recomparePair(L, path, R, path, JsonDiffEngine::Mode::FullPath);

        QCOMPARE(L.children[0].color, DiffColorType::Identical);
        QCOMPARE(R.children[0].color, DiffColorType::Identical);
    }

    void recomparePairDemotesAncestorModerateToIdentical()
    {
        // {"a": {"b": "old"}} vs {"a": {"b": "new"}}
        //  b -> Huge, a -> Moderate before edit.
        // After copyPeer + recomparePair on b: both b -> Identical, and
        // ancestor "a" should demote back to Identical (no other diffs).
        auto build = [](const QString &leaf) {
            DiffNode root = makeRoot();
            DiffNode a = makeContainer("a", QJsonValue::Object);
            a.children.append(makeScalar("b", QJsonValue::String, leaf));
            root.children.append(a);
            return root;
        };
        DiffNode L = build("old");
        DiffNode R = build("new");
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color,             DiffColorType::Moderate);
        QCOMPARE(L.children[0].children[0].color, DiffColorType::Huge);

        JsonDiffEngine::copyPeer(L.children[0].children[0],
                                 R.children[0].children[0]);
        QList<int> path{0, 0};
        JsonDiffEngine::recomparePair(L, path, R, path, JsonDiffEngine::Mode::FullPath);

        QCOMPARE(L.children[0].children[0].color, DiffColorType::Identical);
        QCOMPARE(L.children[0].color,             DiffColorType::Identical);
        QCOMPARE(R.children[0].children[0].color, DiffColorType::Identical);
        QCOMPARE(R.children[0].color,             DiffColorType::Identical);
    }

    void recomparePairKeepsAncestorModerateWhenSiblingStillDiffers()
    {
        // {"a": {"b": "old", "c": "x"}} vs {"a": {"b": "new", "c": "y"}}
        // Both b and c are Huge, a is Moderate.
        // Edit only b. a stays Moderate because c is still Huge.
        auto build = [](const QString &b, const QString &c) {
            DiffNode root = makeRoot();
            DiffNode a = makeContainer("a", QJsonValue::Object);
            a.children.append(makeScalar("b", QJsonValue::String, b));
            a.children.append(makeScalar("c", QJsonValue::String, c));
            root.children.append(a);
            return root;
        };
        DiffNode L = build("old", "x");
        DiffNode R = build("new", "y");
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color, DiffColorType::Moderate);

        // Resolve only b.
        JsonDiffEngine::copyPeer(L.children[0].children[0],
                                 R.children[0].children[0]);
        QList<int> path{0, 0};
        JsonDiffEngine::recomparePair(L, path, R, path, JsonDiffEngine::Mode::FullPath);

        QCOMPARE(L.children[0].children[0].color, DiffColorType::Identical);
        // c is still Huge, so a is still Moderate.
        QCOMPARE(L.children[0].children[1].color, DiffColorType::Huge);
        QCOMPARE(L.children[0].color,             DiffColorType::Moderate);
    }

    void recomparePairPreservesRelationPath()
    {
        // Cross-link must still resolve after the edit (the matched
        // peer is at the same path — copying value doesn't change
        // tree shape).
        DiffNode L = makeRoot();
        L.children.append(makeScalar("k", QJsonValue::String, "old"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("k", QJsonValue::String, "new"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);

        const QList<int> savedRel = L.children[0].relationPath;
        QVERIFY(!savedRel.isEmpty());

        JsonDiffEngine::copyPeer(L.children[0], R.children[0]);
        QList<int> path{0};
        JsonDiffEngine::recomparePair(L, path, R, path, JsonDiffEngine::Mode::FullPath);

        QCOMPARE(L.children[0].relationPath, savedRel);
    }

    void recomparePairHandlesEmptyPath()
    {
        // Path = {} means the root itself. Should be a no-op, not crash.
        DiffNode L = makeRoot();
        DiffNode R = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::Double, "1"));
        R.children.append(makeScalar("a", QJsonValue::Double, "1"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);

        JsonDiffEngine::recomparePair(L, {}, R, {}, JsonDiffEngine::Mode::FullPath);

        // Children below the root weren't touched.
        QCOMPARE(L.children[0].color, DiffColorType::Identical);
    }

    // ------------------------------------------------------------------
    // Phase B — resolveDstParentPath, insertPeer, removePeer
    // ------------------------------------------------------------------

    void resolveDstParentRejectsRootPath()
    {
        DiffNode L = makeRoot();
        QList<int> out;
        QVERIFY(!JsonDiffEngine::resolveDstParentPath(L, {}, out));
    }

    void resolveDstParentRootChildMirrorsRoot()
    {
        // Top-level NotPresent: src parent is root → dst parent is the
        // OTHER root, which we encode as an empty path.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("only_left", QJsonValue::String, "x"));
        QList<int> out;
        QVERIFY(JsonDiffEngine::resolveDstParentPath(L, {0}, out));
        QVERIFY(out.isEmpty());
    }

    void resolveDstParentNestedUsesParentRelationPath()
    {
        // {"a":{"b":1,"only_left":2}} vs {"a":{"b":1}}
        // src.a is Identical/Moderate, src.only_left is NotPresent.
        // resolveDstParentPath returns the dst path to "a".
        DiffNode L = makeRoot();
        DiffNode a_l = makeContainer("a", QJsonValue::Object);
        a_l.children.append(makeScalar("b", QJsonValue::Double, "1"));
        a_l.children.append(makeScalar("only_left", QJsonValue::Double, "2"));
        L.children.append(a_l);

        DiffNode R = makeRoot();
        DiffNode a_r = makeContainer("a", QJsonValue::Object);
        a_r.children.append(makeScalar("b", QJsonValue::Double, "1"));
        R.children.append(a_r);

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].children[1].color, DiffColorType::NotPresent);

        QList<int> out;
        QVERIFY(JsonDiffEngine::resolveDstParentPath(L, {0, 1}, out));
        QCOMPARE(out, (QList<int>{0}));   // R.children[0] is "a"
    }

    void resolveDstParentRejectsWhenParentAlsoNotPresent()
    {
        // {"a":{"b":1}} vs {} — "a" doesn't exist on right; both "a"
        // and "a/b" are NotPresent on left. Pushing "a/b" must fail —
        // its parent has no peer to insert into.
        DiffNode L = makeRoot();
        DiffNode a_l = makeContainer("a", QJsonValue::Object);
        a_l.children.append(makeScalar("b", QJsonValue::Double, "1"));
        L.children.append(a_l);
        DiffNode R = makeRoot();

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QList<int> out;
        QVERIFY(!JsonDiffEngine::resolveDstParentPath(L, {0, 0}, out));
    }

    void insertPeerAppendsAndLinksWholeSubtree()
    {
        // {"a":{"x":1}, "extra":{"deep":42}} vs {"a":{"x":1}}.
        // "extra" is NotPresent on left. After insertPeer, it appears
        // on right; both "extra" and "extra.deep" become Identical and
        // cross-linked.
        DiffNode L = makeRoot();
        DiffNode a_l = makeContainer("a", QJsonValue::Object);
        a_l.children.append(makeScalar("x", QJsonValue::Double, "1"));
        L.children.append(a_l);
        DiffNode extra = makeContainer("extra", QJsonValue::Object);
        extra.children.append(makeScalar("deep", QJsonValue::Double, "42"));
        L.children.append(extra);

        DiffNode R = makeRoot();
        DiffNode a_r = makeContainer("a", QJsonValue::Object);
        a_r.children.append(makeScalar("x", QJsonValue::Double, "1"));
        R.children.append(a_r);

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[1].color, DiffColorType::NotPresent);

        QList<int> dstPath = JsonDiffEngine::insertPeer(L, {1}, R, {},
                                                       JsonDiffEngine::Mode::FullPath);
        QCOMPARE(dstPath, (QList<int>{1}));
        // Top of inserted subtree.
        QCOMPARE(R.children[1].key,                  QString("extra"));
        QCOMPARE(R.children[1].color,                DiffColorType::Identical);
        QCOMPARE(R.children[1].relationPath,         (QList<int>{1}));
        // Interior — must also be cross-linked.
        QCOMPARE(R.children[1].children[0].key,       QString("deep"));
        QCOMPARE(R.children[1].children[0].color,     DiffColorType::Identical);
        QCOMPARE(R.children[1].children[0].relationPath, (QList<int>{1, 0}));
        // Source side flipped from NotPresent → Identical.
        QCOMPARE(L.children[1].color,                DiffColorType::Identical);
        QCOMPARE(L.children[1].relationPath,         (QList<int>{1}));
    }

    void insertPeerAppendToArrayUsesIndexKey()
    {
        // {"arr":[10,20,30]} vs {"arr":[10,20]}: index 2 (value 30)
        // is NotPresent on left. After push, it appears at index 2 on
        // right with key "2".
        DiffNode L = makeRoot();
        DiffNode arr_l = makeContainer("arr", QJsonValue::Array);
        arr_l.children.append(makeScalar("0", QJsonValue::Double, "10"));
        arr_l.children.append(makeScalar("1", QJsonValue::Double, "20"));
        arr_l.children.append(makeScalar("2", QJsonValue::Double, "30"));
        L.children.append(arr_l);

        DiffNode R = makeRoot();
        DiffNode arr_r = makeContainer("arr", QJsonValue::Array);
        arr_r.children.append(makeScalar("0", QJsonValue::Double, "10"));
        arr_r.children.append(makeScalar("1", QJsonValue::Double, "20"));
        R.children.append(arr_r);

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].children[2].color, DiffColorType::NotPresent);

        QList<int> dstPath = JsonDiffEngine::insertPeer(L, {0, 2}, R, {0},
                                                       JsonDiffEngine::Mode::FullPath);
        QCOMPARE(dstPath, (QList<int>{0, 2}));
        QCOMPARE(R.children[0].children.size(), 3);
        QCOMPARE(R.children[0].children[2].key,   QString("2"));   // index-keyed
        QCOMPARE(R.children[0].children[2].value, QString("30"));
        QCOMPARE(R.children[0].children[2].color, DiffColorType::Identical);
    }

    void removePeerOrphansPairedSibling()
    {
        // {"a":1,"b":2,"c":3} vs {"a":1,"b":2,"c":3}: all Identical.
        // Delete "b" on the left. Right's "b" must become NotPresent
        // and right's "c" must shift its relationPath from {2} to {1}.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::Double, "1"));
        L.children.append(makeScalar("b", QJsonValue::Double, "2"));
        L.children.append(makeScalar("c", QJsonValue::Double, "3"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("a", QJsonValue::Double, "1"));
        R.children.append(makeScalar("b", QJsonValue::Double, "2"));
        R.children.append(makeScalar("c", QJsonValue::Double, "3"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(R.children[2].relationPath, (QList<int>{2}));

        QVERIFY(JsonDiffEngine::removePeer(L, {1}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(L.children.size(), 2);
        QCOMPARE(L.children[0].key, QString("a"));
        QCOMPARE(L.children[1].key, QString("c"));
        // Right's "b" was paired → now orphaned to NotPresent.
        QCOMPARE(R.children[1].key,           QString("b"));
        QCOMPARE(R.children[1].color,         DiffColorType::NotPresent);
        QVERIFY (R.children[1].relationPath.isEmpty());
        // Right's "c" used to point at L.children[2]; now it must
        // point at L.children[1] (the slot vacated by the shift).
        QCOMPARE(R.children[2].relationPath, (QList<int>{1}));
    }

    void removePeerNotPresentLeafHasNoOrphan()
    {
        // {"a":1,"only_left":2} vs {"a":1}. only_left is NotPresent on
        // left and has no peer. Deleting it just removes it; right side
        // is left untouched.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a",         QJsonValue::Double, "1"));
        L.children.append(makeScalar("only_left", QJsonValue::Double, "2"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("a", QJsonValue::Double, "1"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[1].color, DiffColorType::NotPresent);

        QVERIFY(JsonDiffEngine::removePeer(L, {1}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(L.children.size(), 1);
        QCOMPARE(R.children.size(), 1);
        QCOMPARE(R.children[0].color,        DiffColorType::Identical);
        QCOMPARE(R.children[0].relationPath, (QList<int>{0}));
    }

    void removePeerRejectsRoot()
    {
        DiffNode L = makeRoot();
        DiffNode R = makeRoot();
        QVERIFY(!JsonDiffEngine::removePeer(L, {}, R,
                                            JsonDiffEngine::Mode::FullPath));
    }

    void removePeerArrayRenumbersSurvivorKeys()
    {
        // Both sides have a 3-element array. Delete the middle element
        // on the left — survivors' keys must shift "0","2" → "0","1"
        // so a subsequent FullPath compare matches by position.
        DiffNode L = makeRoot();
        DiffNode arr_l = makeContainer("arr", QJsonValue::Array);
        arr_l.children.append(makeScalar("0", QJsonValue::Double, "10"));
        arr_l.children.append(makeScalar("1", QJsonValue::Double, "20"));
        arr_l.children.append(makeScalar("2", QJsonValue::Double, "30"));
        L.children.append(arr_l);

        DiffNode R = makeRoot();
        DiffNode arr_r = makeContainer("arr", QJsonValue::Array);
        arr_r.children.append(makeScalar("0", QJsonValue::Double, "10"));
        arr_r.children.append(makeScalar("1", QJsonValue::Double, "20"));
        arr_r.children.append(makeScalar("2", QJsonValue::Double, "30"));
        R.children.append(arr_r);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);

        QVERIFY(JsonDiffEngine::removePeer(L, {0, 1}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(L.children[0].children.size(), 2);
        QCOMPARE(L.children[0].children[0].key, QString("0"));
        QCOMPARE(L.children[0].children[1].key, QString("1"));   // renumbered
        // Values follow the renumber correctly.
        QCOMPARE(L.children[0].children[1].value, QString("30"));
    }

    void removePeerObjectKeysUnchanged()
    {
        // Object children carry meaningful keys — deletion must NOT
        // touch survivor keys.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("a", QJsonValue::Double, "1"));
        L.children.append(makeScalar("b", QJsonValue::Double, "2"));
        L.children.append(makeScalar("c", QJsonValue::Double, "3"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("a", QJsonValue::Double, "1"));
        R.children.append(makeScalar("b", QJsonValue::Double, "2"));
        R.children.append(makeScalar("c", QJsonValue::Double, "3"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);

        QVERIFY(JsonDiffEngine::removePeer(L, {1}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(L.children[0].key, QString("a"));
        QCOMPARE(L.children[1].key, QString("c"));   // NOT "b"
    }

    void detachPairBothSidesGoNotPresent()
    {
        // Identical pair → detach → both sides NotPresent, no peer links.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("k", QJsonValue::String, "v"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("k", QJsonValue::String, "v"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color, DiffColorType::Identical);

        QVERIFY(JsonDiffEngine::detachPair(L, {0}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(L.children[0].color,        DiffColorType::NotPresent);
        QCOMPARE(R.children[0].color,        DiffColorType::NotPresent);
        QVERIFY (L.children[0].relationPath.isEmpty());
        QVERIFY (R.children[0].relationPath.isEmpty());
    }

    void detachPairRejectsRootPath()
    {
        DiffNode L = makeRoot();
        DiffNode R = makeRoot();
        QVERIFY(!JsonDiffEngine::detachPair(L, {}, R,
                                            JsonDiffEngine::Mode::FullPath));
    }

    void resnapshotSubtreeReplacesChildrenFromModel()
    {
        // Build a real model: {"k":{"x":1,"y":2}}, take a snapshot,
        // then mutate the model in place (remove a child) and confirm
        // resnapshotSubtree pulls in the new shape.
        QJsonModel m;
        m.loadJson(R"({"k":{"x":1,"y":2}})");
        DiffNode mySnap = JsonDiffEngine::snapshot(&m);
        QCOMPARE(mySnap.children[0].children.size(), 2);

        // Edit the model: drop "y" from k's children.
        m.setEditable(true);
        QModelIndex kIdx = m.index(0, 0, QModelIndex());
        QModelIndex yIdx;
        for (int i = 0; i < m.rowCount(kIdx); ++i)
        {
            QModelIndex c = m.index(i, 0, kIdx);
            if (m.itemFromIndex(c)->key() == "y") { yIdx = c; break; }
        }
        QVERIFY(m.removeRowAt(yIdx));

        DiffNode peerSnap;            // empty peer just for the API
        QVERIFY(JsonDiffEngine::resnapshotSubtree(mySnap, {0}, &m, peerSnap));
        QCOMPARE(mySnap.children[0].children.size(), 1);
        QCOMPARE(mySnap.children[0].children[0].key,  QString("x"));
        // Re-snapshotted children default to NotPresent (no peer).
        QCOMPARE(mySnap.children[0].children[0].color,
                 DiffColorType::NotPresent);
    }

    void resnapshotSubtreeOrphansPeerDescendants()
    {
        // {"k":{"x":1}} == {"k":{"x":1}}: identical with cross-links
        // on root.children[0].children[0]. After resnapshotSubtree on
        // left's "k", peer's "x" should be orphaned (its relationPath
        // pointed INTO the subtree we just replaced).
        QJsonModel ml, mr;
        ml.loadJson(R"({"k":{"x":1}})");
        mr.loadJson(R"({"k":{"x":1}})");
        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(R.children[0].children[0].color,        DiffColorType::Identical);
        QCOMPARE(R.children[0].children[0].relationPath, (QList<int>{0, 0}));

        QVERIFY(JsonDiffEngine::resnapshotSubtree(L, {0}, &ml, R));

        // Peer of L[0] (R[0]) is left intact — recomparePair handles
        // its color after this call. Its DESCENDANT was orphaned.
        QCOMPARE(R.children[0].children[0].color,        DiffColorType::NotPresent);
        QVERIFY (R.children[0].children[0].relationPath.isEmpty());
        // The pair at the top of the subtree is untouched here.
        QCOMPARE(R.children[0].relationPath,             (QList<int>{0}));
    }

    void resnapshotSubtreeRejectsEmptyPath()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1})");
        DiffNode mySnap = JsonDiffEngine::snapshot(&m);
        DiffNode peerSnap;
        QVERIFY(!JsonDiffEngine::resnapshotSubtree(mySnap, {}, &m, peerSnap));
    }

    void detachPairUnpairedLeafNoOpsOnPeer()
    {
        // NotPresent leaf has no peer to touch on the other side; the
        // function still updates this side (re-marks NotPresent, same).
        DiffNode L = makeRoot();
        L.children.append(makeScalar("only_left", QJsonValue::Double, "1"));
        DiffNode R = makeRoot();
        R.children.append(makeScalar("only_right", QJsonValue::Double, "2"));
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        QCOMPARE(L.children[0].color, DiffColorType::NotPresent);

        QVERIFY(JsonDiffEngine::detachPair(L, {0}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(L.children[0].color, DiffColorType::NotPresent);
        QCOMPARE(R.children[0].color, DiffColorType::NotPresent);
    }

    void removePeerSubtreeOrphansAllDescendants()
    {
        // {"keep":1,"big":{"x":1,"y":2}} vs {"keep":1,"big":{"x":1,"y":2}}.
        // Delete "big" on left. Right's "big", "big.x", "big.y" all go
        // NotPresent.
        DiffNode L = makeRoot();
        L.children.append(makeScalar("keep", QJsonValue::Double, "1"));
        DiffNode big_l = makeContainer("big", QJsonValue::Object);
        big_l.children.append(makeScalar("x", QJsonValue::Double, "1"));
        big_l.children.append(makeScalar("y", QJsonValue::Double, "2"));
        L.children.append(big_l);

        DiffNode R = makeRoot();
        R.children.append(makeScalar("keep", QJsonValue::Double, "1"));
        DiffNode big_r = makeContainer("big", QJsonValue::Object);
        big_r.children.append(makeScalar("x", QJsonValue::Double, "1"));
        big_r.children.append(makeScalar("y", QJsonValue::Double, "2"));
        R.children.append(big_r);

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);

        QVERIFY(JsonDiffEngine::removePeer(L, {1}, R,
                                           JsonDiffEngine::Mode::FullPath));
        QCOMPARE(R.children[1].color,                DiffColorType::NotPresent);
        QCOMPARE(R.children[1].children[0].color,    DiffColorType::NotPresent);
        QCOMPARE(R.children[1].children[1].color,    DiffColorType::NotPresent);
        QVERIFY (R.children[1].relationPath.isEmpty());
        QVERIFY (R.children[1].children[0].relationPath.isEmpty());
        QVERIFY (R.children[1].children[1].relationPath.isEmpty());
        // Root's color: NotPresent descendants don't promote ancestors
        // in this engine (matches original fixColors). Root stays at
        // Identical/None — the "extra subtree" is shown by the per-row
        // NotPresent coloring, not by an ancestor tint.
    }
};

QTEST_GUILESS_MAIN(TestEngine)
#include "test_engine.moc"
