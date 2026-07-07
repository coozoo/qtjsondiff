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
#include <QComboBox>
#include <QSignalSpy>
#include <QMimeData>

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
        diff->modeCombo->setCurrentIndex(
            useFullPath ? int(JsonDiffEngine::Mode::FullPath)
                        : int(JsonDiffEngine::Mode::ParentChildPair));
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
    // Identical JSONs - both modes
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
    // Single nested scalar diff - ancestors should be promoted to Moderate
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
    // type - per README). Parent+child mode treats it as a Huge diff
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
    // Deeply nested structural difference - ancestor chain promoted
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

    // ------------------------------------------------------------------
    // Phase A - edit-in-diff: push selected value INTO peer
    // ------------------------------------------------------------------

    void editPushLeftScalarToRight()
    {
        loadAndCompare(R"({"k":"old"})", R"({"k":"new"})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QModelIndex leftK  = resolvePath(L, {"k"});
        QModelIndex rightK = resolvePath(R, {"k"});

        QCOMPARE(L->itemFromIndex(leftK)->colorType(),  DiffColorType::Huge);
        QCOMPARE(R->itemFromIndex(rightK)->colorType(), DiffColorType::Huge);

        diff->getLeftTreeView()->setCurrentIndex(leftK);
        diff->pushLeftSelectionToRight();

        // Right "k" picked up left's value; both sides resolved.
        QCOMPARE(R->itemFromIndex(rightK)->value(),     QString("old"));
        QCOMPARE(L->itemFromIndex(leftK)->colorType(),  DiffColorType::Identical);
        QCOMPARE(R->itemFromIndex(rightK)->colorType(), DiffColorType::Identical);
    }

    void editPushRightScalarToLeft()
    {
        loadAndCompare(R"({"k":"old"})", R"({"k":"new"})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QModelIndex rightK = resolvePath(R, {"k"});

        diff->getRightTreeView()->setCurrentIndex(rightK);
        diff->pushRightSelectionToLeft();

        QCOMPARE(L->itemFromIndex(resolvePath(L, {"k"}))->value(),
                 QString("new"));
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Identical);
    }

    void editPushDemotesAncestorModerate()
    {
        // {"a":{"b":"old"}} vs {"a":{"b":"new"}}
        // b is Huge, a is Moderate. After push, both Identical.
        loadAndCompare(R"({"a":{"b":"old"}})",
                       R"({"a":{"b":"new"}})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"a"}),       DiffColorType::Moderate);
        QCOMPARE(colorAt(L, {"a", "b"}),  DiffColorType::Huge);

        QModelIndex leftB = resolvePath(L, {"a", "b"});
        diff->getLeftTreeView()->setCurrentIndex(leftB);
        diff->pushLeftSelectionToRight();

        QCOMPARE(colorAt(L, {"a", "b"}), DiffColorType::Identical);
        QCOMPARE(colorAt(L, {"a"}),      DiffColorType::Identical);  // demoted
        QCOMPARE(colorAt(R, {"a", "b"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"a"}),      DiffColorType::Identical);
    }

    void editPushKeepsAncestorModerateWhenSiblingDiffers()
    {
        // {"a":{"b":"old","c":"x"}} vs {"a":{"b":"new","c":"y"}}
        // Push b only - a stays Moderate because c is still Huge.
        loadAndCompare(R"({"a":{"b":"old","c":"x"}})",
                       R"({"a":{"b":"new","c":"y"}})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QModelIndex leftB = resolvePath(L, {"a", "b"});
        diff->getLeftTreeView()->setCurrentIndex(leftB);
        diff->pushLeftSelectionToRight();

        QCOMPARE(colorAt(L, {"a", "b"}), DiffColorType::Identical);
        QCOMPARE(colorAt(L, {"a", "c"}), DiffColorType::Huge);
        QCOMPARE(colorAt(L, {"a"}),      DiffColorType::Moderate);  // not demoted
    }

    void editPushNoOpWhenEditableOff()
    {
        loadAndCompare(R"({"k":"old"})", R"({"k":"new"})", true);
        // Explicitly reset edit mode (the diff widget is shared across
        // tests via initTestCase, so a previous test may have toggled
        // it on). The point of this test: the slot ISN'T gated by
        // editable - only the overlay button visibility is. The check
        // below confirms the slot still works programmatically.
        diff->setDiffEditable(false);
        QCOMPARE(diff->isDiffEditable(), false);

        QJsonModel *L = diff->getLeftJsonModel();
        QModelIndex leftK = resolvePath(L, {"k"});
        diff->getLeftTreeView()->setCurrentIndex(leftK);
        diff->pushLeftSelectionToRight();

        // The slot itself is not gated - that's by design (programmatic
        // use). The overlay is what enforces the user-facing edit-mode
        // gate. Both sides Identical after this call.
        QCOMPARE(colorAt(L, {"k"}),                     DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"k"}),
                                                        DiffColorType::Identical);
    }

    void editPushArrayShrinksRight()
    {
        // Container Huge: same path + same Array type, different child
        // counts. The compare finds them as a Huge pair with a valid
        // peer link, so push overwrites the right side's children.
        loadAndCompare(R"({"k":[1,2]})", R"({"k":[1,2,3,4]})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Huge);
        QCOMPARE(R->itemFromIndex(resolvePath(R, {"k"}))->childCount(), 4);

        QModelIndex leftK = resolvePath(L, {"k"});
        diff->getLeftTreeView()->setCurrentIndex(leftK);
        diff->pushLeftSelectionToRight();

        // Right's array now has 2 elements matching left's.
        QModelIndex rightK = R->index(0, 0);
        QCOMPARE(R->itemFromIndex(rightK)->type(),       QJsonValue::Array);
        QCOMPARE(R->itemFromIndex(rightK)->childCount(), 2);
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Identical);
    }

    void editPushScalarOverContainerParentChild()
    {
        // Parent+Child mode pairs same-key/different-type as Huge
        // (second-stage fallback), so the push works across types.
        loadAndCompare(R"({"k":"hello"})",
                       R"({"k":{"x":1,"y":2}})", false);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Huge);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Huge);

        QModelIndex leftK = resolvePath(L, {"k"});
        diff->getLeftTreeView()->setCurrentIndex(leftK);
        diff->pushLeftSelectionToRight();

        QModelIndex rightK = R->index(0, 0);
        QCOMPARE(R->itemFromIndex(rightK)->type(),       QJsonValue::String);
        QCOMPARE(R->itemFromIndex(rightK)->value(),      QString("hello"));
        QCOMPARE(R->itemFromIndex(rightK)->childCount(), 0);
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Identical);
    }

    // ------------------------------------------------------------------
    // Phase B - edit-in-diff: push NotPresent INTO other side (insert)
    //                          and delete-here from one side
    // ------------------------------------------------------------------

    void editPushNotPresentRootChildToOtherSide()
    {
        // {"only_left":1} vs {"only_right":2}. After push left's
        // "only_left" lands on the right at the root level.
        loadAndCompare(R"({"only_left":1})", R"({"only_right":2})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"only_left"}),  DiffColorType::NotPresent);
        QCOMPARE(colorAt(R, {"only_right"}), DiffColorType::NotPresent);

        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"only_left"}));
        diff->pushLeftSelectionToRight();

        QVERIFY(exists(R, {"only_left"}));
        QVERIFY(exists(R, {"only_right"}));
        QCOMPARE(colorAt(L, {"only_left"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"only_left"}), DiffColorType::Identical);
        QCOMPARE(R->itemFromIndex(resolvePath(R, {"only_left"}))->value(),
                 QString("1"));
    }

    void editPushNotPresentNestedToMatchedParent()
    {
        // {"a":{"keep":1,"extra":2}} vs {"a":{"keep":1}}. "extra" is
        // NotPresent on left under the matched "a". Push appends it
        // into right's "a".
        loadAndCompare(R"({"a":{"keep":1,"extra":2}})",
                       R"({"a":{"keep":1}})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"a","extra"}), DiffColorType::NotPresent);
        QVERIFY(!exists(R, {"a","extra"}));

        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"a","extra"}));
        diff->pushLeftSelectionToRight();

        QVERIFY(exists(R, {"a","extra"}));
        QCOMPARE(colorAt(L, {"a","extra"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"a","extra"}), DiffColorType::Identical);
    }

    void editPushNotPresentSubtreeCrossLinksInteriors()
    {
        // Pushing a NotPresent CONTAINER must light up its interior
        // children with idxRelation too - not just the top.
        loadAndCompare(R"({"keep":0,"big":{"x":1,"y":2}})",
                       R"({"keep":0})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"big"}));
        diff->pushLeftSelectionToRight();

        QVERIFY(exists(R, {"big","x"}));
        QVERIFY(exists(R, {"big","y"}));
        QCOMPARE(colorAt(R, {"big","x"}), DiffColorType::Identical);
        QCOMPARE(R->itemFromIndex(resolvePath(R, {"big","x"}))->idxRelation().isValid(), true);
    }

    void editPushNotPresentNoOpWhenParentAlsoMissing()
    {
        // {"deep":{"leaf":1}} vs {}. Both "deep" and "deep/leaf" are
        // NotPresent on left. Push of "deep/leaf" must fail because
        // its parent has no peer on the right. The user has to push
        // "deep" first.
        loadAndCompare(R"({"deep":{"leaf":1}})", R"({})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();

        // Behavioral: the arrow action stays in the toolbar always
        // (so the user's eye doesn't track a vanishing/appearing
        // button), but it's DISABLED on "deep/leaf" because there's
        // nowhere to copy it (parent also missing). It's ENABLED on
        // "deep" itself (root is always a valid matched parent on
        // the other side).
        QAction *leftPush = nullptr;
        for (QAction *a : diff->left_cont->toolbar->actions())
            if (a->text() == QObject::tr("Push to right")) leftPush = a;
        QVERIFY(leftPush);

        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"deep","leaf"}));
        QCOMPARE(leftPush->isVisible(), true);
        QCOMPARE(leftPush->isEnabled(), false);

        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"deep"}));
        QCOMPARE(leftPush->isVisible(), true);
        QCOMPARE(leftPush->isEnabled(), true);

        // Clicking "deep/leaf" must still be a safe no-op (defence in
        // depth - the slot itself should refuse even if the action got
        // triggered programmatically).
        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"deep","leaf"}));
        diff->pushLeftSelectionToRight();
        QVERIFY(!exists(R, {"deep"}));
        QVERIFY(!exists(R, {"deep","leaf"}));

        // Then push the parent - both should land in one step.
        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"deep"}));
        diff->pushLeftSelectionToRight();
        QVERIFY(exists(R, {"deep","leaf"}));
    }

    void editPushNotPresentRightToLeft()
    {
        // Symmetric to editPushNotPresentRootChildToOtherSide.
        loadAndCompare(R"({"a":1})", R"({"a":1,"b":2})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(R, {"b"}), DiffColorType::NotPresent);

        diff->getRightTreeView()->setCurrentIndex(resolvePath(R, {"b"}));
        diff->pushRightSelectionToLeft();

        QVERIFY(exists(L, {"b"}));
        QCOMPARE(colorAt(L, {"b"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"b"}), DiffColorType::Identical);
    }

    void editDeleteNotPresentRemovesRowOnlyFromThatSide()
    {
        // Deleting a NotPresent row on left removes it and the right
        // side is untouched (no peer to orphan).
        loadAndCompare(R"({"a":1,"only_left":2})", R"({"a":1})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"only_left"}));
        diff->deleteLeftSelection();

        QVERIFY(!exists(L, {"only_left"}));
        QVERIFY( exists(L, {"a"}));
        QVERIFY( exists(R, {"a"}));
    }

    void editDeletePairedRowOrphansPeer()
    {
        // Deleting a Huge-paired row on left turns the right peer into
        // NotPresent (not "removed" on right too - the symmetric
        // delete is the user's separate next click).
        loadAndCompare(R"({"k":"old","x":"same"})",
                       R"({"k":"new","x":"same"})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Huge);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Huge);

        diff->getLeftTreeView()->setCurrentIndex(resolvePath(L, {"k"}));
        diff->deleteLeftSelection();

        QVERIFY(!exists(L, {"k"}));
        QVERIFY( exists(R, {"k"}));
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::NotPresent);
    }

    // ------------------------------------------------------------------
    // Phase B+ - single-tree Add child / Delete row on QJsonContainer
    // ------------------------------------------------------------------

    void containerAddChildToRootObject()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"a":1})"));
        c.setEditable(true);

        QModelIndex newIdx = c.addChildOf(QModelIndex());
        QVERIFY(newIdx.isValid());
        QCOMPARE(c.getJsonModel()->itemFromIndex(newIdx)->key(),
                 QString("new_key"));
        QCOMPARE(c.getJsonModel()->itemFromIndex(newIdx)->type(),
                 QJsonValue::String);
        QJsonObject out = c.getJsonModel()->getJsonDocument().object();
        QVERIFY(out.contains("a"));
        QVERIFY(out.contains("new_key"));
    }

    void containerAddChildUniqueKeyOnObject()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"new_key":"taken","new_key_1":"also"})"));
        c.setEditable(true);

        QModelIndex newIdx = c.addChildOf(QModelIndex());
        QVERIFY(newIdx.isValid());
        // "new_key" and "new_key_1" already exist → "new_key_2".
        QCOMPARE(c.getJsonModel()->itemFromIndex(newIdx)->key(),
                 QString("new_key_2"));
    }

    void containerAddChildToArrayUsesIndexKey()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"arr":[10,20]})"));
        c.setEditable(true);

        // Find the "arr" row, append to it.
        QJsonModel *m = c.getJsonModel();
        QModelIndex arrIdx;
        for (int i = 0; i < m->rowCount(QModelIndex()); ++i)
        {
            QModelIndex idx = m->index(i, 0, QModelIndex());
            if (m->itemFromIndex(idx)->key() == "arr") { arrIdx = idx; break; }
        }
        QVERIFY(arrIdx.isValid());

        QModelIndex newIdx = c.addChildOf(arrIdx);
        QVERIFY(newIdx.isValid());
        // Array auto-keys: third element gets key "2".
        QCOMPARE(m->itemFromIndex(newIdx)->key(), QString("2"));
    }

    void containerAddChildRejectsScalarParent()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"k":"x"})"));
        c.setEditable(true);

        QJsonModel *m = c.getJsonModel();
        QModelIndex k = m->index(0, 0, QModelIndex());
        QCOMPARE(m->itemFromIndex(k)->type(), QJsonValue::String);

        QModelIndex newIdx = c.addChildOf(k);
        QVERIFY(!newIdx.isValid());
    }

    void containerAddChildNoOpWhenNotEditable()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"a":1})"));
        // Default: editable off. Add should refuse without flipping
        // model state (so integrators can wire shortcuts unconditionally).
        QModelIndex newIdx = c.addChildOf(QModelIndex());
        QVERIFY(!newIdx.isValid());
        QCOMPARE(c.getJsonModel()->rowCount(QModelIndex()), 1);
    }

    void containerRemoveAtRemovesRow()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"a":1,"b":2})"));
        c.setEditable(true);

        QJsonModel *m = c.getJsonModel();
        QModelIndex bIdx;
        for (int i = 0; i < m->rowCount(QModelIndex()); ++i)
        {
            QModelIndex idx = m->index(i, 0, QModelIndex());
            if (m->itemFromIndex(idx)->key() == "b") { bIdx = idx; break; }
        }
        QVERIFY(bIdx.isValid());
        QVERIFY(c.removeAt(bIdx));
        QJsonObject out = m->getJsonDocument().object();
        QVERIFY( out.contains("a"));
        QVERIFY(!out.contains("b"));
    }

    void editInlineValueChangeFlipsIdenticalToHuge()
    {
        // User-reported: an Identical row edited in place stays
        // Identical. With the dataChanged hook it should flip to Huge
        // on both sides.
        loadAndCompare(R"({"k":"v"})", R"({"k":"v"})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Identical);

        QModelIndex valueCell = resolvePath(L, {"k"}).siblingAtColumn(2);
        QVERIFY(L->setData(valueCell, QString("changed"), Qt::EditRole));

        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Huge);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Huge);
    }

    void editInlineKeyRenameDetachesPair()
    {
        // Renaming a paired key detaches: both sides go NotPresent
        // (the slot now names a different item - re-pairing requires
        // a fresh Compare).
        loadAndCompare(R"({"k":"v"})", R"({"k":"v"})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Identical);

        QModelIndex keyCell = resolvePath(L, {"k"}).siblingAtColumn(0);
        QVERIFY(L->setData(keyCell, QString("k_renamed"), Qt::EditRole));

        QCOMPARE(colorAt(L, {"k_renamed"}), DiffColorType::NotPresent);
        QCOMPARE(colorAt(R, {"k"}),         DiffColorType::NotPresent);
    }

    void editInlineTypeChangeOnContainerResnapshots()
    {
        // {"k":{"x":1}} == {"k":{"x":1}}: identical with paired x.
        // Change LEFT k's type from Object to String via setData.
        // Expect: left k = Huge, left k's old child "x" disappears
        // from the model AND from the snapshot - and right's "x"
        // gets orphaned to NotPresent (the cross-link points into a
        // subtree that no longer exists).
        loadAndCompare(R"({"k":{"x":1}})", R"({"k":{"x":1}})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QCOMPARE(colorAt(L, {"k"}),       DiffColorType::Identical);
        QCOMPARE(colorAt(L, {"k", "x"}),  DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"k", "x"}),  DiffColorType::Identical);

        QModelIndex kIdx = resolvePath(L, {"k"});
        QVERIFY(L->setData(kIdx.siblingAtColumn(1), "String"));

        // Pair color at k flipped to Huge.
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Huge);
        QCOMPARE(colorAt(R, {"k"}), DiffColorType::Huge);
        // Left's children are gone (Object → String drops them).
        QCOMPARE(L->itemFromIndex(kIdx)->childCount(), 0);
        // Right's x is orphaned (lost its peer).
        QCOMPARE(colorAt(R, {"k", "x"}), DiffColorType::NotPresent);
    }

    void editInlineFlipBackToIdenticalWhenValueReverts()
    {
        // Edit Identical → Huge → edit back to original → Identical.
        loadAndCompare(R"({"k":"v"})", R"({"k":"v"})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QModelIndex valueCell = resolvePath(L, {"k"}).siblingAtColumn(2);
        QVERIFY(L->setData(valueCell, QString("changed"), Qt::EditRole));
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Huge);

        QVERIFY(L->setData(valueCell, QString("v"), Qt::EditRole));
        QCOMPARE(colorAt(L, {"k"}), DiffColorType::Identical);
        QCOMPARE(colorAt(diff->getRightJsonModel(), {"k"}),
                 DiffColorType::Identical);
    }

    void editDeleteArrayElementRenumbers()
    {
        // {"arr":[10,20,30]} vs {"arr":[10,20,30]}, delete arr[1] on
        // left → left's array becomes [10,30] with keys "0","1" - not
        // "0","2". Right's "1" becomes NotPresent (orphaned peer) and
        // right's "2" relationPath slides from {0,2} → {0,1}.
        loadAndCompare(R"({"arr":[10,20,30]})",
                       R"({"arr":[10,20,30]})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QModelIndex arrLeft = resolvePath(L, {"arr"});
        QModelIndex one = L->index(1, 0, arrLeft);
        diff->getLeftTreeView()->setCurrentIndex(one);
        diff->deleteLeftSelection();

        QCOMPARE(L->rowCount(arrLeft), 2);
        QCOMPARE(L->itemFromIndex(L->index(0, 0, arrLeft))->key(), QString("0"));
        QCOMPARE(L->itemFromIndex(L->index(1, 0, arrLeft))->key(), QString("1"));
        QCOMPARE(L->itemFromIndex(L->index(1, 0, arrLeft))->value(),
                 QString("30"));
        // Right side: orphan + shifted peer.
        QModelIndex arrRight = resolvePath(R, {"arr"});
        QCOMPARE(R->itemFromIndex(R->index(1, 0, arrRight))->colorType(),
                 DiffColorType::NotPresent);
        // Right's "2" now points at left's slot 1 (formerly slot 2).
        QCOMPARE(R->itemFromIndex(R->index(2, 0, arrRight))->idxRelation(),
                 L->index(1, 0, arrLeft));
    }

    void containerRemoveAtNoOpWhenNotEditable()
    {
        QJsonContainer c(host);
        c.loadJson(QString(R"({"a":1})"));
        // Default: editable off.
        QModelIndex aIdx = c.getJsonModel()->index(0, 0, QModelIndex());
        QVERIFY(!c.removeAt(aIdx));
        QCOMPARE(c.getJsonModel()->rowCount(QModelIndex()), 1);
    }

    // ------------------------------------------------------------------
    // Drag-and-drop: item dropped on the other side becomes gray
    // (NotPresent) and the drop-target parent's pair color updates.
    // ------------------------------------------------------------------

    void dndDropIntoMatchedParentMarksGray()
    {
        // Identical objects on both sides → all Identical. Drop a new
        // key onto the right side: the new node is NotPresent, and the
        // root pair flips to Huge (child counts diverge) with the root
        // ancestor going Moderate via the existing fixColors path.
        loadAndCompare(R"({"a":1,"b":2})",
                       R"({"a":1,"b":2})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();

        // Build the MIME payload from the left side, drop onto the
        // right root. We go through the model's API directly (no
        // QDrag spin-up) since the slot under test is rowsInserted.
        QModelIndex a = resolvePath(L, {"a"});
        QMimeData *md = L->mimeData(QModelIndexList{a});
        QVERIFY(md);
        QVERIFY(R->dropMimeData(md, Qt::CopyAction, -1, -1, QModelIndex()));
        delete md;

        // The dropped node landed with key "a" already taken → deduped
        // to "a_1". Its color is NotPresent (no peer on the left).
        QModelIndex newOnRight = resolvePath(R, {"a_1"});
        QVERIFY(newOnRight.isValid());
        QCOMPARE(R->itemFromIndex(newOnRight)->colorType(),
                 DiffColorType::NotPresent);
        // No cross-link - left side has nothing matched here.
        QVERIFY(!R->itemFromIndex(newOnRight)->idxRelation().isValid());

        // Sanity: pre-existing matched siblings keep their color.
        QCOMPARE(colorAt(R, {"a"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"b"}), DiffColorType::Identical);
    }

    void dndDropIntoNestedContainerRefreshesParent()
    {
        // Drop a new entry into a matched nested object. The dropped
        // node is NotPresent, and the matched parent's pair color
        // refreshes - the parent now has more children on one side.
        loadAndCompare(R"({"obj":{"x":1}})",
                       R"({"obj":{"x":1}})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QModelIndex objLeft  = resolvePath(L, {"obj"});
        QModelIndex objRight = resolvePath(R, {"obj"});

        // Before: matched.
        QCOMPARE(R->itemFromIndex(objRight)->colorType(),
                 DiffColorType::Identical);

        // Drop "x" from left's obj onto right's obj.
        QModelIndex xLeft = resolvePath(L, {"obj", "x"});
        QMimeData *md = L->mimeData(QModelIndexList{xLeft});
        QVERIFY(md);
        QVERIFY(R->dropMimeData(md, Qt::CopyAction, -1, -1, objRight));
        delete md;

        // The dropped duplicate is NotPresent. The parent's pair color
        // is no longer Identical now that the child counts diverge.
        QModelIndex newOnRight = resolvePath(R, {"obj", "x_1"});
        QVERIFY(newOnRight.isValid());
        QCOMPARE(R->itemFromIndex(newOnRight)->colorType(),
                 DiffColorType::NotPresent);
        QVERIFY(R->itemFromIndex(objRight)->colorType() != DiffColorType::Identical);
    }

    void dndDropPreservesUnaffectedPeerColors()
    {
        // Pre-existing matched siblings on both sides keep their
        // Identical state; only the newly-inserted node is NotPresent.
        loadAndCompare(R"({"keep":"same","x":1})",
                       R"({"keep":"same","x":1})", true);
        diff->setDiffEditable(true);

        QJsonModel *L = diff->getLeftJsonModel();
        QJsonModel *R = diff->getRightJsonModel();
        QModelIndex x = resolvePath(L, {"x"});

        QMimeData *md = L->mimeData(QModelIndexList{x});
        QVERIFY(md);
        QVERIFY(R->dropMimeData(md, Qt::CopyAction, -1, -1, QModelIndex()));
        delete md;

        // "keep" matched-peer link survives - we asserted on the
        // pre-drop pair, drop only adds a new node; existing
        // relationPaths are untouched by the snapshot splice.
        QCOMPARE(colorAt(L, {"keep"}), DiffColorType::Identical);
        QCOMPARE(colorAt(R, {"keep"}), DiffColorType::Identical);
        QCOMPARE(R->itemFromIndex(resolvePath(R, {"keep"}))->idxRelation(),
                 resolvePath(L, {"keep"}));
    }

    // Locks in that JsonDiffEngine::apply's inline-collected
    // diffIndices() list matches what QJsonContainer::fillGotoList
    // would produce - so QJsonContainer::on_model_dataUpdated can
    // hydrate its gotoIndexes_list from the model without triggering
    // the O(N) post-apply walk that used to freeze the UI.
    void diffIndicesEqualsFillGotoList()
    {
        loadAndCompare(
            R"({"a":1,"b":2,"c":{"x":1,"y":2},"d":[1,2,3]})",
            R"({"a":1,"b":9,"c":{"x":1,"y":9},"d":[1,2,9,4]})",
            true);

        QJsonModel* L = diff->getLeftJsonModel();
        QJsonModel* R = diff->getRightJsonModel();

        QCOMPARE(L->diffIndices(),
                 diff->left_cont->fillGotoList(L, QModelIndex()));
        QCOMPARE(R->diffIndices(),
                 diff->right_cont->fillGotoList(R, QModelIndex()));
        // Sanity: the compare above produces at least one diff on
        // each side, otherwise the equality above would be vacuously
        // trivial (two empty lists).
        QVERIFY(L->diffIndices().size() > 0);
        QVERIFY(R->diffIndices().size() > 0);
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
