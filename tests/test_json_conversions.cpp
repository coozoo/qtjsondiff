#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "qjsonmodel.h"

class TestJsonConversions : public QObject
{
    Q_OBJECT

private:
    // Helper to get the index of the testable child item.
    // Our test JSON will always be wrapped in {"root": ...}
    QModelIndex getTestableChildIndex(QJsonModel* model) {
        if (model->rowCount() > 0) {
            return model->index(0, 0, QModelIndex());
        }
        return QModelIndex();
    }

private slots:
    void testKeyValueArrayObjectRoundtrip_data()
    {
        QTest::addColumn<QString>("json");
        // Wrap the array in a root object
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

        // Get the index for the "root" item, which is the array we want to convert
        QModelIndex idx = getTestableChildIndex(&model);
        QVERIFY(idx.isValid());

        // 1. Convert Array -> Object
        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": {"bar":"car"}})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        // 2. Convert Object -> Array
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

        // 1. Convert Array -> Object
        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": {"0":"ar", "1":"bg"}})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        // 2. Convert Object -> Array
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

        // 1. Convert Object -> Array
        model.setData(idx.siblingAtColumn(1), "Array");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": ["ar", "bg"]})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        // 2. Convert Array -> Object
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

        // 1. Convert Object -> Array
        model.setData(idx.siblingAtColumn(1), "Array");
        QJsonDocument doc2 = model.getJsonDocument();
        QJsonDocument expectedDoc2 = QJsonDocument::fromJson(R"({"root": [["bar", "car"]]})");
        QCOMPARE(doc2.object(), expectedDoc2.object());

        // 2. Convert Array -> Object
        model.setData(idx.siblingAtColumn(1), "Object");
        QJsonDocument doc3 = model.getJsonDocument();
        QCOMPARE(doc3.object(), originalDoc.object());
    }
};

QTEST_MAIN(TestJsonConversions)
#include "test_json_conversions.moc"
