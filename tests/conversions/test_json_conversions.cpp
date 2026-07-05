#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMimeData>
#include <QRegularExpression>
#include "qjsonmodel.h"

class TestJsonConversions : public QObject
{
    Q_OBJECT

private:
    QModelIndex getTestableChildIndex(QJsonModel* model)
    {
        if (model->rowCount() > 0)
        {
            return model->index(0, 0, QModelIndex());
        }
        return QModelIndex();
    }

    QModelIndex childByKey(QJsonModel* model, const QModelIndex& parent, const QString& key)
    {
        for (int i = 0; i < model->rowCount(parent); ++i)
        {
            QModelIndex c = model->index(i, 0, parent);
            if (model->itemFromIndex(c)->key() == key)
            {
                return c;
            }
        }
        return QModelIndex();
    }

private slots:

    // ------------------------------------------------------------------
    // Existing Object<->Array round-trip tests (kept verbatim).
    // ------------------------------------------------------------------

    void testKeyValueArrayObjectRoundtrip_data()
    {
        QTest::addColumn<QString>("json");
        QTest::newRow("kv_array") << R"({"root": [["bar", "car"]]})";
    }

    void testKeyValueArrayObjectRoundtrip()
    {
        QFETCH(QString, json);
        QJsonDocument originalDoc = QJsonDocument::fromJson(json.toUtf8());

        QJsonModel model;
        model.loadJson(json.toUtf8());

        QJsonDocument doc1 = model.getJsonDocument();
        QCOMPARE(doc1, originalDoc);

        QModelIndex idx = getTestableChildIndex(&model);
        QVERIFY(idx.isValid());

        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": {"bar":"car"}})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        model.setData(idx.siblingAtColumn(1), "Array");
        QJsonDocument doc3 = model.getJsonDocument();
        QCOMPARE(doc3.object(), originalDoc.object());
    }

    void testSimpleArraySequentialObjectRoundtrip_data()
    {
        QTest::addColumn<QString>("json");
        QTest::newRow("simple_array") << R"({"root": ["ar", "bg"]})";
    }

    void testSimpleArraySequentialObjectRoundtrip()
    {
        QFETCH(QString, json);
        QJsonDocument originalDoc = QJsonDocument::fromJson(json.toUtf8());

        QJsonModel model;
        model.loadJson(json.toUtf8());

        QJsonDocument doc1 = model.getJsonDocument();
        QCOMPARE(doc1, originalDoc);

        QModelIndex idx = getTestableChildIndex(&model);
        QVERIFY(idx.isValid());

        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": {"0":"ar", "1":"bg"}})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        model.setData(idx.siblingAtColumn(1), "Array");
        QJsonDocument doc3 = model.getJsonDocument();
        QCOMPARE(doc3.object(), originalDoc.object());
    }

    void testSequentialObjectSimpleArrayRoundtrip_data()
    {
        QTest::addColumn<QString>("json");
        QTest::newRow("seq_object") << R"({"root": {"0":"ar", "1":"bg"}})";
    }

    void testSequentialObjectSimpleArrayRoundtrip()
    {
        QFETCH(QString, json);
        QJsonDocument originalDoc = QJsonDocument::fromJson(json.toUtf8());

        QJsonModel model;
        model.loadJson(json.toUtf8());

        QJsonDocument doc1 = model.getJsonDocument();
        QCOMPARE(doc1, originalDoc);

        QModelIndex idx = getTestableChildIndex(&model);
        QVERIFY(idx.isValid());

        model.setData(idx.siblingAtColumn(1), "Array");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": ["ar", "bg"]})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc3 = model.getJsonDocument();
        QCOMPARE(doc3.object(), originalDoc.object());
    }

    void testGenericObjectKeyValueArrayRoundtrip_data()
    {
        QTest::addColumn<QString>("json");
        QTest::newRow("generic_object") << R"({"root": {"bar":"car"}})";
    }

    void testGenericObjectKeyValueArrayRoundtrip()
    {
        QFETCH(QString, json);
        QJsonDocument originalDoc = QJsonDocument::fromJson(json.toUtf8());

        QJsonModel model;
        model.loadJson(json.toUtf8());

        QJsonDocument doc1 = model.getJsonDocument();
        QCOMPARE(doc1, originalDoc);

        QModelIndex idx = getTestableChildIndex(&model);
        QVERIFY(idx.isValid());

        model.setData(idx.siblingAtColumn(1), "Array");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": [["bar", "car"]]})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc3 = model.getJsonDocument();
        QCOMPARE(doc3.object(), originalDoc.object());
    }

    // ------------------------------------------------------------------
    // Load / parse
    // ------------------------------------------------------------------

    void testLoadValidObject()
    {
        QJsonModel model;
        QVERIFY(model.loadJson(R"({"a":1,"b":"two"})"));
        QVERIFY(!model.hasParseError());
        QCOMPARE(model.lastErrorMessage(), QString());
        QCOMPARE(model.rowCount(), 2);
    }

