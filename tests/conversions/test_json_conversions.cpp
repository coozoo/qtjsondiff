#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
};

QTEST_MAIN(TestJsonConversions)
#include "test_json_conversions.moc"
