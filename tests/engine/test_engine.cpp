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
#include <QSet>
#include <QFile>
#include <QIODevice>

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

    void applyPathWritesOnlyAlongPath()
    {
        // Setup: {a:1, b:{c:1, d:2}, e:3} vs {a:1, b:{c:1, d:9}, e:3}.
        // After compare + full apply, b.d is Huge, b is Moderate, root
        // is Moderate. Verify applyPath along path=[1,1] (b.d) touches
        // exactly the ancestors and the leaf while leaving unrelated
        // siblings' color/idxRelation intact — no layoutChanged fires.
        QJsonModel ml;
        ml.loadJson(R"({"a":1,"b":{"c":1,"d":2},"e":3})");
        QJsonModel mr;
        mr.loadJson(R"({"a":1,"b":{"c":1,"d":9},"e":3})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Baseline state after full apply.
        QModelIndex bL   = ml.index(1, 0);
        QModelIndex bcL  = ml.index(0, 0, bL);
        QModelIndex bdL  = ml.index(1, 0, bL);
        QModelIndex eL   = ml.index(2, 0);
        QCOMPARE(ml.itemFromIndex(bdL)->colorType(), DiffColorType::Huge);
        QCOMPARE(ml.itemFromIndex(bL) ->colorType(), DiffColorType::Moderate);
        QCOMPARE(ml.itemFromIndex(bcL)->colorType(), DiffColorType::Identical);
        QCOMPARE(ml.itemFromIndex(eL) ->colorType(), DiffColorType::Identical);

        // Corrupt siblings' state — applyPath must NOT overwrite them
        // (they're outside the path). If applyPath walked the whole
        // tree it would restore these from the snapshot.
        ml.itemFromIndex(bcL)->setColorType(DiffColorType::None);
        ml.itemFromIndex(eL) ->setColorType(DiffColorType::None);

        // No layoutChanged should fire on a targeted applyPath — the
        // whole point of the fast path is to avoid the persistent-
        // index remap. Dataupdated is emitted for the counter widget.
        QSignalSpy layoutSpy(&ml, &QAbstractItemModel::layoutChanged);
        QSignalSpy dataChangedSpy(&ml, &QAbstractItemModel::dataChanged);
        QSignalSpy dataUpdatedSpy(&ml, &QJsonModel::dataUpdated);

        JsonDiffEngine::applyPath(L, &ml, {1, 1}, &mr);

        QCOMPARE(layoutSpy.count(), 0);
        QVERIFY (dataChangedSpy.count() >= 2);   // b + b.d rows
        QCOMPARE(dataUpdatedSpy.count(), 1);
        // Path targets refreshed from snapshot.
        QCOMPARE(ml.itemFromIndex(bdL)->colorType(), DiffColorType::Huge);
        QCOMPARE(ml.itemFromIndex(bL) ->colorType(), DiffColorType::Moderate);
        // Off-path siblings were left corrupted — applyPath did NOT
        // touch them, proving it walked only the path.
        QCOMPARE(ml.itemFromIndex(bcL)->colorType(), DiffColorType::None);
        QCOMPARE(ml.itemFromIndex(eL) ->colorType(), DiffColorType::None);
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

    void removePeerSetsParentHugeWhenSizesNowDiffer()
    {
        // Original main-branch rule (compareValue): paired Object/Array
        // containers with mismatched child counts BOTH go Huge. After
        // an edit-driven delete, removePeer must replay that rule so
        // the user sees the same "this object now differs" signal they
        // would get from a fresh Compare.
        //
        // ParentChildPair pairs items by parent.key+key+type, which can
        // place peers at different indices on the two sides — that
        // structural skew is also what made the old "approximate"
        // ancestor walk land on the wrong subtree.
        //
        // Setup: left {"a":{"x":1,"y":9},"z":2},
        //        right {"z":2,"a":{"x":1,"y":8}}.
        // After Compare (ParentChildPair):
        //   L.a [0]   pairs R.a [1] — same size, Identical → Moderate
        //                            after fixColors promotes via y.
        //   L.a.x [0,0] pairs R.a.x [1,0] — Identical.
        //   L.a.y [0,1] pairs R.a.y [1,1] — Huge (9 vs 8).
        //   L.z [1]   pairs R.z [0] — Identical.
        DiffNode L = makeRoot();
        DiffNode a_l = makeContainer("a", QJsonValue::Object);
        a_l.children.append(makeScalar("x", QJsonValue::Double, "1"));
        a_l.children.append(makeScalar("y", QJsonValue::Double, "9"));
        L.children.append(a_l);
        L.children.append(makeScalar("z", QJsonValue::Double, "2"));

        DiffNode R = makeRoot();
        R.children.append(makeScalar("z", QJsonValue::Double, "2"));
        DiffNode a_r = makeContainer("a", QJsonValue::Object);
        a_r.children.append(makeScalar("x", QJsonValue::Double, "1"));
        a_r.children.append(makeScalar("y", QJsonValue::Double, "8"));
        R.children.append(a_r);

        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::ParentChildPair);

        // Pre-delete sanity: both `a` sides Moderate via Huge child y.
        QCOMPARE(L.children[0].color, DiffColorType::Moderate);   // L.a
        QCOMPARE(R.children[1].color, DiffColorType::Moderate);   // R.a
        QCOMPARE(R.children[1].children[1].color, DiffColorType::Huge); // R.a.y

        QVERIFY(JsonDiffEngine::removePeer(L, {0, 1}, R,
                                           JsonDiffEngine::Mode::ParentChildPair));

        // R.a.y is the orphan — no peer on the (now-smaller) src side.
        QCOMPARE(R.children[1].children[1].color, DiffColorType::NotPresent);
        QVERIFY (R.children[1].children[1].relationPath.isEmpty());
        // Sizes differ (L.a has 1 child, R.a still has 2) → both `a`
        // sides go Huge per the original rule. Roots get promoted to
        // Moderate (Huge child counts as a diff descendant).
        QCOMPARE(L.children[0].color, DiffColorType::Huge);       // L.a
        QCOMPARE(R.children[1].color, DiffColorType::Huge);       // R.a
        QCOMPARE(L.color,             DiffColorType::Moderate);   // L root
        QCOMPARE(R.color,             DiffColorType::Moderate);   // R root
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

    // ------------------------------------------------------------------
    // weightOf(): fingerprint used by upcoming array alignment
    // ------------------------------------------------------------------

    void weightOfScalarStable()
    {
        QJsonModel m;
        m.loadJson(R"({"k": "hello"})");
        DiffNode snap = JsonDiffEngine::snapshot(&m);
        DiffNode *k = childByKey(snap, "k");
        QVERIFY(k);
        const qint64 w = JsonDiffEngine::weightOf(*k);
        // Cache is populated by snapshot and matches on-demand compute.
        QCOMPARE(k->weight, w);
    }

    void weightOfEqualObjectsRegardlessOfKeyOrder()
    {
        // Same JSON with keys serialized in different order must weigh
        // the same. Load-order comes from QJsonDocument which sorts
        // object keys, so we can't easily force a different order via
        // loadJson — instead build DiffNodes by hand.
        DiffNode a;
        a.type = QJsonValue::Object;
        a.children.append(makeScalar("alpha", QJsonValue::String, "one"));
        a.children.append(makeScalar("beta",  QJsonValue::String, "two"));

        DiffNode b;
        b.type = QJsonValue::Object;
        // Swapped insertion order:
        b.children.append(makeScalar("beta",  QJsonValue::String, "two"));
        b.children.append(makeScalar("alpha", QJsonValue::String, "one"));

        QCOMPARE(JsonDiffEngine::weightOf(a), JsonDiffEngine::weightOf(b));
    }

    void weightOfDifferentValuesDifferent()
    {
        DiffNode a = makeScalar("k", QJsonValue::String, "hello");
        DiffNode b = makeScalar("k", QJsonValue::String, "world");
        QVERIFY(JsonDiffEngine::weightOf(a) != JsonDiffEngine::weightOf(b));
    }

    void weightIgnoresKey()
    {
        // Two scalars with the SAME value but different keys must
        // weigh the same — alignment needs to match "same value at
        // different position", so the key must not contribute.
        DiffNode a = makeScalar("first",  QJsonValue::String, "same");
        DiffNode b = makeScalar("second", QJsonValue::String, "same");
        QCOMPARE(JsonDiffEngine::weightOf(a), JsonDiffEngine::weightOf(b));
    }

    void snapshotPopulatesWeightBottomUp()
    {
        QJsonModel m;
        m.loadJson(R"({"outer": {"inner": "leaf"}})");
        DiffNode snap = JsonDiffEngine::snapshot(&m);
        DiffNode *outer = childByKey(snap, "outer");
        QVERIFY(outer);
        DiffNode *inner = childByKey(*outer, "inner");
        QVERIFY(inner);
        // Every node in the tree carries a computed weight.
        QVERIFY(inner->weight > 0);
        QVERIFY(outer->weight > 0);
        QVERIFY(snap.weight > 0);
        // And the cache matches recomputation on demand.
        QCOMPARE(inner->weight, JsonDiffEngine::weightOf(*inner));
        QCOMPARE(outer->weight, JsonDiffEngine::weightOf(*outer));
    }

    void weightOfEmptyContainers()
    {
        DiffNode obj;
        obj.type = QJsonValue::Object;
        DiffNode arr;
        arr.type = QJsonValue::Array;
        // "{}" and "[]" both stringify to length-2 strings but with
        // different characters, so their hashes must differ.
        const qint64 wo = JsonDiffEngine::weightOf(obj);
        const qint64 wa = JsonDiffEngine::weightOf(arr);
        QVERIFY(wo != wa);
    }

    void knownValueRegression()
    {
        // weightOfString is qHash-based; the exact hash depends on Qt
        // build + per-process seed. Only verify that identical inputs
        // hash to identical values and that different inputs differ.
        DiffNode a = makeScalar("k", QJsonValue::String, "abc");
        DiffNode b = makeScalar("k", QJsonValue::String, "abc");
        DiffNode c = makeScalar("k", QJsonValue::String, "abd");
        QCOMPARE(JsonDiffEngine::weightOf(a), JsonDiffEngine::weightOf(b));
        QVERIFY(JsonDiffEngine::weightOf(a) != JsonDiffEngine::weightOf(c));
    }

    // ------------------------------------------------------------------
    // alignByWeight(): LCS-based sequence alignment for array children
    // ------------------------------------------------------------------

    void alignIdentical()
    {
        const auto out = JsonDiffEngine::alignByWeight({1, 2, 3}, {1, 2, 3});
        QCOMPARE(out.size(), 3);
        QCOMPARE(out[0], qMakePair(0, 0));
        QCOMPARE(out[1], qMakePair(1, 1));
        QCOMPARE(out[2], qMakePair(2, 2));
    }

    void alignBothEmpty()
    {
        const auto out = JsonDiffEngine::alignByWeight({}, {});
        QCOMPARE(out.size(), 0);
    }

    void alignLeftOnly()
    {
        const auto out = JsonDiffEngine::alignByWeight({1, 2}, {});
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[0], qMakePair(0, -1));
        QCOMPARE(out[1], qMakePair(1, -1));
    }

    void alignRightOnly()
    {
        const auto out = JsonDiffEngine::alignByWeight({}, {1, 2});
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[0], qMakePair(-1, 0));
        QCOMPARE(out[1], qMakePair(-1, 1));
    }

    void alignRightHasTrailingInsert()
    {
        const auto out = JsonDiffEngine::alignByWeight({1, 2, 3}, {1, 2, 3, 4});
        QCOMPARE(out.size(), 4);
        QCOMPARE(out[0], qMakePair(0, 0));
        QCOMPARE(out[1], qMakePair(1, 1));
        QCOMPARE(out[2], qMakePair(2, 2));
        QCOMPARE(out[3], qMakePair(-1, 3));
    }

    void alignLeftHasTrailingExtra()
    {
        const auto out = JsonDiffEngine::alignByWeight({1, 2, 3}, {1, 2});
        QCOMPARE(out.size(), 3);
        QCOMPARE(out[0], qMakePair(0, 0));
        QCOMPARE(out[1], qMakePair(1, 1));
        QCOMPARE(out[2], qMakePair(2, -1));
    }

    void alignMiddleInsertOnRight()
    {
        // Left [1, 3], Right [1, 2, 3]. Bipartite output layout: matched
        // pairs sorted by leftIdx first, then left orphans, then right
        // orphans (phantoms).
        const auto out = JsonDiffEngine::alignByWeight({1, 3}, {1, 2, 3});
        QCOMPARE(out.size(), 3);
        QCOMPARE(out[0], qMakePair(0, 0));
        QCOMPARE(out[1], qMakePair(1, 2));
        QCOMPARE(out[2], qMakePair(-1, 1));
    }

    void alignCompleteMismatch()
    {
        // No common items. All lefts come first (in original order),
        // then all rights.
        const auto out = JsonDiffEngine::alignByWeight({1, 2}, {3, 4});
        QCOMPARE(out.size(), 4);
        QCOMPARE(out[0], qMakePair(0, -1));
        QCOMPARE(out[1], qMakePair(1, -1));
        QCOMPARE(out[2], qMakePair(-1, 0));
        QCOMPARE(out[3], qMakePair(-1, 1));
    }

    void alignDuplicatesValidPairing()
    {
        // Bipartite by weight: for each shared weight, pair k-th on
        // each side. Assertions preserved: every index appears once;
        // matched pairs share weight. Right index order is NOT
        // guaranteed to ascend (bipartite is unordered).
        const QList<qint64> L = {1, 1, 2};
        const QList<qint64> R = {1, 2, 1};
        const auto out = JsonDiffEngine::alignByWeight(L, R);

        QSet<int> leftSeen, rightSeen;
        for (const auto &p : out)
        {
            if (p.first != -1)
            {
                QVERIFY(!leftSeen.contains(p.first));
                leftSeen.insert(p.first);
            }
            if (p.second != -1)
            {
                QVERIFY(!rightSeen.contains(p.second));
                rightSeen.insert(p.second);
            }
            if (p.first != -1 && p.second != -1)
                QCOMPARE(L[p.first], R[p.second]);
        }
        QCOMPARE(leftSeen.size(),  L.size());
        QCOMPARE(rightSeen.size(), R.size());
    }

    // ------------------------------------------------------------------
    // alignByWeightWithKey(): primary-key preference for object children
    // ------------------------------------------------------------------

    // Build an Object DiffNode with an "id" field carrying the given
    // string value + a "junk" scalar so two same-id objects still
    // differ enough that weight-only matching would miss the pair.
    static DiffNode makeIdObj(const QString &idValue, const QString &junk)
    {
        DiffNode obj;
        obj.type = QJsonValue::Object;

        DiffNode idField;
        idField.key   = "id";
        idField.type  = QJsonValue::String;
        idField.value = idValue;
        idField.weight = JsonDiffEngine::weightOf(idField);

        DiffNode junkField;
        junkField.key   = "junk";
        junkField.type  = QJsonValue::String;
        junkField.value = junk;
        junkField.weight = JsonDiffEngine::weightOf(junkField);

        obj.children.append(idField);
        obj.children.append(junkField);
        obj.weight = JsonDiffEngine::weightOf(obj);
        return obj;
    }

    void alignByWeightWithKeyPairsSameId()
    {
        // Same-order objects with matching "id" but different junk.
        // Full-node weight differs (junk changed), but key-preferred
        // alignment must pair them because their id fingerprints match.
        QList<DiffNode> L, R;
        L.append(makeIdObj("A", "one"));
        L.append(makeIdObj("B", "two"));
        R.append(makeIdObj("A", "one-changed"));
        R.append(makeIdObj("B", "two-changed"));

        const auto out = JsonDiffEngine::alignByWeightWithKey(L, R, "id");

        // Order-preserving LCS pairs both when ids align in order.
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[0], qMakePair(0, 0));
        QCOMPARE(out[1], qMakePair(1, 1));

        // Sanity: weight-only alignment would NOT pair them (junk
        // makes full-node weights differ), so the key preference is
        // load-bearing here.
        QList<qint64> lw{L[0].weight, L[1].weight};
        QList<qint64> rw{R[0].weight, R[1].weight};
        const auto weightOnly = JsonDiffEngine::alignByWeight(lw, rw);
        int matchedWeightOnly = 0;
        for (const auto &p : weightOnly)
            if (p.first != -1 && p.second != -1) ++matchedWeightOnly;
        QCOMPARE(matchedWeightOnly, 0);
    }

    void alignByWeightWithKeyCommaSeparatedList()
    {
        // matchKey="id,eventId" — L uses "id", R uses "eventId";
        // both should still pair when values match.
        QList<DiffNode> L, R;
        {
            DiffNode o; o.type = QJsonValue::Object;
            DiffNode id; id.key="id"; id.type=QJsonValue::String; id.value="AAA";
            id.weight = JsonDiffEngine::weightOf(id);
            o.children.append(id);
            o.weight = JsonDiffEngine::weightOf(o);
            L.append(o);
        }
        {
            DiffNode o; o.type = QJsonValue::Object;
            DiffNode eid; eid.key="eventId"; eid.type=QJsonValue::String; eid.value="AAA";
            eid.weight = JsonDiffEngine::weightOf(eid);
            o.children.append(eid);
            o.weight = JsonDiffEngine::weightOf(o);
            R.append(o);
        }
        // The values under the (differently-named) key fields both
        // canonicalize to the same string, so the id-weight matches.
        // With a single "id" matchKey, R would fall into the no-key
        // bucket and not pair; with "id,eventId" both find a key.
        const auto out = JsonDiffEngine::alignByWeightWithKey(L, R, "id,eventId");
        int matched = 0;
        for (const auto &p : out)
            if (p.first != -1 && p.second != -1) ++matched;
        QCOMPARE(matched, 1);
    }

    void alignByWeightWithKeyBucketsHasKeySeparateFromNoKey()
    {
        // L has one item WITH id, one WITHOUT.
        // R has one item WITH id (matching L's id), one WITHOUT
        //   (matching L's no-key by full-node weight).
        // Both bucket-pair independently — 2 matches, no cross-pair.
        QList<DiffNode> L, R;
        auto makeIdObj = [](const QString &id) {
            DiffNode o; o.type = QJsonValue::Object;
            DiffNode k; k.key="id"; k.type=QJsonValue::String; k.value=id;
            k.weight = JsonDiffEngine::weightOf(k);
            o.children.append(k);
            o.weight = JsonDiffEngine::weightOf(o);
            return o;
        };
        auto makePlainObj = [](const QString &val) {
            DiffNode o; o.type = QJsonValue::Object;
            DiffNode k; k.key="name"; k.type=QJsonValue::String; k.value=val;
            k.weight = JsonDiffEngine::weightOf(k);
            o.children.append(k);
            o.weight = JsonDiffEngine::weightOf(o);
            return o;
        };
        L.append(makeIdObj("100"));
        L.append(makePlainObj("hello"));
        R.append(makePlainObj("hello"));
        R.append(makeIdObj("100"));

        const auto out = JsonDiffEngine::alignByWeightWithKey(L, R, "id");
        int matched = 0;
        for (const auto &p : out)
            if (p.first != -1 && p.second != -1) ++matched;
        QCOMPARE(matched, 2);
    }

    void alignByWeightWithKeyReorderedMatchesAll()
    {
        // Reordered objects [A,B] vs [B,A] with matching ids.
        // Bipartite by identity pairs both regardless of order.
        QList<DiffNode> L, R;
        L.append(makeIdObj("A", "one"));
        L.append(makeIdObj("B", "two"));
        R.append(makeIdObj("B", "two"));
        R.append(makeIdObj("A", "one"));

        const auto out = JsonDiffEngine::alignByWeightWithKey(L, R, "id");
        int matched = 0;
        for (const auto &p : out)
            if (p.first != -1 && p.second != -1) ++matched;
        QCOMPARE(matched, 2);
    }

    void alignByWeightWithKeyEmptyKeyFallsBackToWeight()
    {
        // With an empty key, behavior collapses to plain weight LCS.
        QList<DiffNode> L, R;
        L.append(makeIdObj("A", "same"));
        R.append(makeIdObj("A", "same"));
        R.append(makeIdObj("X", "different"));

        const auto out = JsonDiffEngine::alignByWeightWithKey(L, R, "");
        QCOMPARE(out.size(), 2);
        // L[0] == R[0] fully (same content), so weight-LCS pairs them
        // and R[1] is an orphan.
        QCOMPARE(out[0], qMakePair(0, 0));
        QCOMPARE(out[1], qMakePair(-1, 1));
    }

    void alignByWeightWithKeyMissingKeyFallsBackPerElement()
    {
        // One side has objects without the key; those fall back to
        // full-node weight, which still lets identical objects match.
        QList<DiffNode> L, R;
        L.append(makeIdObj("A", "one"));
        DiffNode scalar;
        scalar.type = QJsonValue::String;
        scalar.value = "notanobject";
        scalar.weight = JsonDiffEngine::weightOf(scalar);
        L.append(scalar);
        R.append(scalar);
        R.append(makeIdObj("A", "one"));

        const auto out = JsonDiffEngine::alignByWeightWithKey(L, R, "id");
        // Whatever pairing LCS picks, invariants hold.
        QSet<int> ls, rs;
        for (const auto &p : out)
        {
            if (p.first != -1)  ls.insert(p.first);
            if (p.second != -1) rs.insert(p.second);
        }
        QCOMPARE(ls.size(), 2);
        QCOMPARE(rs.size(), 2);
    }

    void alignPatienceAnchorsOnUniqueWeights()
    {
        // 1 is duplicated on both sides; 2 and 3 each appear once on
        // each side, in the SAME relative order. Patience picks 2 and
        // 3 as anchors first (they're unique on both), then LCS fills
        // in the 1s in the sub-ranges. Both 2 and 3 must end up paired.
        const QList<qint64> L = {1, 2, 1, 3, 1};
        const QList<qint64> R = {1, 2, 1, 1, 3};
        const auto out = JsonDiffEngine::alignByWeight(L, R);

        auto findPair = [&](qint64 leftValue) -> QPair<int, int> {
            for (const auto &p : out)
                if (p.first != -1 && L[p.first] == leftValue)
                    return p;
            return qMakePair(-1, -1);
        };
        const auto pair2 = findPair(2);
        const auto pair3 = findPair(3);
        QVERIFY(pair2.second != -1);
        QVERIFY(pair3.second != -1);
        QCOMPARE(R[pair2.second], qint64(2));
        QCOMPARE(R[pair3.second], qint64(3));
    }

    void alignReorderedMatchesAll()
    {
        // Left [1, 2, 3], Right [3, 2, 1]. Bipartite matches all three
        // regardless of order — the whole point of the algorithm switch.
        const QList<qint64> L = {1, 2, 3};
        const QList<qint64> R = {3, 2, 1};
        const auto out = JsonDiffEngine::alignByWeight(L, R);

        QSet<int> leftSeen, rightSeen;
        int matched = 0;
        for (const auto &p : out)
        {
            if (p.first != -1) leftSeen.insert(p.first);
            if (p.second != -1) rightSeen.insert(p.second);
            if (p.first != -1 && p.second != -1)
            {
                QCOMPARE(L[p.first], R[p.second]);
                ++matched;
            }
        }
        QCOMPARE(leftSeen.size(),  L.size());
        QCOMPARE(rightSeen.size(), R.size());
        QCOMPARE(matched, 3);
    }

    // ------------------------------------------------------------------
    // compare() populates arrayAlignment on paired Array pairs;
    // compareAsync() marshals the field across the QThread boundary.
    // ------------------------------------------------------------------

    void comparePopulatesArrayAlignmentOnPairedArrays()
    {
        // Left  {arr: [1, 2, 3]}, Right {arr: [1, 3]}.
        // FullPath will pair the `arr` node on both sides; alignment
        // should show (0,0), (1,-1), (2,1).
        QJsonModel ml;
        ml.loadJson(R"({"arr":[1,2,3]})");
        QJsonModel mr;
        mr.loadJson(R"({"arr":[1,3]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);

        DiffNode *arrL = childByKey(L, "arr");
        DiffNode *arrR = childByKey(R, "arr");
        QVERIFY(arrL);
        QVERIFY(arrR);

        // Both sides should carry an alignment for the paired arrays.
        QCOMPARE(arrL->arrayAlignment.size(), 3);
        QCOMPARE(arrR->arrayAlignment.size(), 3);

        // Bipartite output layout: matched pairs sorted by leftIdx,
        // then left orphans (leftIdx, -1).
        QCOMPARE(arrL->arrayAlignment[0], qMakePair(0, 0));
        QCOMPARE(arrL->arrayAlignment[1], qMakePair(2, 1));
        QCOMPARE(arrL->arrayAlignment[2], qMakePair(1, -1));

        // Right side view is the pair-swap of left's view.
        QCOMPARE(arrR->arrayAlignment[0], qMakePair(0, 0));
        QCOMPARE(arrR->arrayAlignment[1], qMakePair(1, 2));
        QCOMPARE(arrR->arrayAlignment[2], qMakePair(-1, 1));
    }

    void compareLeavesArrayAlignmentEmptyOnScalars()
    {
        // arrayAlignment now populates on both Object AND Array
        // containers under content-first (align mode). Scalars must
        // stay empty — nothing to align inside a leaf.
        QJsonModel ml;
        ml.loadJson(R"({"obj":{"k":"v"}})");
        QJsonModel mr;
        mr.loadJson(R"({"obj":{"k":"v"}})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);

        DiffNode *obj = childByKey(L, "obj");
        DiffNode *k   = childByKey(*obj, "k");
        QVERIFY(obj);
        QVERIFY(k);
        // Root and obj are Objects → alignment populated.
        QVERIFY(!L.arrayAlignment.isEmpty());
        QVERIFY(!obj->arrayAlignment.isEmpty());
        // Scalar leaf → empty.
        QVERIFY(k->arrayAlignment.isEmpty());
    }

    // ------------------------------------------------------------------
    // apply() + arrayAlignment: phantom row production
    // ------------------------------------------------------------------

    void applyInsertsPhantomsOnMissingRightRow()
    {
        // Left  {arr:[1,2,3]}, Right {arr:[1,3]}.
        // R needs a phantom at row 1 to align with L's orphan.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[1,2,3]})");
        mr.loadJson(R"({"arr":[1,3]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);

        // Apply on both sides.
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Left arr still has 3 real rows, no phantoms.
        QModelIndex arrLIdx;
        for (int i = 0; i < ml.rowCount(); ++i)
        {
            QModelIndex idx = ml.index(i, 0);
            if (ml.itemFromIndex(idx)->key() == "arr")
            {
                arrLIdx = idx;
                break;
            }
        }
        QVERIFY(arrLIdx.isValid());
        QCOMPARE(ml.rowCount(arrLIdx), 3);
        for (int i = 0; i < 3; ++i)
        {
            QVERIFY(!ml.itemFromIndex(ml.index(i, 0, arrLIdx))->isPhantom());
        }

        // Right arr grows from 2 rows → 3 rows; middle is phantom.
        QModelIndex arrRIdx;
        for (int i = 0; i < mr.rowCount(); ++i)
        {
            QModelIndex idx = mr.index(i, 0);
            if (mr.itemFromIndex(idx)->key() == "arr")
            {
                arrRIdx = idx;
                break;
            }
        }
        QVERIFY(arrRIdx.isValid());
        QCOMPARE(mr.rowCount(arrRIdx), 3);
        // Merge sequence places phantom at the row of the missing
        // item so rows align with the left side. L=[1,2,3], R=[1,3] →
        // R model becomes [1, phantom, 3].
        QVERIFY(!mr.itemFromIndex(mr.index(0, 0, arrRIdx))->isPhantom());
        QVERIFY( mr.itemFromIndex(mr.index(1, 0, arrRIdx))->isPhantom());
        QVERIFY(!mr.itemFromIndex(mr.index(2, 0, arrRIdx))->isPhantom());
        QCOMPARE(mr.itemFromIndex(mr.index(1, 0, arrRIdx))->colorType(),
                 DiffColorType::NotPresent);
    }

    void alignModeReorderedMatchesByIdWithDrift()
    {
        // Realistic scenario: reordered array of objects, matched by
        // id, some drift in other fields.
        //   L: [{id:100,val:"A"}, {id:200,val:"B"}, {id:300,val:"C"}]
        //   R: [{id:300,val:"C2"}, {id:100,val:"A"}, {id:200,val:"B"}]
        //     — position 0,1,2 → id 100,200,300 on left
        //     — position 0,1,2 → id 300,100,200 on right (rotated)
        //     — id 300 has drift on val ("C" vs "C2"), 100/200 identical
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[{"id":100,"val":"A"},{"id":200,"val":"B"},{"id":300,"val":"C"}]})");
        mr.loadJson(R"({"arr":[{"id":300,"val":"C2"},{"id":100,"val":"A"},{"id":200,"val":"B"}]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Walk into arr on the left side.
        QModelIndex arrIdx;
        for (int i = 0; i < ml.rowCount(); ++i)
        {
            QModelIndex k = ml.index(i, 0);
            if (ml.itemFromIndex(k)->key() == "arr") { arrIdx = k; break; }
        }
        QVERIFY(arrIdx.isValid());
        QCOMPARE(ml.rowCount(arrIdx), 3);  // no phantoms — all paired

        // For each left child, walk into it and check colors.
        auto colorOfKeyChild = [&](const QModelIndex &parent,
                                   const QString &key) -> DiffColorType {
            for (int i = 0; i < ml.rowCount(parent); ++i)
            {
                QModelIndex c = ml.index(i, 0, parent);
                if (ml.itemFromIndex(c)->key() == key)
                    return ml.itemFromIndex(c)->colorType();
            }
            return DiffColorType::None;
        };
        auto idOfRow = [&](int row) -> QString {
            QModelIndex obj = ml.index(row, 0, arrIdx);
            for (int i = 0; i < ml.rowCount(obj); ++i)
            {
                QModelIndex c = ml.index(i, 0, obj);
                QJsonTreeItem *it = ml.itemFromIndex(c);
                if (it->key() == "id") return it->value();
            }
            return QString();
        };

        // For each left object, verify its colors line up with its id.
        for (int r = 0; r < 3; ++r)
        {
            const QString id = idOfRow(r);
            QModelIndex obj = ml.index(r, 0, arrIdx);
            QJsonTreeItem *objItem = ml.itemFromIndex(obj);
            const DiffColorType idColor  = colorOfKeyChild(obj, "id");
            const DiffColorType valColor = colorOfKeyChild(obj, "val");
            QCOMPARE(idColor, DiffColorType::Identical);
            if (id == "300")
            {
                QCOMPARE(valColor, DiffColorType::Huge);
                QCOMPARE(objItem->colorType(), DiffColorType::Moderate);
            }
            else
            {
                QCOMPARE(valColor, DiffColorType::Identical);
                QCOMPARE(objItem->colorType(), DiffColorType::Identical);
            }
        }
    }

    void alignModeArrayChildrenWithoutIdFallBackToWeight()
    {
        // Children are Objects without an "id" field. identityOf falls
        // back to full-node weight. If content is identical (just
        // reordered), weights match and pairing succeeds. If content
        // drifts, weights differ → no match. This is the shape of the
        // "0 matches on 3-vs-3" case: array children whose top-level
        // keys don't include the field the user typed.
        QJsonModel ml, mr;
        // Same content, different order — should all pair.
        ml.loadJson(R"({"a":[{"name":"X","v":1},{"name":"Y","v":2}]})");
        mr.loadJson(R"({"a":[{"name":"Y","v":2},{"name":"X","v":1}]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);

        // Locate the array node.
        DiffNode *a = childByKey(L, "a");
        QVERIFY(a);
        QCOMPARE(a->arrayAlignment.size(), 2);
        int matched = 0;
        for (const auto &p : a->arrayAlignment)
            if (p.first != -1 && p.second != -1) ++matched;
        QCOMPARE(matched, 2);
    }

    void alignModeArrayChildrenWithoutIdAndDriftGivesOrphans()
    {
        // Same shape as above but the objects have drifted content →
        // full-node weights differ → nothing pairs.
        QJsonModel ml, mr;
        ml.loadJson(R"({"a":[{"name":"X","v":1},{"name":"Y","v":2}]})");
        mr.loadJson(R"({"a":[{"name":"X","v":99},{"name":"Y","v":88}]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);

        DiffNode *a = childByKey(L, "a");
        QVERIFY(a);
        // 2 left orphans + 2 right orphans = 4 entries, 0 matched.
        int matched = 0, lOrph = 0, rOrph = 0;
        for (const auto &p : a->arrayAlignment)
        {
            if (p.first != -1 && p.second != -1) ++matched;
            else if (p.first != -1) ++lOrph;
            else ++rOrph;
        }
        QCOMPARE(matched, 0);
        QCOMPARE(lOrph, 2);
        QCOMPARE(rOrph, 2);
    }

    void alignModeThreeItemArrayAllWithIdMatchAll()
    {
        // Exact shape of user's failing 3-vs-3 case: three objects on
        // each side, each carrying an "id", reordered on right side,
        // some fields drifted. Every pair must match by id.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[
            {"id":"AAA","name":"Alice","score":10},
            {"id":"BBB","name":"Bob","score":20},
            {"id":"CCC","name":"Carol","score":30}
        ]})");
        mr.loadJson(R"({"arr":[
            {"id":"CCC","name":"Carol-new","score":30},
            {"id":"AAA","name":"Alice","score":11},
            {"id":"BBB","name":"Bob","score":20}
        ]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);

        DiffNode *a = childByKey(L, "arr");
        QVERIFY(a);
        QCOMPARE(a->arrayAlignment.size(), 3);
        int matched = 0;
        for (const auto &p : a->arrayAlignment)
            if (p.first != -1 && p.second != -1) ++matched;
        QCOMPARE(matched, 3);
    }

    void alignModeThreeItemArrayIdNamedDifferentlyGivesOrphans()
    {
        // Exact shape but the id field is called "eventId" not "id".
        // With matchKey="id" my algorithm falls back to full-node
        // weight, which differs due to the "name" drift — 0 matches.
        // This documents the field-name limitation.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[
            {"eventId":"AAA","name":"Alice"},
            {"eventId":"BBB","name":"Bob"},
            {"eventId":"CCC","name":"Carol"}
        ]})");
        mr.loadJson(R"({"arr":[
            {"eventId":"CCC","name":"Carol-new"},
            {"eventId":"AAA","name":"Alice-new"},
            {"eventId":"BBB","name":"Bob"}
        ]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);

        DiffNode *a = childByKey(L, "arr");
        QVERIFY(a);
        int matched = 0;
        for (const auto &p : a->arrayAlignment)
            if (p.first != -1 && p.second != -1) ++matched;
        // "Bob" is identical on both sides → its full-node weight
        // matches → 1 pair. The other two drifted → orphans.
        QCOMPARE(matched, 1);
    }

    void alignModeNestedArraysReorderedAndDrifted()
    {
        // Top-level array has objects with id; each object contains an
        // inner array whose children don't have id. The top-level
        // matches by id; the inner array pairs by weight — succeeds
        // when reordered without drift.
        QJsonModel ml, mr;
        ml.loadJson(R"({"events":[
            {"id":100,"markets":[{"name":"M1"},{"name":"M2"}]},
            {"id":200,"markets":[{"name":"P1"},{"name":"P2"}]}
        ]})");
        mr.loadJson(R"({"events":[
            {"id":200,"markets":[{"name":"P2"},{"name":"P1"}]},
            {"id":100,"markets":[{"name":"M2"},{"name":"M1"}]}
        ]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);

        DiffNode *events = childByKey(L, "events");
        QVERIFY(events);
        int outerMatched = 0;
        for (const auto &p : events->arrayAlignment)
            if (p.first != -1 && p.second != -1) ++outerMatched;
        QCOMPARE(outerMatched, 2);
        QCOMPARE(events->color, DiffColorType::Identical);

        // Every event's markets should have paired both entries too.
        for (const auto &ev : events->children)
        {
            const DiffNode *markets = nullptr;
            for (const auto &c : ev.children)
                if (c.key == "markets") { markets = &c; break; }
            QVERIFY(markets);
            int innerMatched = 0;
            for (const auto &p : markets->arrayAlignment)
                if (p.first != -1 && p.second != -1) ++innerMatched;
            QCOMPARE(innerMatched, 2);
        }
    }

    void weightsOfEqualSelectionsMatch()
    {
        // Sanity-check: two Objects with same content but different
        // insertion order should produce equal weights.
        QJsonModel m1, m2;
        m1.loadJson(R"([{"marketName":"Full Time - 2UP","selectionId":"SBTS_2_4238631637","selectionName":"Colombia"}])");
        m2.loadJson(R"([{"selectionName":"Colombia","selectionId":"SBTS_2_4238631637","marketName":"Full Time - 2UP"}])");
        DiffNode L = JsonDiffEngine::snapshot(&m1);
        DiffNode R = JsonDiffEngine::snapshot(&m2);
        QVERIFY(!L.children.isEmpty());
        QVERIFY(!R.children.isEmpty());
        qDebug() << "L weight:" << L.children[0].weight;
        qDebug() << "R weight:" << R.children[0].weight;
        QCOMPARE(L.children[0].weight, R.children[0].weight);
    }

    void alignRealBareVsSorted()
    {
        // Load the user's actual bare.json vs sorted.json fixtures and
        // count how alignment behaves. Files live one level above the
        // repo so this is skipped on CI where they don't exist.
        QFile b("/mnt/nvmestorage/claudecode/qtjsondiff/bare.json");
        QFile s("/mnt/nvmestorage/claudecode/qtjsondiff/sorted.json");
        if (!b.exists() || !s.exists())
            QSKIP("bare.json/sorted.json fixtures not present");
        QVERIFY(b.open(QIODevice::ReadOnly));
        QVERIFY(s.open(QIODevice::ReadOnly));
        QJsonModel ml, mr;
        ml.loadJson(b.readAll());
        mr.loadJson(s.readAll());

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);

        // Walk the whole snapshot and count every Array with alignment
        // populated: total pairs and matched-pair counts.
        int arraysSeen = 0, allMatched = 0, someUnmatched = 0;
        std::function<void(const DiffNode&, const QString&)> walk =
            [&](const DiffNode &n, const QString &pathHint) {
            if (n.type == QJsonValue::Array && !n.arrayAlignment.isEmpty())
            {
                ++arraysSeen;
                int m = 0, orphans = 0;
                for (const auto &p : n.arrayAlignment)
                    if (p.first != -1 && p.second != -1) ++m;
                    else ++orphans;
                if (orphans == 0) ++allMatched;
                else
                {
                    ++someUnmatched;
                    if (someUnmatched <= 3)
                        qDebug().noquote() << "unmatched array at" << pathHint
                                           << "size=" << n.children.size()
                                           << "matched=" << m
                                           << "orphans=" << orphans;
                }
            }
            for (int i = 0; i < n.children.size(); ++i)
                walk(n.children[i], pathHint + "/" + n.children[i].key);
        };
        walk(L, "");
        qDebug() << "arrays seen:" << arraysSeen
                 << "fully matched:" << allMatched
                 << "with orphans:" << someUnmatched;
        // With the qHash fix, every selections array (same content just
        // reordered) should now match fully. 97 arrays: 1 outer accas
        // + 96 selections arrays.
        QVERIFY(arraysSeen >= 97);
        QCOMPARE(someUnmatched, 0);
    }

    void applySkipsPhantomsWhenBothSidesHaveOrphans()
    {
        // Two 2-item arrays, no matches. Prior behavior appended 2
        // phantoms on each side. New behavior: leave both sides as-is
        // (2 rows), since inserting phantoms would only duplicate the
        // orphan display.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[{"id":"A"},{"id":"B"}]})");
        mr.loadJson(R"({"arr":[{"id":"X"},{"id":"Y"}]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Both sides should still have exactly 2 rows in arr — no phantoms.
        QModelIndex arrL, arrR;
        for (int i = 0; i < ml.rowCount(); ++i)
        {
            QModelIndex k = ml.index(i, 0);
            if (ml.itemFromIndex(k)->key() == "arr") { arrL = k; break; }
        }
        for (int i = 0; i < mr.rowCount(); ++i)
        {
            QModelIndex k = mr.index(i, 0);
            if (mr.itemFromIndex(k)->key() == "arr") { arrR = k; break; }
        }
        QCOMPARE(ml.rowCount(arrL), 2);
        QCOMPARE(mr.rowCount(arrR), 2);
        for (int i = 0; i < 2; ++i)
        {
            QVERIFY(!ml.itemFromIndex(ml.index(i, 0, arrL))->isPhantom());
            QVERIFY(!mr.itemFromIndex(mr.index(i, 0, arrR))->isPhantom());
        }
    }

    void alignBareVsSortedNoMatchKeyMatchesByWeight()
    {
        // Align mode, matchKey EMPTY. bare vs sorted has semantically
        // identical content in different orders → weight-based
        // bipartite must pair everything. Zero orphans across every
        // array in the tree.
        QFile b("/mnt/nvmestorage/claudecode/qtjsondiff/bare.json");
        QFile s("/mnt/nvmestorage/claudecode/qtjsondiff/sorted.json");
        if (!b.exists() || !s.exists())
            QSKIP("bare/sorted fixtures not present");
        QVERIFY(b.open(QIODevice::ReadOnly));
        QVERIFY(s.open(QIODevice::ReadOnly));
        QJsonModel ml, mr;
        ml.loadJson(b.readAll());
        mr.loadJson(s.readAll());
        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        int arrays = 0, withOrphans = 0;
        std::function<void(const DiffNode&)> walk = [&](const DiffNode &n){
            if (n.type == QJsonValue::Array && !n.arrayAlignment.isEmpty())
            {
                ++arrays;
                for (const auto &p : n.arrayAlignment)
                    if (p.first == -1 || p.second == -1) { ++withOrphans; break; }
            }
            for (const auto &c : n.children) walk(c);
        };
        walk(L);
        qDebug() << "empty-key: arrays" << arrays << "with orphans" << withOrphans;
        QCOMPARE(withOrphans, 0);
    }

    void applyMergeSequenceAlignsRowsInsideMatchedPair()
    {
        // Outer match by id; inside the matched pair, the inner
        // "items" array is missing one row on the right. Merge
        // sequence should place a phantom at the missing position on
        // the right side so both trees line up row-for-row.
        //   L: {"id":1,"items":[A,B,C]}
        //   R: {"id":1,"items":[A,C]}       — B missing
        QJsonModel ml, mr;
        ml.loadJson(R"({"outer":[{"id":1,"items":["A","B","C"]}]})");
        mr.loadJson(R"({"outer":[{"id":1,"items":["A","C"]}]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Walk to outer[0].items on the right side.
        auto findChild = [](QJsonModel &m, const QModelIndex &parent,
                            const QString &key) -> QModelIndex {
            for (int i = 0; i < m.rowCount(parent); ++i)
            {
                QModelIndex idx = m.index(i, 0, parent);
                if (m.itemFromIndex(idx)->key() == key) return idx;
            }
            return QModelIndex();
        };
        QModelIndex outerR = findChild(mr, QModelIndex(), "outer");
        QVERIFY(outerR.isValid());
        QModelIndex obj0R = mr.index(0, 0, outerR);
        QVERIFY(obj0R.isValid());
        QModelIndex itemsR = findChild(mr, obj0R, "items");
        QVERIFY(itemsR.isValid());
        QCOMPARE(mr.rowCount(itemsR), 3);
        // Row 0 = "A" (real), row 1 = phantom (aligned with L's "B"),
        // row 2 = "C" (real).
        QVERIFY(!mr.itemFromIndex(mr.index(0, 0, itemsR))->isPhantom());
        QVERIFY( mr.itemFromIndex(mr.index(1, 0, itemsR))->isPhantom());
        QVERIFY(!mr.itemFromIndex(mr.index(2, 0, itemsR))->isPhantom());
    }

    void applyRecomparesIdMatchedPairWithDrift()
    {
        // User's exact case: two objects with same "id" but different
        // "name". Aligned by id → pair should be Moderate with only
        // the name field Huge; id and rank stay Identical.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[{"id":751,"name":"Goals","rank":5}]})");
        mr.loadJson(R"({"arr":[{"id":751,"name":"jgjjgkgk","rank":5}]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Walk down to arr[0].name on the left model.
        QModelIndex arrIdx;
        for (int i = 0; i < ml.rowCount(); ++i)
        {
            QModelIndex k = ml.index(i, 0);
            if (ml.itemFromIndex(k)->key() == "arr") { arrIdx = k; break; }
        }
        QVERIFY(arrIdx.isValid());
        QCOMPARE(ml.rowCount(arrIdx), 1);
        QModelIndex objIdx = ml.index(0, 0, arrIdx);
        QJsonTreeItem *obj = ml.itemFromIndex(objIdx);
        QVERIFY(obj);
        QCOMPARE(obj->colorType(), DiffColorType::Moderate);

        for (int i = 0; i < ml.rowCount(objIdx); ++i)
        {
            QModelIndex ci = ml.index(i, 0, objIdx);
            QJsonTreeItem *c = ml.itemFromIndex(ci);
            const QString key = c->key();
            const DiffColorType color = c->colorType();
            if (key == "name")
                QCOMPARE(color, DiffColorType::Huge);
            else
                QCOMPARE(color, DiffColorType::Identical);
        }
    }

    void applyLeavesModelUnchangedWithoutAlign()
    {
        // When compare is called without arrayOverlay=true, apply() must
        // NOT produce phantoms — preserves the positional-mode regression
        // floor and matches the "Smart Array checkbox off" case.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[1,2,3]})");
        mr.loadJson(R"({"arr":[1,3]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath); // default false

        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Right arr still 2 rows, no phantoms.
        QModelIndex arrRIdx;
        for (int i = 0; i < mr.rowCount(); ++i)
        {
            QModelIndex idx = mr.index(i, 0);
            if (mr.itemFromIndex(idx)->key() == "arr")
            {
                arrRIdx = idx;
                break;
            }
        }
        QVERIFY(arrRIdx.isValid());
        QCOMPARE(mr.rowCount(arrRIdx), 2);
        for (int i = 0; i < 2; ++i)
        {
            QVERIFY(!mr.itemFromIndex(mr.index(i, 0, arrRIdx))->isPhantom());
        }
    }

    void snapshotSkipsExistingPhantoms()
    {
        // A model that already has phantoms (from a previous apply)
        // must snapshot as if the phantoms weren't there — otherwise
        // the next compare would treat them as real orphans and stack
        // more phantoms on top.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":[1,2,3]})");
        mr.loadJson(R"({"arr":[1,3]})");

        DiffNode L1 = JsonDiffEngine::snapshot(&ml);
        DiffNode R1 = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L1, R1, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(R1, &mr, &ml);
        // Right model now has 3 rows with a phantom at index 1.

        // Re-snapshot right — the phantom must be filtered out.
        DiffNode R2 = JsonDiffEngine::snapshot(&mr);
        DiffNode *arr = childByKey(R2, "arr");
        QVERIFY(arr);
        QCOMPARE(arr->children.size(), 2);
        QCOMPARE(arr->children[0].value, QStringLiteral("1"));
        QCOMPARE(arr->children[1].value, QStringLiteral("3"));
    }

    void compareAsyncCarriesArrayAlignmentAcrossThread()
    {
        // Same input as the sync test but via compareAsync + queued
        // signal — proves the DiffNode metatype marshalling survives
        // the new field.
        JsonDiffEngine engine;
        engine.setArrayOverlay(true);
        QThread thread;
        ThreadStopper stopper{thread};
        engine.moveToThread(&thread);
        thread.start();

        QJsonModel ml;
        ml.loadJson(R"({"arr":[1,2,3]})");
        QJsonModel mr;
        mr.loadJson(R"({"arr":[1,3]})");

        auto L = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(&ml));
        auto R = QSharedPointer<DiffNode>::create(JsonDiffEngine::snapshot(&mr));

        QSignalSpy finishedSpy(&engine, &JsonDiffEngine::finished);
        QMetaObject::invokeMethod(&engine, "compareAsync", Qt::QueuedConnection,
            Q_ARG(QSharedPointer<DiffNode>, L),
            Q_ARG(QSharedPointer<DiffNode>, R),
            Q_ARG(JsonDiffEngine::Mode, JsonDiffEngine::Mode::FullPath));

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 1);

        const QList<QVariant> args = finishedSpy.takeFirst();
        auto resultLeft  = args[0].value<QSharedPointer<DiffNode>>();
        auto resultRight = args[1].value<QSharedPointer<DiffNode>>();
        QVERIFY(resultLeft);
        QVERIFY(resultRight);

        DiffNode *arrL = childByKey(*resultLeft, "arr");
        DiffNode *arrR = childByKey(*resultRight, "arr");
        QVERIFY(arrL);
        QVERIFY(arrR);
        QCOMPARE(arrL->arrayAlignment.size(), 3);
        QCOMPARE(arrL->arrayAlignment[0], qMakePair(0, 0));
        QCOMPARE(arrL->arrayAlignment[1], qMakePair(2, 1));
        QCOMPARE(arrL->arrayAlignment[2], qMakePair(1, -1));
        QCOMPARE(arrR->arrayAlignment.size(), 3);
    }

    // ------------------------------------------------------------------
    // Overlay: LCS-align paired arrays on top of positional modes.
    // Same result shape as content-first for the children of an aligned
    // array, but the OUTER walker is still FullPath / PCP.
    // ------------------------------------------------------------------

    void overlayFullPathShiftPathologyFixedByLCS()
    {
        // Left [A,B,C,D], Right [A,B,X,C,D]. Positional FullPath pairs
        // by index and marks 3 of 5 rows as Huge (shift). Overlay must
        // pair by content: A↔A, B↔B, C↔C, D↔D, right X as orphan.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":["A","B","C","D"]})");
        mr.loadJson(R"({"arr":["A","B","X","C","D"]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"",
                                /*arrayOverlay=*/true);

        DiffNode *arrL = childByKey(L, "arr");
        DiffNode *arrR = childByKey(R, "arr");
        QVERIFY(arrL);
        QVERIFY(arrR);
        QCOMPARE(arrL->arrayAlignment.size(), 5);
        QCOMPARE(arrR->arrayAlignment.size(), 5);

        int matched = 0, orphansLeft = 0, orphansRight = 0;
        for (const auto &p : arrL->arrayAlignment)
        {
            if (p.first != -1 && p.second != -1) ++matched;
            else if (p.first == -1) ++orphansRight;
            else ++orphansLeft;
        }
        QCOMPARE(matched, 4);
        QCOMPARE(orphansLeft, 0);
        QCOMPARE(orphansRight, 1);

        // Every matched pair carries an Identical color, orphan is NotPresent.
        for (const auto &p : arrL->arrayAlignment)
        {
            if (p.first != -1 && p.second != -1)
            {
                QCOMPARE(arrL->children[p.first].color, DiffColorType::Identical);
                QCOMPARE(arrR->children[p.second].color, DiffColorType::Identical);
            }
            else if (p.second != -1)
                QCOMPARE(arrR->children[p.second].color, DiffColorType::NotPresent);
        }
        // Parent array is Moderate (has an orphan child), root also Moderate.
        QCOMPARE(arrL->color, DiffColorType::Moderate);
        QCOMPARE(arrR->color, DiffColorType::Moderate);
    }

    void overlayParentChildPairSameFixture()
    {
        // Same fixture, PCP+overlay. Under PCP alone, array children
        // match by (parent.key, index) — same shift bug as FullPath.
        // Overlay must produce the same clean pairing.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":["A","B","C","D"]})");
        mr.loadJson(R"({"arr":["A","B","X","C","D"]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::ParentChildPair,
                                /*matchKey=*/"",
                                /*arrayOverlay=*/true);

        DiffNode *arrL = childByKey(L, "arr");
        DiffNode *arrR = childByKey(R, "arr");
        QVERIFY(arrL);
        QVERIFY(arrR);
        QCOMPARE(arrL->arrayAlignment.size(), 5);

        int matched = 0;
        for (const auto &p : arrL->arrayAlignment)
            if (p.first != -1 && p.second != -1) ++matched;
        QCOMPARE(matched, 4);
    }

    void overlayOffLeavesModeUntouched()
    {
        // Same fixture, FullPath, arrayOverlay=false. This is the
        // regression floor: nothing about the existing positional
        // behavior may change unless the overlay flag is on.
        QJsonModel ml, mr;
        ml.loadJson(R"({"arr":["A","B","C","D"]})");
        mr.loadJson(R"({"arr":["A","B","X","C","D"]})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"",
                                /*arrayOverlay=*/false);

        DiffNode *arrL = childByKey(L, "arr");
        QVERIFY(arrL);
        // arrayAlignment must NOT be populated when overlay is off.
        QVERIFY(arrL->arrayAlignment.isEmpty());
    }

    void overlayFullPathBareVsSortedFixtureAllPair()
    {
        // Real live fixture: bare vs sorted differ ONLY in array order.
        // FullPath alone marks lots of items as Huge because indices
        // don't line up. Overlay+matchKey must resolve every array to
        // zero orphans.
        QFile b("/mnt/nvmestorage/claudecode/qtjsondiff/bare.json");
        QFile s("/mnt/nvmestorage/claudecode/qtjsondiff/sorted.json");
        if (!b.exists() || !s.exists())
            QSKIP("bare/sorted fixtures not present");
        QVERIFY(b.open(QIODevice::ReadOnly));
        QVERIFY(s.open(QIODevice::ReadOnly));

        QJsonModel ml, mr;
        ml.loadJson(b.readAll());
        mr.loadJson(s.readAll());
        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"id",
                                /*arrayOverlay=*/true);

        int arrays = 0, withOrphans = 0;
        std::function<void(const DiffNode&)> walk = [&](const DiffNode &n){
            if (n.type == QJsonValue::Array && !n.arrayAlignment.isEmpty())
            {
                ++arrays;
                for (const auto &p : n.arrayAlignment)
                    if (p.first == -1 || p.second == -1)
                    { ++withOrphans; break; }
            }
            for (const auto &c : n.children) walk(c);
        };
        walk(L);
        QVERIFY(arrays >= 97);
        QCOMPARE(withOrphans, 0);
    }

    void overlayParentChildPairBareVsSortedFixtureAllPair()
    {
        // Same fixture, PCP outer + overlay inside. Same expectation:
        // zero orphans across every array once the arrays get LCS-aligned.
        QFile b("/mnt/nvmestorage/claudecode/qtjsondiff/bare.json");
        QFile s("/mnt/nvmestorage/claudecode/qtjsondiff/sorted.json");
        if (!b.exists() || !s.exists())
            QSKIP("bare/sorted fixtures not present");
        QVERIFY(b.open(QIODevice::ReadOnly));
        QVERIFY(s.open(QIODevice::ReadOnly));

        QJsonModel ml, mr;
        ml.loadJson(b.readAll());
        mr.loadJson(s.readAll());
        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R,
                                JsonDiffEngine::Mode::ParentChildPair,
                                /*matchKey=*/"id",
                                /*arrayOverlay=*/true);

        int arrays = 0, withOrphans = 0;
        std::function<void(const DiffNode&)> walk = [&](const DiffNode &n){
            if (n.type == QJsonValue::Array && !n.arrayAlignment.isEmpty())
            {
                ++arrays;
                for (const auto &p : n.arrayAlignment)
                    if (p.first == -1 || p.second == -1)
                    { ++withOrphans; break; }
            }
            for (const auto &c : n.children) walk(c);
        };
        walk(L);
        QVERIFY(arrays >= 97);
        QCOMPARE(withOrphans, 0);
    }

    // ------------------------------------------------------------------
    // Object phantoms — same UX as array phantoms, key-matched.
    // ------------------------------------------------------------------

    void overlayObjectMissingKeyGetsPhantomOnRight()
    {
        // {a:1, b:2, c:3} vs {a:1, c:3}. Overlay must pair a↔a and c↔c
        // and produce a phantom on right at row 1 aligning with `b` on
        // left, so both sides show 3 rows and matched pairs line up.
        QJsonModel ml, mr;
        ml.loadJson(R"({"a":1,"b":2,"c":3})");
        mr.loadJson(R"({"a":1,"c":3})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Left: 3 real rows a, b, c.
        QCOMPARE(ml.rowCount(), 3);
        QVERIFY(!ml.itemFromIndex(ml.index(0, 0))->isPhantom());
        QVERIFY(!ml.itemFromIndex(ml.index(1, 0))->isPhantom());
        QVERIFY(!ml.itemFromIndex(ml.index(2, 0))->isPhantom());

        // Right: 3 rows, phantom in the middle for `b`.
        QCOMPARE(mr.rowCount(), 3);
        QVERIFY(!mr.itemFromIndex(mr.index(0, 0))->isPhantom());
        QVERIFY(mr.itemFromIndex(mr.index(1, 0))->isPhantom());
        QVERIFY(!mr.itemFromIndex(mr.index(2, 0))->isPhantom());

        // Matched keys line up row-for-row.
        QCOMPARE(ml.itemFromIndex(ml.index(0, 0))->key(), QStringLiteral("a"));
        QCOMPARE(mr.itemFromIndex(mr.index(0, 0))->key(), QStringLiteral("a"));
        QCOMPARE(ml.itemFromIndex(ml.index(2, 0))->key(), QStringLiteral("c"));
        QCOMPARE(mr.itemFromIndex(mr.index(2, 0))->key(), QStringLiteral("c"));
    }

    void overlayObjectMissingKeyGetsPhantomOnLeft()
    {
        // Symmetric: right has extra key.
        QJsonModel ml, mr;
        ml.loadJson(R"({"a":1,"c":3})");
        mr.loadJson(R"({"a":1,"b":2,"c":3})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(L, &ml, &mr);
        JsonDiffEngine::apply(R, &mr, &ml);

        QCOMPARE(ml.rowCount(), 3);
        QVERIFY(ml.itemFromIndex(ml.index(1, 0))->isPhantom());
        QCOMPARE(mr.rowCount(), 3);
        QVERIFY(!mr.itemFromIndex(mr.index(1, 0))->isPhantom());
    }

    void overlayObjectParentChildPairSameFixture()
    {
        // PCP + overlay should produce the same phantom for `b`.
        QJsonModel ml, mr;
        ml.loadJson(R"({"a":1,"b":2,"c":3})");
        mr.loadJson(R"({"a":1,"c":3})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::ParentChildPair,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(R, &mr, &ml);

        QCOMPARE(mr.rowCount(), 3);
        QVERIFY(mr.itemFromIndex(mr.index(1, 0))->isPhantom());
    }

    void overlayNestedObjectPhantomAtInnerLevel()
    {
        // Nested case: outer key matches, missing key inside.
        QJsonModel ml, mr;
        ml.loadJson(R"({"outer":{"a":1,"b":2,"c":3}})");
        mr.loadJson(R"({"outer":{"a":1,"c":3}})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(R, &mr, &ml);

        // Right's `outer` should have 3 children with phantom at row 1.
        QModelIndex outerR;
        for (int i = 0; i < mr.rowCount(); ++i)
        {
            QModelIndex k = mr.index(i, 0);
            if (mr.itemFromIndex(k)->key() == "outer") { outerR = k; break; }
        }
        QVERIFY(outerR.isValid());
        QCOMPARE(mr.rowCount(outerR), 3);
        QVERIFY(mr.itemFromIndex(mr.index(1, 0, outerR))->isPhantom());
    }

    void overlayObjectRootLevelPhantom()
    {
        // Root-level object with missing top-level key must also
        // produce a phantom (root is the special "always paired" case
        // in the post-pass).
        QJsonModel ml, mr;
        ml.loadJson(R"({"a":1,"missing":2})");
        mr.loadJson(R"({"a":1})");

        DiffNode L = JsonDiffEngine::snapshot(&ml);
        DiffNode R = JsonDiffEngine::snapshot(&mr);
        JsonDiffEngine::compare(L, R, JsonDiffEngine::Mode::FullPath,
                                /*matchKey=*/"", /*arrayOverlay=*/true);
        JsonDiffEngine::apply(R, &mr, &ml);

        QCOMPARE(mr.rowCount(), 2);
        QVERIFY(mr.itemFromIndex(mr.index(1, 0))->isPhantom());
    }
};

QTEST_GUILESS_MAIN(TestEngine)
#include "test_engine.moc"