    void testLoadValidArrayRoot()
    {
        QJsonModel model;
        QVERIFY(model.loadJson(R"([1,2,3])"));
        QVERIFY(!model.hasParseError());
        QCOMPARE(model.rowCount(), 3);
        QJsonDocument doc = model.getJsonDocument();
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 3);
    }

    void testLoadEmptyObject()
    {
        QJsonModel model;
        QVERIFY(model.loadJson(R"({})"));
        QVERIFY(!model.hasParseError());
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.getJsonDocument().object(), QJsonObject{});
    }

    void testLoadEmptyArray()
    {
        QJsonModel model;
        QVERIFY(model.loadJson(R"([])"));
        QVERIFY(!model.hasParseError());
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.getJsonDocument().isArray());
        QCOMPARE(model.getJsonDocument().array().size(), 0);
    }

    void testLoadInvalidJsonSetsErrorState()
    {
        QJsonModel model;
        QSignalSpy parseSpy(&model, &QJsonModel::parseErrorChanged);
        QSignalSpy updateSpy(&model, &QJsonModel::dataUpdated);

        // Returns true (per current contract) and populates an Error placeholder model.
        QVERIFY(model.loadJson("{ this is not json"));

        QVERIFY(model.hasParseError());
        QVERIFY(!model.lastErrorMessage().isEmpty());
        QVERIFY(model.lastErrorOffset() >= 0);
        QCOMPARE(parseSpy.count(), 1);
        QCOMPARE(updateSpy.count(), 1);

        // Placeholder shape: {"Error":"...","offset":N}
        QModelIndex errorIdx = childByKey(&model, QModelIndex(), "Error");
        QVERIFY(errorIdx.isValid());
        QModelIndex offsetIdx = childByKey(&model, QModelIndex(), "offset");
        QVERIFY(offsetIdx.isValid());
    }

    void testLoadValidAfterErrorClearsFlag()
    {
        QJsonModel model;
        model.loadJson("garbage");
        QVERIFY(model.hasParseError());

        QSignalSpy parseSpy(&model, &QJsonModel::parseErrorChanged);
        model.loadJson(R"({"ok":true})");
        QVERIFY(!model.hasParseError());
        QCOMPARE(parseSpy.count(), 1);
    }

    // ------------------------------------------------------------------
    // Round-trip via getJsonDocument
    // ------------------------------------------------------------------

    void testRoundTripMixedScalars()
    {
        const QString json =
            R"({"s":"text","i":42,"f":3.5,"t":true,"f2":false,"n":null})";
        QJsonModel model;
        model.loadJson(json.toUtf8());

        QJsonDocument original = QJsonDocument::fromJson(json.toUtf8());
        QCOMPARE(model.getJsonDocument().object(), original.object());
    }

    void testRoundTripArrayOfObjects()
    {
        const QString json = R"({"items":[{"id":1,"name":"a"},{"id":2,"name":"b"}]})";
        QJsonModel model;
        model.loadJson(json.toUtf8());
        QCOMPARE(model.getJsonDocument().object(),
                 QJsonDocument::fromJson(json.toUtf8()).object());
    }

    void testRoundTripArrayOfArrays()
    {
        const QString json = R"({"grid":[[1,2],[3,4],[5,6]]})";
        QJsonModel model;
        model.loadJson(json.toUtf8());
        QCOMPARE(model.getJsonDocument().object(),
                 QJsonDocument::fromJson(json.toUtf8()).object());
    }

    // ------------------------------------------------------------------
    // setEditable + flags() matrix
    // ------------------------------------------------------------------

    void testFlagsReadOnlyByDefault()
    {
        QJsonModel model;
        model.loadJson(R"({"k":"v","arr":[1,2]})");

        QModelIndex k = childByKey(&model, QModelIndex(), "k");
        QVERIFY(k.isValid());
        QVERIFY(!(model.flags(k) & Qt::ItemIsEditable));
        QVERIFY(!(model.flags(k.siblingAtColumn(1)) & Qt::ItemIsEditable));
        QVERIFY(!(model.flags(k.siblingAtColumn(2)) & Qt::ItemIsEditable));
    }

    void testFlagsEditableObjectChild()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"k":"v"})");

        QModelIndex k = childByKey(&model, QModelIndex(), "k");
        QVERIFY(k.isValid());
        QVERIFY(model.flags(k) & Qt::ItemIsEditable);
        QVERIFY(model.flags(k.siblingAtColumn(1)) & Qt::ItemIsEditable);
        QVERIFY(model.flags(k.siblingAtColumn(2)) & Qt::ItemIsEditable);
    }

    void testFlagsArrayElementKeyNotEditable()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"arr":[1,2]})");

        QModelIndex arr = childByKey(&model, QModelIndex(), "arr");
        QModelIndex first = model.index(0, 0, arr);
        QVERIFY(first.isValid());
        // Key column is *not* editable for array elements.
        QVERIFY(!(model.flags(first) & Qt::ItemIsEditable));
        // Type and Value (scalar) columns are editable.
        QVERIFY(model.flags(first.siblingAtColumn(1)) & Qt::ItemIsEditable);
        QVERIFY(model.flags(first.siblingAtColumn(2)) & Qt::ItemIsEditable);
    }

    void testFlagsContainerValueNotEditable()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"obj":{},"arr":[],"n":null})");

        for (const char* key : {"obj", "arr", "n"})
        {
            QModelIndex idx = childByKey(&model, QModelIndex(), key);
            QVERIFY(idx.isValid());
            QVERIFY2(!(model.flags(idx.siblingAtColumn(2)) & Qt::ItemIsEditable),
                     key);
        }
    }

    // ------------------------------------------------------------------
    // setData: column 0 (key) and column 2 (value) happy paths
    // ------------------------------------------------------------------

    void testRenameKey()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"old":"value"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "old");
        QVERIFY(idx.isValid());

        QVERIFY(model.setData(idx, "fresh"));
        QJsonObject obj = model.getJsonDocument().object();
        QVERIFY(!obj.contains("old"));
        QVERIFY(obj.contains("fresh"));
        QCOMPARE(obj.value("fresh").toString(), QString("value"));
    }

    void testEditStringValue()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"k":"old"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "k");
        QVERIFY(model.setData(idx.siblingAtColumn(2), "new"));
        QCOMPARE(model.getJsonDocument().object().value("k").toString(),
                 QString("new"));
    }

    void testEditDoubleValueSerializesAsNumber()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"n":1})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "n");
        QVERIFY(model.setData(idx.siblingAtColumn(2), "3.14"));
        QJsonValue v = model.getJsonDocument().object().value("n");
        QVERIFY(v.isDouble());
        QCOMPARE(v.toDouble(), 3.14);
    }

    void testEditBoolValueSerializesAsBool()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"b":false})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "b");
        QVERIFY(model.setData(idx.siblingAtColumn(2), "true"));
        QJsonValue v = model.getJsonDocument().object().value("b");
        QVERIFY(v.isBool());
        QCOMPARE(v.toBool(), true);
    }

    // ------------------------------------------------------------------
    // Scalar <-> scalar type conversions
    // ------------------------------------------------------------------

    void testTypeConvertStringToDouble()
    {
        QJsonModel model;
        model.loadJson(R"({"x":"3.14"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Double"));
        QJsonValue v = model.getJsonDocument().object().value("x");
        QVERIFY(v.isDouble());
        QCOMPARE(v.toDouble(), 3.14);
    }

    void testTypeConvertStringTrueToDouble()
    {
        QJsonModel model;
        model.loadJson(R"({"x":"true"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Double"));
        QCOMPARE(model.getJsonDocument().object().value("x").toDouble(), 1.0);
    }

    void testTypeConvertStringFalseToDouble()
    {
        QJsonModel model;
        model.loadJson(R"({"x":"false"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Double"));
        QCOMPARE(model.getJsonDocument().object().value("x").toDouble(), 0.0);
    }

    void testTypeConvertBoolToDouble()
    {
        QJsonModel model;
        model.loadJson(R"({"x":true})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Double"));
        QCOMPARE(model.getJsonDocument().object().value("x").toDouble(), 1.0);
    }

    void testTypeConvertDoubleToString()
    {
        QJsonModel model;
        model.loadJson(R"({"x":1.5})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "String"));
        QJsonValue v = model.getJsonDocument().object().value("x");
        QVERIFY(v.isString());
        QCOMPARE(v.toString(), QString("1.5"));
    }

    void testTypeConvertNullToString()
    {
        QJsonModel model;
        model.loadJson(R"({"x":null})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "String"));
        QJsonValue v = model.getJsonDocument().object().value("x");
        QVERIFY(v.isString());
        QCOMPARE(v.toString(), QString(""));
    }

    void testTypeConvertStringToBool()
    {
        QJsonModel model;
        model.loadJson(R"({"x":"true"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Bool"));
        QJsonValue v = model.getJsonDocument().object().value("x");
        QVERIFY(v.isBool());
        QCOMPARE(v.toBool(), true);
    }

    void testTypeConvertAnyToNull()
    {
        QJsonModel model;
        model.loadJson(R"({"a":"s","b":1,"c":true})");
        for (const char* key : {"a", "b", "c"})
        {
            QModelIndex idx = childByKey(&model, QModelIndex(), key);
            QVERIFY(model.setData(idx.siblingAtColumn(1), "Null"));
        }
        QJsonObject obj = model.getJsonDocument().object();
        QVERIFY(obj.value("a").isNull());
        QVERIFY(obj.value("b").isNull());
        QVERIFY(obj.value("c").isNull());
    }

    void testTypeConvertScalarToObjectGivesEmptyObject()
    {
        QJsonModel model;
        model.loadJson(R"({"x":"text"})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Object"));
        QJsonValue v = model.getJsonDocument().object().value("x");
        QVERIFY(v.isObject());
        QCOMPARE(v.toObject(), QJsonObject{});
    }

    void testTypeConvertContainerToScalarDropsChildren()
    {
        QJsonModel model;
        model.loadJson(R"({"x":{"nested":1,"more":2}})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        QCOMPARE(model.rowCount(idx), 2);

        QVERIFY(model.setData(idx.siblingAtColumn(1), "String"));
        QCOMPARE(model.rowCount(idx), 0);
        QJsonValue v = model.getJsonDocument().object().value("x");
        QVERIFY(v.isString());
    }

    void testTypeConvertNoOpReturnsTrue()
    {
        QJsonModel model;
        model.loadJson(R"({"x":1})");
        QModelIndex idx = childByKey(&model, QModelIndex(), "x");
        // Same type, no-op path: must still return true and not corrupt model.
        QVERIFY(model.setData(idx.siblingAtColumn(1), "Double"));
        QCOMPARE(model.getJsonDocument().object().value("x").toDouble(), 1.0);
    }

    // ------------------------------------------------------------------
    // Signal contract
    // ------------------------------------------------------------------

    void testSetDataEmitsSignals()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"k":"v"})");

        QModelIndex idx = childByKey(&model, QModelIndex(), "k");
        QSignalSpy dataChangedSpy(&model, &QJsonModel::dataChanged);
        QSignalSpy modelChangedSpy(&model, &QJsonModel::modelChanged);

        QVERIFY(model.setData(idx.siblingAtColumn(2), "v2"));
        QCOMPARE(dataChangedSpy.count(), 1);
        QCOMPARE(modelChangedSpy.count(), 1);

        QVERIFY(model.setData(idx, "renamed"));
        QCOMPARE(dataChangedSpy.count(), 2);
        QCOMPARE(modelChangedSpy.count(), 2);

        QVERIFY(model.setData(idx.siblingAtColumn(1), "Double"));
        QVERIFY(modelChangedSpy.count() >= 3);
    }

    void testLoadJsonEmitsDataUpdated()
    {
        QJsonModel model;
        QSignalSpy spy(&model, &QJsonModel::dataUpdated);
        QVERIFY(model.loadJson(R"({"k":1})"));
        QCOMPARE(spy.count(), 1);
    }

    // ------------------------------------------------------------------
    // replaceFromJson — Phase A "copy from peer" in-place replace
    // ------------------------------------------------------------------

    void replaceFromJsonScalarOverScalar()
    {
        QJsonModel m;
        m.loadJson(R"({"k":"old"})");
        QModelIndex k = childByKey(&m, QModelIndex(), "k");

        QVERIFY(m.replaceFromJson(k, QJsonValue(42.0)));

        QCOMPARE(m.itemFromIndex(k)->key(),  QString("k"));     // key preserved
        QCOMPARE(m.itemFromIndex(k)->type(), QJsonValue::Double);
        QJsonValue v = m.getJsonDocument().object().value("k");
        QVERIFY(v.isDouble());
        QCOMPARE(v.toDouble(), 42.0);
    }

    void replaceFromJsonContainerOverScalar()
    {
        QJsonModel m;
        m.loadJson(R"({"k":"old"})");
        QModelIndex k = childByKey(&m, QModelIndex(), "k");

        QJsonObject obj;
        obj["x"] = 1;
        obj["y"] = QJsonValue(true);
        QVERIFY(m.replaceFromJson(k, obj));

        QCOMPARE(m.itemFromIndex(k)->type(),       QJsonValue::Object);
        QCOMPARE(m.itemFromIndex(k)->childCount(), 2);
        QJsonObject out = m.getJsonDocument().object().value("k").toObject();
        QCOMPARE(out.value("x").toDouble(), 1.0);
        QCOMPARE(out.value("y").toBool(),   true);
    }

    void replaceFromJsonScalarOverContainer()
    {
        QJsonModel m;
        m.loadJson(R"({"k":{"nested":1,"more":2}})");
        QModelIndex k = childByKey(&m, QModelIndex(), "k");
        QCOMPARE(m.itemFromIndex(k)->childCount(), 2);

        QVERIFY(m.replaceFromJson(k, QJsonValue("hello")));

        QCOMPARE(m.itemFromIndex(k)->type(),       QJsonValue::String);
        QCOMPARE(m.itemFromIndex(k)->childCount(), 0);
        QCOMPARE(m.getJsonDocument().object().value("k").toString(), QString("hello"));
    }

    void replaceFromJsonArrayOverArray()
    {
        QJsonModel m;
        m.loadJson(R"({"arr":[1,2,3]})");
        QModelIndex arr = childByKey(&m, QModelIndex(), "arr");
        QCOMPARE(m.itemFromIndex(arr)->childCount(), 3);

        QJsonArray newArr;
        newArr.append("a"); newArr.append("b");
        QVERIFY(m.replaceFromJson(arr, newArr));

        QCOMPARE(m.itemFromIndex(arr)->childCount(), 2);
        QJsonArray out = m.getJsonDocument().object().value("arr").toArray();
        QCOMPARE(out.size(),         2);
        QCOMPARE(out[0].toString(),  QString("a"));
        QCOMPARE(out[1].toString(),  QString("b"));
    }

    void replaceFromJsonEmitsSignals()
    {
        QJsonModel m;
        m.loadJson(R"({"k":"old"})");
        QModelIndex k = childByKey(&m, QModelIndex(), "k");

        QSignalSpy dataChangedSpy(&m, &QJsonModel::dataChanged);
        QSignalSpy modelChangedSpy(&m, &QJsonModel::modelChanged);
        QSignalSpy dataUpdatedSpy(&m, &QJsonModel::dataUpdated);

        QVERIFY(m.replaceFromJson(k, QJsonValue("new")));
        QVERIFY(dataChangedSpy.count()  >= 1);
        QCOMPARE(modelChangedSpy.count(), 1);
        // Structural change → dataUpdated also fires so QJsonContainer
        // resets its find/goto caches and refreshes the diff counter.
        QCOMPARE(dataUpdatedSpy.count(), 1);
    }

    void typeChangeEmitsDataUpdated()
    {
        // Pre-existing latent bug, fixed alongside Phase A: the column-1
        // type-change branch of setData is structural (children
        // removed/inserted) and must trigger find/goto cache reset.
        QJsonModel m;
        m.loadJson(R"({"k":{"x":1,"y":2}})");
        QModelIndex k = childByKey(&m, QModelIndex(), "k");

        QSignalSpy dataUpdatedSpy(&m, &QJsonModel::dataUpdated);
        QVERIFY(m.setData(k.siblingAtColumn(1), "String"));  // Object -> String, drops 2 children
        QCOMPARE(dataUpdatedSpy.count(), 1);
    }

    void replaceFromJsonRejectsInvalid()
    {
        QJsonModel m;
        m.loadJson(R"({"k":"v"})");
        QVERIFY(!m.replaceFromJson(QModelIndex(), QJsonValue("x")));
        QVERIFY(!m.replaceFromJson(m.index(0, 0), QJsonValue(QJsonValue::Undefined)));
    }

    // ------------------------------------------------------------------
    // appendChildFromJson / removeRowAt — Phase B "add / delete row"
    // ------------------------------------------------------------------

    void appendChildScalarToObject()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1})");
        // root parent, object: key matters.
        QModelIndex newIdx = m.appendChildFromJson(QModelIndex(), "b", QJsonValue("hi"));
        QVERIFY(newIdx.isValid());
        QCOMPARE(m.itemFromIndex(newIdx)->key(),  QString("b"));
        QCOMPARE(m.itemFromIndex(newIdx)->type(), QJsonValue::String);
        QJsonObject obj = m.getJsonDocument().object();
        QCOMPARE(obj.value("a").toDouble(), 1.0);
        QCOMPARE(obj.value("b").toString(), QString("hi"));
    }

    void appendChildContainerToObject()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1})");
        QJsonObject child;
        child["x"] = 1;
        child["y"] = QJsonValue(true);
        QModelIndex newIdx = m.appendChildFromJson(QModelIndex(), "nested", child);
        QVERIFY(newIdx.isValid());
        QCOMPARE(m.itemFromIndex(newIdx)->type(),       QJsonValue::Object);
        QCOMPARE(m.itemFromIndex(newIdx)->childCount(), 2);
        QJsonObject out = m.getJsonDocument().object();
        QCOMPARE(out.value("nested").toObject().value("x").toDouble(), 1.0);
        QCOMPARE(out.value("nested").toObject().value("y").toBool(),   true);
    }

    void appendChildToArrayUsesIndexAsKey()
    {
        QJsonModel m;
        m.loadJson(R"({"arr":[10,20]})");
        QModelIndex arrIdx = childByKey(&m, QModelIndex(), "arr");
        // For arrays, caller-supplied key is ignored; the model uses the
        // new index. Verifies the loadJson convention is preserved.
        QModelIndex newIdx = m.appendChildFromJson(arrIdx, "ignored", QJsonValue(30.0));
        QVERIFY(newIdx.isValid());
        QCOMPARE(m.itemFromIndex(newIdx)->key(), QString("2"));
        QJsonArray out = m.getJsonDocument().object().value("arr").toArray();
        QCOMPARE(out.size(),         3);
        QCOMPARE(out[2].toDouble(),  30.0);
    }

    void appendChildRejectsScalarParent()
    {
        QJsonModel m;
        m.loadJson(R"({"scalar":"x"})");
        QModelIndex sc = childByKey(&m, QModelIndex(), "scalar");
        QModelIndex newIdx = m.appendChildFromJson(sc, "anything", QJsonValue(1.0));
        QVERIFY(!newIdx.isValid());
    }

    void appendChildEmitsSignals()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1})");
        QSignalSpy modelChangedSpy(&m, &QJsonModel::modelChanged);
        QSignalSpy dataUpdatedSpy(&m, &QJsonModel::dataUpdated);
        QVERIFY(m.appendChildFromJson(QModelIndex(), "b", QJsonValue(2.0)).isValid());
        QCOMPARE(modelChangedSpy.count(), 1);
        QCOMPARE(dataUpdatedSpy.count(), 1);
    }

    void appendChildRejectsEmptyKeyForObject()
    {
        // QJsonObject::insert("", v) is legal but every subsequent
        // empty-key insert overwrites the prior one on serialize. The
        // model refuses to accept the empty key so integrators don't
        // silently corrupt JSON.
        QJsonModel m;
        m.loadJson(R"({"a":1})");
        QSignalSpy modelChangedSpy(&m, &QJsonModel::modelChanged);
        QSignalSpy dataUpdatedSpy(&m, &QJsonModel::dataUpdated);
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression("appendChildFromJson: empty key for Object parent"));
        QModelIndex newIdx = m.appendChildFromJson(QModelIndex(),
                                                  QString(),
                                                  QJsonValue(2.0));
        QVERIFY(!newIdx.isValid());
        QCOMPARE(modelChangedSpy.count(), 0);
        QCOMPARE(dataUpdatedSpy.count(), 0);
        // Tree intact.
        QJsonObject out = m.getJsonDocument().object();
        QCOMPARE(out.size(), 1);
        QCOMPARE(out.value("a").toDouble(), 1.0);
    }

    void appendChildEmptyKeyForArrayIsFine()
    {
        // Arrays override the caller's key with the row index, so an
        // empty key is harmless on that path.
        QJsonModel m;
        m.loadJson(R"({"arr":[10,20]})");
        QModelIndex arrIdx = childByKey(&m, QModelIndex(), "arr");
        QModelIndex newIdx = m.appendChildFromJson(arrIdx,
                                                   QString(),
                                                   QJsonValue(30.0));
        QVERIFY(newIdx.isValid());
        QCOMPARE(m.itemFromIndex(newIdx)->key(), QString("2"));
        QJsonArray out = m.getJsonDocument().object().value("arr").toArray();
        QCOMPARE(out.size(),        3);
        QCOMPARE(out[2].toDouble(), 30.0);
    }

    void insertChildRejectsEmptyKeyForObject()
    {
        // insertChildFromJson delegates to appendChildFromJson for
        // Object parents (object children are unordered) so the empty-
        // key rejection should propagate without a separate guard.
        QJsonModel m;
        m.loadJson(R"({"a":1,"b":2})");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression("appendChildFromJson: empty key for Object parent"));
        QModelIndex newIdx = m.insertChildFromJson(QModelIndex(), 0,
                                                  QString(),
                                                  QJsonValue(3.0));
        QVERIFY(!newIdx.isValid());
        QJsonObject out = m.getJsonDocument().object();
        QCOMPARE(out.size(), 2);
    }

    void removeRowAtScalar()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1,"b":2,"c":3})");
        QModelIndex bIdx = childByKey(&m, QModelIndex(), "b");
        QSignalSpy modelChangedSpy(&m, &QJsonModel::modelChanged);
        QSignalSpy dataUpdatedSpy(&m, &QJsonModel::dataUpdated);

        QVERIFY(m.removeRowAt(bIdx));
        QCOMPARE(modelChangedSpy.count(), 1);
        QCOMPARE(dataUpdatedSpy.count(), 1);
        QJsonObject out = m.getJsonDocument().object();
        QVERIFY( out.contains("a"));
        QVERIFY(!out.contains("b"));
        QVERIFY( out.contains("c"));
    }

    void removeRowAtContainerWithChildren()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1,"big":{"x":1,"y":2,"z":[1,2,3]},"c":3})");
        QModelIndex bigIdx = childByKey(&m, QModelIndex(), "big");
        QVERIFY(m.removeRowAt(bigIdx));
        QJsonObject out = m.getJsonDocument().object();
        QVERIFY(!out.contains("big"));
        QCOMPARE(out.size(), 2);
    }

    void removeRowAtKeepsRowCountConsistentAcrossSignals()
    {
        // I-001: Between beginRemoveRows and endRemoveRows the model
        // must report the pre-remove row count (Qt QAIM contract).
        // The pre-fix implementation did takeChildren() + setChildren()
        // which briefly emptied the parent's mChilds — so an observer
        // that queried rowCount() mid-transaction saw 0.
        //
        // This test connects a lambda to rowsAboutToBeRemoved that
        // queries rowCount(parent). If the pre-remove state is intact,
        // the returned count matches the initial count.
        QJsonModel m;
        m.loadJson(R"({"a":1,"b":2,"c":3})");
        const int initial = m.rowCount(QModelIndex());
        QCOMPARE(initial, 3);

        int observedInAboutTo = -1;
        int observedInRemoved = -1;
        QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeRemoved,
                         [&](const QModelIndex &p, int, int) {
                             observedInAboutTo = m.rowCount(p);
                         });
        QObject::connect(&m, &QAbstractItemModel::rowsRemoved,
                         [&](const QModelIndex &p, int, int) {
                             observedInRemoved = m.rowCount(p);
                         });

        QModelIndex bIdx = childByKey(&m, QModelIndex(), "b");
        QVERIFY(m.removeRowAt(bIdx));

        QCOMPARE(observedInAboutTo, initial);       // pre-remove state
        QCOMPARE(observedInRemoved, initial - 1);   // post-remove state
    }

    void insertChildFromJsonKeepsRowCountConsistentAcrossSignals()
    {
        // Symmetric guard for insertChildFromJson's array-mid-insert
        // path. Between beginInsertRows and endInsertRows the model
        // must report the pre-insert row count in rowsAboutToBeInserted
        // and the post-insert count in rowsInserted.
        QJsonModel m;
        m.loadJson(R"({"arr":[10,20,30,40]})");
        QModelIndex arr = childByKey(&m, QModelIndex(), "arr");
        const int initial = m.rowCount(arr);
        QCOMPARE(initial, 4);

        int observedInAboutTo = -1;
        int observedInInserted = -1;
        QObject::connect(&m, &QAbstractItemModel::rowsAboutToBeInserted,
                         [&](const QModelIndex &p, int, int) {
                             observedInAboutTo = m.rowCount(p);
                         });
        QObject::connect(&m, &QAbstractItemModel::rowsInserted,
                         [&](const QModelIndex &p, int, int) {
                             observedInInserted = m.rowCount(p);
                         });

        // row=2 forces the mid-insert path (row != count, array parent).
        QVERIFY(m.insertChildFromJson(arr, 2, QString(), QJsonValue(25.0)).isValid());

        QCOMPARE(observedInAboutTo, initial);       // pre-insert
        QCOMPARE(observedInInserted, initial + 1);  // post-insert
    }

    void removeRowAtRejectsInvalidAndRoot()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1})");
        QVERIFY(!m.removeRowAt(QModelIndex()));     // invalid → rejected
        // Root has no parent — removeRowAt rejects when item == mRootItem.
        // We can't construct a "root" QModelIndex except as invalid, so
        // this is the same case as above; documenting via comment.
    }

    void removeRowAtRenumbersArrayKeys()
    {
        // Array [10,20,30,40]: keys "0","1","2","3". Removing index 1
        // must renumber to "0","1","2" — not "0","2","3".
        QJsonModel m;
        m.loadJson(R"({"arr":[10,20,30,40]})");
        QModelIndex arr = childByKey(&m, QModelIndex(), "arr");
        QModelIndex one = m.index(1, 0, arr);
        QCOMPARE(m.itemFromIndex(one)->key(), QString("1"));

        QVERIFY(m.removeRowAt(one));

        QCOMPARE(m.rowCount(arr), 3);
        QCOMPARE(m.itemFromIndex(m.index(0, 0, arr))->key(), QString("0"));
        QCOMPARE(m.itemFromIndex(m.index(1, 0, arr))->key(), QString("1"));
        QCOMPARE(m.itemFromIndex(m.index(2, 0, arr))->key(), QString("2"));
        // Values shifted correctly with the renumber.
        QJsonArray out = m.getJsonDocument().object().value("arr").toArray();
        QCOMPARE(out.size(),         3);
        QCOMPARE(out[0].toDouble(),  10.0);
        QCOMPARE(out[1].toDouble(),  30.0);
        QCOMPARE(out[2].toDouble(),  40.0);
    }

    void removeRowAtNoRenumberForObjectKeys()
    {
        // Object children carry meaningful keys — those must NOT change
        // when a sibling is deleted.
        QJsonModel m;
        m.loadJson(R"({"a":1,"b":2,"c":3})");
        QModelIndex bIdx = childByKey(&m, QModelIndex(), "b");
        QVERIFY(m.removeRowAt(bIdx));
        QCOMPARE(m.rowCount(QModelIndex()), 2);
        QCOMPARE(m.itemFromIndex(m.index(0, 0, QModelIndex()))->key(),
                 QString("a"));
        QCOMPARE(m.itemFromIndex(m.index(1, 0, QModelIndex()))->key(),
                 QString("c"));   // not "b"
    }

    void removeRowAtArrayEmitsKeyDataChanged()
    {
        // Renumbering survivors fires dataChanged on the key column so
        // any view re-fetches the new displayed key.
        QJsonModel m;
        m.loadJson(R"({"arr":[10,20,30]})");
        QModelIndex arr = childByKey(&m, QModelIndex(), "arr");
        QSignalSpy dataChangedSpy(&m, &QJsonModel::dataChanged);

        QVERIFY(m.removeRowAt(m.index(0, 0, arr)));   // remove "0"
        // Two surviving siblings renumber from "1","2" → "0","1".
        // Expect at least one dataChanged per renamed sibling (in
        // addition to whatever the structural removal emitted).
        int keyColumnChanges = 0;
        for (const auto &args : dataChangedSpy)
        {
            QModelIndex tl = args.at(0).value<QModelIndex>();
            if (tl.column() == 0 && tl.parent() == arr)
                ++keyColumnChanges;
        }
        QVERIFY(keyColumnChanges >= 2);
    }

    // ------------------------------------------------------------------
    // Round-trip after multi-step edits
    // ------------------------------------------------------------------

    void testRoundTripAfterEdits()
    {
        QJsonModel model;
        model.setEditable(true);
        model.loadJson(R"({"a":"x","b":1})");

        QModelIndex a = childByKey(&model, QModelIndex(), "a");
        QVERIFY(model.setData(a.siblingAtColumn(2), "y"));      // value
        QVERIFY(model.setData(a, "alpha"));                     // rename

        QModelIndex b = childByKey(&model, QModelIndex(), "b");
        QVERIFY(model.setData(b.siblingAtColumn(1), "String")); // type

        QJsonDocument first = model.getJsonDocument();

        // Reload serialized form into a fresh model and compare.
        QJsonModel model2;
        QVERIFY(model2.loadJson(first.toJson()));
        QCOMPARE(model2.getJsonDocument(), first);
    }

    // ------------------------------------------------------------------
    // Drag-and-drop: MIME + flags + drop semantics
    // ------------------------------------------------------------------

    void testDndFlagsRequireEditable()
    {
        // Non-editable: drag/drop bits absent. Read-only contract preserved.
        QJsonModel m;
        m.loadJson(R"({"a":1,"b":[10,20]})");
        QModelIndex a = childByKey(&m, QModelIndex(), "a");
        QVERIFY(!(m.flags(a) & Qt::ItemIsDragEnabled));
        QVERIFY(!(m.flags(QModelIndex()) & Qt::ItemIsDropEnabled));

        // Editable: any non-root item is draggable; containers (Object,
        // Array, and root because root is an Object here) accept drops.
        m.setEditable(true);
        QVERIFY(m.flags(a) & Qt::ItemIsDragEnabled);
        QModelIndex b = childByKey(&m, QModelIndex(), "b");
        QVERIFY(m.flags(b) & Qt::ItemIsDragEnabled);
        QVERIFY(m.flags(b) & Qt::ItemIsDropEnabled);       // array
        QVERIFY(m.flags(QModelIndex()) & Qt::ItemIsDropEnabled);  // root object
    }

    void testDndScalarRejectsDrops()
    {
        // A scalar value cannot accept children — flag is off, and
        // canDropMimeData refuses.
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"s":"hi","arr":[1]})");
        QModelIndex s = childByKey(&m, QModelIndex(), "s");
        QVERIFY(!(m.flags(s) & Qt::ItemIsDropEnabled));

        // Build a valid payload and verify canDropMimeData says no.
        QModelIndex src = m.index(0, 0, childByKey(&m, QModelIndex(), "arr"));
        QMimeData *md = m.mimeData(QModelIndexList{src});
        QVERIFY(md);
        QVERIFY(!m.canDropMimeData(md, Qt::CopyAction, -1, -1, s));
        delete md;
    }

    void testDndMimeRoundTripScalar()
    {
        // mimeData of an array element + dropMimeData onto an object
        // parent yields a new child with the source value.
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"obj":{},"arr":[42,"hi",true]})");
        QModelIndex arr = childByKey(&m, QModelIndex(), "arr");
        QModelIndex obj = childByKey(&m, QModelIndex(), "obj");

        QMimeData *md = m.mimeData(QModelIndexList{m.index(0, 0, arr)});
        QVERIFY(md);
        QVERIFY(m.canDropMimeData(md, Qt::CopyAction, -1, -1, obj));
        QVERIFY(m.dropMimeData(md, Qt::CopyAction, -1, -1, obj));
        delete md;

        // Object parent: source key was "0" (array index) — pre-deduped
        // because "0" doesn't exist in obj — so the new entry's key is
        // "0" and value is 42.
        QCOMPARE(m.rowCount(obj), 1);
        QModelIndex newChild = m.index(0, 0, obj);
        QCOMPARE(m.itemFromIndex(newChild)->key(),   QStringLiteral("0"));
        QCOMPARE(m.itemFromIndex(newChild)->value(), QStringLiteral("42"));
        QCOMPARE(m.itemFromIndex(newChild)->type(),  QJsonValue::Double);
    }

    void testDndMimeRoundTripContainer()
    {
        // Dropping a nested subtree preserves structure all the way down.
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"src":{"deep":{"k":"v","list":[1,2]}},"dst":{}})");
        QModelIndex deep = childByKey(&m,
                                      childByKey(&m, QModelIndex(), "src"),
                                      "deep");
        QModelIndex dst = childByKey(&m, QModelIndex(), "dst");

        QMimeData *md = m.mimeData(QModelIndexList{deep});
        QVERIFY(md);
        QVERIFY(m.dropMimeData(md, Qt::CopyAction, -1, -1, dst));
        delete md;

        // dst now has a "deep" child mirroring the source.
        QModelIndex newDeep = childByKey(&m, dst, "deep");
        QVERIFY(newDeep.isValid());
        QCOMPARE(m.itemFromIndex(newDeep)->type(), QJsonValue::Object);
        QCOMPARE(m.rowCount(newDeep), 2);
        QVERIFY(childByKey(&m, newDeep, "k").isValid());
        QVERIFY(childByKey(&m, newDeep, "list").isValid());
        QCOMPARE(m.rowCount(childByKey(&m, newDeep, "list")), 2);
    }

    void testDndObjectKeyDedup()
    {
        // Dropping a child with a key that collides with an existing
        // sibling key gets a "_1" / "_2" suffix.
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"a":{"x":1},"b":{"x":2}})");
        QModelIndex a = childByKey(&m, QModelIndex(), "a");
        QModelIndex b = childByKey(&m, QModelIndex(), "b");

        QMimeData *md = m.mimeData(QModelIndexList{m.index(0, 0, a)});
        QVERIFY(md);
        QVERIFY(m.dropMimeData(md, Qt::CopyAction, -1, -1, b));
        delete md;

        // b now has "x" (existing) and "x_1" (the dropped copy).
        QCOMPARE(m.rowCount(b), 2);
        QVERIFY(childByKey(&m, b, "x").isValid());
        QVERIFY(childByKey(&m, b, "x_1").isValid());
    }

    void testDndArrayMidRowInsert()
    {
        // dropMimeData with a specific row on an array inserts at that
        // position; surviving siblings renumber to keep their displayed
        // keys consecutive (matches removeRowAt's invariant).
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"src":99,"arr":[10,20,30]})");
        QModelIndex src = childByKey(&m, QModelIndex(), "src");
        QModelIndex arr = childByKey(&m, QModelIndex(), "arr");

        QMimeData *md = m.mimeData(QModelIndexList{src});
        QVERIFY(md);
        QVERIFY(m.dropMimeData(md, Qt::CopyAction, 1, -1, arr));  // insert at row 1
        delete md;

        QCOMPARE(m.rowCount(arr), 4);
        // arr is now [10, 99, 20, 30] with keys "0","1","2","3".
        for (int i = 0; i < 4; ++i)
        {
            QModelIndex c = m.index(i, 0, arr);
            QCOMPARE(m.itemFromIndex(c)->key(), QString::number(i));
        }
        QCOMPARE(m.itemFromIndex(m.index(0, 0, arr))->value(), QStringLiteral("10"));
        QCOMPARE(m.itemFromIndex(m.index(1, 0, arr))->value(), QStringLiteral("99"));
        QCOMPARE(m.itemFromIndex(m.index(2, 0, arr))->value(), QStringLiteral("20"));
        QCOMPARE(m.itemFromIndex(m.index(3, 0, arr))->value(), QStringLiteral("30"));
    }

    void testDndRejectsWhenNotEditable()
    {
        // dropMimeData refuses outright when the model is not editable,
        // even if the MIME payload is well-formed. canDropMimeData also
        // says no. Source side can still mimeData (read-only inspection),
        // but the destination side won't accept it.
        QJsonModel src;
        src.setEditable(true);
        src.loadJson(R"({"x":1})");
        QModelIndex x = childByKey(&src, QModelIndex(), "x");
        QMimeData *md = src.mimeData(QModelIndexList{x});
        QVERIFY(md);

        QJsonModel dst;
        dst.loadJson(R"({"y":2})");  // NOT editable
        QVERIFY(!dst.canDropMimeData(md, Qt::CopyAction, -1, -1, QModelIndex()));
        QVERIFY(!dst.dropMimeData(md, Qt::CopyAction, -1, -1, QModelIndex()));
        QCOMPARE(dst.rowCount(QModelIndex()), 1);  // unchanged
        delete md;
    }

    void testDndDropAtRoot()
    {
        // Empty / invalid parent = root. Allowed when root is a container
        // and model is editable.
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"a":1})");

        QJsonObject env;
        env.insert("key",   "b");
        env.insert("value", 42);
        QMimeData md;
        md.setData(QJsonModel::kItemMimeType,
                   QJsonDocument(env).toJson(QJsonDocument::Compact));

        QVERIFY(m.canDropMimeData(&md, Qt::CopyAction, -1, -1, QModelIndex()));
        QVERIFY(m.dropMimeData(&md, Qt::CopyAction, -1, -1, QModelIndex()));
        QCOMPARE(m.rowCount(QModelIndex()), 2);
        QVERIFY(childByKey(&m, QModelIndex(), "b").isValid());
    }

    void testDndFileUrlMimeIgnored()
    {
        // Sanity: a text/uri-list MIME payload (what a file drag carries)
        // is rejected by the model — keeps the existing file-drop path on
        // the surrounding groupbox eventFilter free of contention.
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"a":1})");

        QMimeData md;
        md.setData("text/uri-list", "file:///tmp/example.json\r\n");
        QVERIFY(!m.canDropMimeData(&md, Qt::CopyAction, -1, -1, QModelIndex()));
        QVERIFY(!m.dropMimeData(&md, Qt::CopyAction, -1, -1, QModelIndex()));
    }

    // -----------------------------------------------------------------
    // Phase 4: phantom rows in QJsonModel
    // -----------------------------------------------------------------

    // Find the first-level child by key on a model. Returns nullptr if
    // not found. Test helper only — the model's own itemFromIndex plus
    // walk-children is fine for a single level.
    static QJsonTreeItem* firstLevelChildByKey(QJsonModel &m, const QString &k)
    {
        const QModelIndex root = QModelIndex();
        const int n = m.rowCount(root);
        for (int i = 0; i < n; ++i)
        {
            QModelIndex idx = m.index(i, 0, root);
            QJsonTreeItem *item = static_cast<QJsonTreeItem*>(idx.internalPointer());
            if (item && item->key() == k)
                return item;
        }
        return nullptr;
    }

    void phantomIsFalseByDefault()
    {
        QJsonModel m;
        m.loadJson(R"({"a":1,"b":2})");
        QJsonTreeItem *a = firstLevelChildByKey(m, "a");
        QVERIFY(a);
        QVERIFY(!a->isPhantom());
    }

    void getJsonDocumentSkipsPhantomsInObject()
    {
        QJsonModel m;
        m.loadJson(R"({"real":1,"ghost":2})");
        QJsonTreeItem *ghost = firstLevelChildByKey(m, "ghost");
        QVERIFY(ghost);
        ghost->setPhantom(true);

        const QJsonDocument out = m.getJsonDocument();
        QVERIFY(out.isObject());
        const QJsonObject obj = out.object();
        QVERIFY( obj.contains("real"));
        QVERIFY(!obj.contains("ghost"));
    }

    void getJsonDocumentSkipsPhantomsInArray()
    {
        QJsonModel m;
        m.loadJson(R"({"arr":[10,20,30]})");
        QJsonTreeItem *arr = firstLevelChildByKey(m, "arr");
        QVERIFY(arr);
        QCOMPARE(arr->childCount(), 3);
        // Mark the middle element phantom.
        arr->child(1)->setPhantom(true);

        const QJsonDocument out = m.getJsonDocument();
        const QJsonArray a = out.object().value("arr").toArray();
        QCOMPARE(a.size(), 2);
        QCOMPARE(a[0].toInt(), 10);
        QCOMPARE(a[1].toInt(), 30);
    }

    void flagsRejectPhantomEdit()
    {
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"a":1})");
        QJsonTreeItem *a = firstLevelChildByKey(m, "a");
        QVERIFY(a);
        // Non-phantom scalar: value column is editable.
        const int n = m.rowCount(QModelIndex());
        QModelIndex valueIdx;
        for (int i = 0; i < n; ++i)
        {
            QModelIndex k = m.index(i, 0, QModelIndex());
            if (static_cast<QJsonTreeItem*>(k.internalPointer())->key() == "a")
            {
                valueIdx = m.index(i, 2, QModelIndex());
                break;
            }
        }
        QVERIFY(valueIdx.isValid());
        QVERIFY(m.flags(valueIdx) & Qt::ItemIsEditable);

        // Flip to phantom → editable bit drops off.
        a->setPhantom(true);
        QVERIFY(!(m.flags(valueIdx) & Qt::ItemIsEditable));
    }

    void setDataRejectsPhantom()
    {
        QJsonModel m;
        m.setEditable(true);
        m.loadJson(R"({"a":"hello"})");
        QJsonTreeItem *a = firstLevelChildByKey(m, "a");
        QVERIFY(a);
        a->setPhantom(true);
        // Value column write must fail and not change the item.
        QModelIndex valueIdx;
        const int n = m.rowCount(QModelIndex());
        for (int i = 0; i < n; ++i)
        {
            QModelIndex k = m.index(i, 0, QModelIndex());
            if (static_cast<QJsonTreeItem*>(k.internalPointer())->key() == "a")
            {
                valueIdx = m.index(i, 2, QModelIndex());
                break;
            }
        }
        QVERIFY(valueIdx.isValid());
        QVERIFY(!m.setData(valueIdx, "goodbye", Qt::EditRole));
        QCOMPARE(a->value(), QStringLiteral("hello"));
    }
};

QTEST_MAIN(TestJsonConversions)
#include "test_json_conversions.moc"
