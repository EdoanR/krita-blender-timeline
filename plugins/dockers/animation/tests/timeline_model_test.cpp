/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "timeline_model_test.h"

#include "kis_image.h"
#include "kis_node.h"
#include "kis_raster_keyframe_channel.h"
#include "kis_time_span.h"
#include <limits>
#include <kundo2stack.h>
#include "kis_paint_device.h"

#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QTemporaryDir>
#include <QKeyEvent>
#include <QMouseEvent>
#include "KisAnimTimelineTransform.h"
#include "KisAnimTimelineSummary.h"
#include "KisAnimTimelineRangeField.h"
#include <QLineEdit>
#include <QWheelEvent>

#include "kis_image_animation_interface.h"
#include "KisDocument.h"
#include "KisNodeDisplayModeAdapter.h"
#include "KisPart.h"
#include "kis_name_server.h"
#include "flake/kis_shape_controller.h"
#include "kis_undo_adapter.h"

#include "KisAnimTimelineFramesView.h"
#include "KisAnimTimelineFramesModel.h"

#include "kis_node_dummies_graph.h"

#include "commands/kis_image_layer_add_command.h"
#include "commands/kis_image_layer_remove_command.h"

#include "kis_double_parse_spin_box.h"
#include "kis_int_parse_spin_box.h"

#include <testui.h>
#include <testutil.h>


void TimelineModelTest::init()
{
    // TESTUI's harness does not enable this by default. The headless model
    // tests need the same synchronous node notifications as KISTEST's other modes.
    KisSynchronizedConnectionBase::setAutoModeForUnittestsEnabled(true);
    m_doc = KisPart::instance()->createDocument();

    m_nameServer = new KisNameServer();
    m_shapeController = new KisShapeController(m_nameServer, m_doc->undoStack());
    m_displayModeAdapter = new KisNodeDisplayModeAdapter();
    //m_nodeModel = new KisNodeModel(0);

    initBase();

    m_doc->setCurrentImage(m_image);
}

void TimelineModelTest::cleanup()
{
    cleanupBase();

    //delete m_nodeModel;
    delete m_shapeController;
    delete m_nameServer;
    delete m_doc;
    delete m_displayModeAdapter;
}

#include "timeline_frames_index_converter.h"


void TimelineModelTest::testConverter()
{
    constructImage();
    addSelectionMasks();
    m_shapeController->setImage(m_image);

    m_layer1->enableAnimation();

    // all nodes are visible in timeline by default, so
    // we should hide the ones we don't expect to see
    m_mask1->setPinnedToTimeline(false);
    m_sel1->setPinnedToTimeline(false);
    m_sel2->setPinnedToTimeline(false);
    m_layer3->setPinnedToTimeline(false);
    m_layer4->setPinnedToTimeline(false);

    m_layer1->setPinnedToTimeline(true);
    m_layer2->setPinnedToTimeline(true);
    m_sel3->setPinnedToTimeline(true);

    TimelineFramesIndexConverter converter(m_shapeController);

    QCOMPARE(converter.rowCount(), 3);
    QCOMPARE(converter.rowForDummy(m_shapeController->dummyForNode(m_layer1)), 2);
    QCOMPARE(converter.rowForDummy(m_shapeController->dummyForNode(m_layer2)), 1);
    QCOMPARE(converter.rowForDummy(m_shapeController->dummyForNode(m_sel3)), 0);

    QCOMPARE(converter.dummyFromRow(2), m_shapeController->dummyForNode(m_layer1));
    QCOMPARE(converter.dummyFromRow(1), m_shapeController->dummyForNode(m_layer2));
    QCOMPARE(converter.dummyFromRow(0), m_shapeController->dummyForNode(m_sel3));

    TimelineNodeListKeeper keeper(0, m_shapeController, m_displayModeAdapter);

    QCOMPARE(keeper.rowCount(), 3);
    QCOMPARE(keeper.rowForDummy(m_shapeController->dummyForNode(m_layer1)), 2);
    QCOMPARE(keeper.rowForDummy(m_shapeController->dummyForNode(m_layer2)), 1);
    QCOMPARE(keeper.rowForDummy(m_shapeController->dummyForNode(m_sel3)), 0);

    QCOMPARE(keeper.dummyFromRow(2), m_shapeController->dummyForNode(m_layer1));
    QCOMPARE(keeper.dummyFromRow(1), m_shapeController->dummyForNode(m_layer2));
    QCOMPARE(keeper.dummyFromRow(0), m_shapeController->dummyForNode(m_sel3));

    TimelineNodeListKeeper::OtherLayersList list = keeper.otherLayersList();

    Q_FOREACH (const TimelineNodeListKeeper::OtherLayer &l, list) {
        qDebug() << ppVar(l.name) << ppVar(l.dummy->node()->name());
    }

}

void TimelineModelTest::testModel()
{
    QScopedPointer<KisAnimTimelineFramesModel> model(new KisAnimTimelineFramesModel(0));
}

void TimelineModelTest::testRetimeFrames()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_layer1->enableAnimation();
    auto channel = m_layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    QVERIFY(channel);
    channel->addKeyframe(2);
    channel->addKeyframe(4);
    channel->addKeyframe(10);
    KisPaintLayerSP secondLayer = new KisPaintLayer(m_image, "second animated layer", OPACITY_OPAQUE_U8);
    m_image->addNode(secondLayer);
    secondLayer->setPinnedToTimeline(true);
    secondLayer->enableAnimation();
    auto secondChannel = secondLayer->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    QVERIFY(secondChannel);
    secondChannel->addKeyframe(3);
    secondChannel->addKeyframe(4);
    m_shapeController->setImage(m_image);
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    int row = -1;
    int secondRow = -1;
    for (int r = 0; r < model.rowCount(); ++r) {
        if (model.nodeAt(model.index(r, 0)).data() == m_layer1.data()) row = r;
        if (model.nodeAt(model.index(r, 0)).data() == secondLayer.data()) secondRow = r;
    }
    QVERIFY(row >= 0);
    QVERIFY(secondRow >= 0);
    QVERIFY(model.summaryKeyframeTimes().contains(2));
    QVERIFY(model.summaryKeyframeTimes().contains(3));
    QModelIndexList selected{model.index(row, 2), model.index(row, 4)};
    QVERIFY(!model.retimeFrames(selected, 4.0)); // destination 10 is occupied
    const auto sourceKey = channel->keyframeAt(4);
    const auto replacedKey = channel->keyframeAt(10);
    QVERIFY(model.retimeFrames(selected, 4.0, 0, nullptr, -1, true, true));
    QVERIFY(model.retimeFrames(selected, 4.0, 0, nullptr, -1, false, true));
    m_image->waitForDone();
    QVERIFY(!channel->keyframeAt(4));
    QCOMPARE(channel->keyframeAt(10), sourceKey);
    m_doc->undoStack()->undo();
    m_image->waitForDone();
    QCOMPARE(channel->keyframeAt(4), sourceKey);
    QCOMPARE(channel->keyframeAt(10), replacedKey);
    QVERIFY(!model.retimeFrames(selected, 0.01)); // integer rounding merges keys
    QVERIFY(!model.retimeFrames(selected, 1.0, -3));
    QVERIFY(!model.retimeFrames(selected, std::numeric_limits<double>::infinity()));
    QVERIFY(channel->keyframeAt(2));
    QVERIFY(channel->keyframeAt(4));
    QVERIFY(model.retimeFrames(selected, 2.0));
    m_image->waitForDone();
    QVERIFY(channel->keyframeAt(2));
    QVERIFY(!channel->keyframeAt(4));
    QVERIFY(channel->keyframeAt(6));
    QVERIFY(channel->keyframeAt(10));
    QVERIFY(secondChannel->keyframeAt(3));
    QVERIFY(secondChannel->keyframeAt(4));
    QVERIFY(model.summaryKeyframeTimes().contains(6));
    m_doc->undoStack()->undo();
    m_image->waitForDone();
    QVERIFY(channel->keyframeAt(4));
    QVERIFY(!channel->keyframeAt(6));
    m_doc->undoStack()->redo();
    m_image->waitForDone();
    QVERIFY(!channel->keyframeAt(4));
    QVERIFY(channel->keyframeAt(6));
    // Summary-style selection moves both layers in one undoable operation,
    // including a destination occupied by another selected source key.
    QModelIndexList bothLayers{model.index(row, 2), model.index(row, 6),
                              model.index(secondRow, 3), model.index(secondRow, 4)};
    QModelIndexList retimed;
    QVERIFY(model.retimeFrames(bothLayers, 1.0, 1, &retimed));
    m_image->waitForDone();
    QCOMPARE(retimed.size(), 4);
    QVERIFY(channel->keyframeAt(3));
    QVERIFY(channel->keyframeAt(7));
    QVERIFY(secondChannel->keyframeAt(4));
    QVERIFY(secondChannel->keyframeAt(5));
    QVERIFY(!secondChannel->keyframeAt(3));
    m_doc->undoStack()->undo();
    m_image->waitForDone();
    QVERIFY(channel->keyframeAt(6));
    QVERIFY(secondChannel->keyframeAt(3));
    m_layer1->setUserLocked(true);
    QVERIFY(!model.retimeFrames({model.index(row, 6)}, 1.0, 1));
    QVERIFY(channel->keyframeAt(6));
}

void TimelineModelTest::testPreviewRangeSurvivesSelection()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_layer1->enableAnimation();
    m_shapeController->setImage(m_image);
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    KisAnimTimelineFramesView view(nullptr);
    view.setModel(&model);
    auto ai = m_image->animationInterface();
    ai->setDocumentRange(KisTimeSpan::fromTimeToTime(0, 20));
    ai->setPreviewPlaybackRange(KisTimeSpan::fromTimeToTime(3, 7), true);
    QVERIFY(model.rowCount() > 0);
    QItemSelection selection(model.index(0, 2), model.index(0, 10));
    view.selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    QCOMPARE(ai->activePlaybackRange().start(), 3);
    QCOMPARE(ai->activePlaybackRange().end(), 7);
    QCOMPARE(ai->documentPlaybackRange().start(), 0);
    QCOMPARE(ai->documentPlaybackRange().end(), 20);
    QVERIFY(model.headerData(3, Qt::Horizontal, KisTimeBasedItemModel::WithinPlaybackRange).toBool());
    QVERIFY(!model.headerData(8, Qt::Horizontal, KisTimeBasedItemModel::WithinPlaybackRange).toBool());
    QCOMPARE(model.headerData(0, Qt::Horizontal, KisTimeBasedItemModel::PreviewRangeStart).toInt(), 3);
    ai->setPreviewPlaybackRange(ai->previewPlaybackRange(), false);
    QCOMPARE(ai->activePlaybackRange().end(), 20);
    QCOMPARE(ai->previewPlaybackRange().end(), 7);
    QVERIFY(!model.headerData(0, Qt::Horizontal, KisTimeBasedItemModel::PreviewRangeStart).isValid());
    QVERIFY(model.headerData(8, Qt::Horizontal, KisTimeBasedItemModel::WithinPlaybackRange).toBool());
}

void TimelineModelTest::testPreviewRangeRoundTrip()
{
    constructImage();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto ai = m_image->animationInterface();
    ai->setDocumentRange(KisTimeSpan::fromTimeToTime(0, 30));
    for (bool enabled : {true, false}) {
        ai->setPreviewPlaybackRange(KisTimeSpan::fromTimeToTime(5, 13), enabled);
        const QString path = dir.filePath(enabled ? "enabled.kra" : "disabled.kra");
        QVERIFY(m_doc->exportDocumentSync(path, "application/x-krita"));
        QScopedPointer<KisDocument> loaded(KisPart::instance()->createDocument());
        QVERIFY(loaded->loadNativeFormat(path));
        auto saved = loaded->image()->animationInterface();
        QCOMPARE(saved->previewPlaybackRangeEnabled(), enabled);
        QCOMPARE(saved->previewPlaybackRange(), KisTimeSpan::fromTimeToTime(5, 13));
        QCOMPARE(saved->documentPlaybackRange(), KisTimeSpan::fromTimeToTime(0, 30));
        QCOMPARE(saved->activePlaybackRange(), enabled ? KisTimeSpan::fromTimeToTime(5, 13) : KisTimeSpan::fromTimeToTime(0, 30));
    }
    // Even preview == clip must retain its explicit enabled flag in a clone.
    ai->setPreviewPlaybackRange(ai->documentPlaybackRange(), true);
    KisImageSP clone = m_image->clone();
    QVERIFY(clone->animationInterface()->previewPlaybackRangeEnabled());
    QCOMPARE(clone->animationInterface()->previewPlaybackRange(), ai->documentPlaybackRange());
}

void TimelineModelTest::testModalTransforms()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_layer1->enableAnimation();
    auto channel = m_layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    channel->addKeyframe(6);
    channel->addKeyframe(8);
    channel->addKeyframe(16); // unselected collision target
    m_shapeController->setImage(m_image);
    m_image->animationInterface()->switchCurrentTimeAsync(4);
    m_image->waitForDone();
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    int row = -1;
    for (int r = 0; r < model.rowCount(); ++r) if (model.nodeAt(model.index(r, 0)).data() == m_layer1.data()) row = r;
    QVERIFY(row >= 0);
    QWidget body; // Never show a window or inject system input in these tests.
    QVBoxLayout layout(&body);
    KisAnimTimelineFramesView view(&body);
    layout.addWidget(&view);
    view.setModel(&model);
    QItemSelection selection;
    selection.select(model.index(row, 6), model.index(row, 6));
    selection.select(model.index(row, 8), model.index(row, 8));
    view.selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    KisAnimTimelineTransform tool(&view, &model, &body);
    auto key = [&tool](int k, const QString &text = QString(), Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QKeyEvent event(QEvent::KeyPress, k, modifiers, text);
        QApplication::sendEvent(&tool, &event);
    };
    const int undoCount = m_doc->undoStack()->count();
    QVERIFY(tool.begin(false, QPoint(100, 100)));
    tool.moveTo(QPoint(100 + 2 * view.columnWidth(0), 100), Qt::NoModifier);
    QCOMPARE(tool.offset(), 2);
    QVERIFY(tool.isValidTransform());
    QVERIFY(channel->keyframeAt(6)); // live preview does not mutate keys
    QCOMPARE(m_doc->undoStack()->count(), undoCount);
    key(Qt::Key_Escape);
    QVERIFY(!tool.isTransforming());
    QVERIFY(channel->keyframeAt(6));
    QCOMPARE(m_doc->undoStack()->count(), undoCount);

    QVERIFY(tool.begin(true, QPoint(100, 100)));
    key(Qt::Key_2, "2");
    QCOMPARE(tool.factor(), 2.0);
    key(Qt::Key_Return);
    m_image->waitForDone();
    QVERIFY(!channel->keyframeAt(6));
    QVERIFY(channel->keyframeAt(8));
    QVERIFY(channel->keyframeAt(12)); // scaled about playhead 4, not first key 6
    QCOMPARE(view.selectionModel()->selectedIndexes().size(), 2);
    QCOMPARE(m_doc->undoStack()->count(), undoCount + 1);
    m_doc->undoStack()->undo();
    m_image->waitForDone();
    QVERIFY(channel->keyframeAt(6));
    QVERIFY(channel->keyframeAt(8));
    view.selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    QVERIFY(tool.begin(false, QPoint(0, 0)));
    tool.moveTo(QPoint(view.columnWidth(0) * 10, 0), Qt::ShiftModifier);
    QCOMPARE(tool.offset(), 1); // precision at 1/10 mouse speed
    tool.moveTo(QPoint(view.columnWidth(0) * 12, 0), Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(tool.offset(), 1); // combined modifiers snap to a frame
    tool.moveTo(QPoint(view.columnWidth(0) * 24, 0), Qt::ControlModifier);
    QCOMPARE(tool.offset(), 24); // Ctrl snaps to a second at 24fps
    key(Qt::Key_Backspace);
    key(Qt::Key_1, "1");
    key(Qt::Key_0, "0");
    QVERIFY(!tool.isValidTransform()); // 6 -> occupied 16
    key(Qt::Key_Return);
    QVERIFY(tool.isTransforming()); // invalid confirmation keeps editing
    key(Qt::Key_Backspace);
    QVERIFY(tool.isValidTransform());
    QCOMPARE(tool.offset(), 1);
    key(Qt::Key_Escape);
    // Explicit-pivot mirroring is valid, but zero scale merges keys.
    QVERIFY(model.retimeFrames(selection.indexes(), -1, 0, nullptr, 7, true));
    QVERIFY(!model.retimeFrames(selection.indexes(), 0, 0, nullptr, 7, true));
    const auto key6 = channel->keyframeAt(6);
    const auto key8 = channel->keyframeAt(8);
    QVERIFY(model.retimeFrames(selection.indexes(), -1, 0, nullptr, 7));
    m_image->waitForDone();
    QCOMPARE(channel->keyframeAt(6).data(), key8.data());
    QCOMPARE(channel->keyframeAt(8).data(), key6.data());
    m_doc->undoStack()->undo();
    m_image->waitForDone();
    QCOMPARE(channel->keyframeAt(6).data(), key6.data());
    view.selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    QVERIFY(tool.begin(false, QPoint(0, 0), true));
    tool.moveTo(QPoint(view.columnWidth(0), 0), Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(), QPointF(view.columnWidth(0), 0), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&tool, &release);
    QVERIFY(!tool.isTransforming());
    m_image->waitForDone();
    QVERIFY(channel->keyframeAt(7));
    QVERIFY(channel->keyframeAt(9));
    m_doc->undoStack()->undo();
    m_image->waitForDone();
    QVERIFY(channel->keyframeAt(6));
    QVERIFY(channel->keyframeAt(8));
}

void TimelineModelTest::testSummaryInteraction()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_layer1->enableAnimation();
    auto channel = m_layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    channel->addKeyframe(6);
    channel->addKeyframe(7);
    channel->addKeyframe(8);
    m_shapeController->setImage(m_image);
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    QWidget body;
    KisAnimTimelineFramesView view(&body);
    view.setModel(&model);
    view.resize(1000, 200);
    view.doItemsLayout();
    KisAnimTimelineSummary summary(&view, &model, &body);
    auto click = [&](int time, Qt::KeyboardModifiers modifiers) {
        const int left = summary.mapFromGlobal(view.viewport()->mapToGlobal(QPoint())).x();
        const QPoint pos(left + view.columnViewportPosition(time) + view.columnWidth(time) / 2, 12);
        QMouseEvent event(QEvent::MouseButtonPress, pos, summary.mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton, modifiers);
        QApplication::sendEvent(&summary, &event);
    };
    click(6, Qt::NoModifier);
    QCOMPARE(view.selectionModel()->selectedIndexes().size(), 1);
    click(8, Qt::ShiftModifier);
    QCOMPARE(view.selectionModel()->selectedIndexes().size(), 3);
    QVERIFY(view.selectionModel()->isSelected(model.index(view.selectionModel()->selectedIndexes().first().row(), 7)));
    click(6, Qt::ShiftModifier);
    QCOMPARE(view.selectionModel()->selectedIndexes().size(), 1);
    QCOMPARE(view.selectionModel()->selectedIndexes().first().column(), 6);
    click(8, Qt::ControlModifier);
    click(6, Qt::ControlModifier);
    QCOMPARE(view.selectionModel()->selectedIndexes().size(), 1);
    QCOMPARE(view.selectionModel()->selectedIndexes().first().column(), 8);
    bool dragStarted = false;
    summary.startTransform = [&](bool scale, QPoint, QPoint, bool drag, Qt::KeyboardModifiers) { dragStarted = drag && !scale; };
    click(8, Qt::NoModifier);
    const QPoint move(view.columnViewportPosition(10) + view.columnWidth(0) + view.verticalHeader()->width(), 12);
    QMouseEvent event(QEvent::MouseMove, move, summary.mapToGlobal(move), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&summary, &event);
    QVERIFY(dragStarted);
}

void TimelineModelTest::testTrackClickActivatesLayer()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_layer2->setPinnedToTimeline(true);
    m_shapeController->setImage(m_image);
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    QWidget body;
    KisAnimTimelineFramesView view(&body);
    view.setModel(&model);
    view.resize(1000, 200);
    view.doItemsLayout();

    model.slotCurrentNodeChanged(m_layer1);
    int targetRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.nodeAt(model.index(row, 8)).data() == m_layer2.data()) {
            targetRow = row;
            break;
        }
    }
    QVERIFY(targetRow >= 0);
    const auto cell = model.index(targetRow, 8);
    QVERIFY(!cell.data(KisTimeBasedItemModel::FrameExistsRole).toBool());
    QVERIFY(!cell.data(KisAnimTimelineFramesModel::ActiveLayerRole).toBool());

    KisNodeSP activated;
    connect(&model, &KisAnimTimelineFramesModel::requestCurrentNodeChanged,
            this, [&](KisNodeSP node) { activated = node; });
    const QPoint pos = view.visualRect(cell).center();
    QCOMPARE(view.indexAt(pos), cell);
    QMouseEvent press(QEvent::MouseButtonPress, pos, view.viewport()->mapToGlobal(pos),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &press);
    QCOMPARE(activated, KisNodeSP(m_layer2));
    QVERIFY(cell.data(KisAnimTimelineFramesModel::ActiveLayerRole).toBool());
}

void TimelineModelTest::testRangeFieldShortcut()
{
    KisIntParseSpinBox field;
    field.setRange(0, 9999);
    field.setValue(3);
    int playhead = 42;
    new KisAnimTimelineRangeField(&field, [&]() { field.setValue(playhead); });
    auto editor = field.findChild<QLineEdit *>();
    QVERIFY(editor);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QApplication::sendEvent(editor, &press);
    QCOMPARE(field.value(), 42);
    playhead = 64;
    QMouseEvent arrowPress(QEvent::MouseButtonPress, QPointF(field.width()-2, 2), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QApplication::sendEvent(&field, &arrowPress);
    QCOMPARE(field.value(), 64);
}

void TimelineModelTest::testTimelineZoom()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_shapeController->setImage(m_image);
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    QWidget body;
    KisAnimTimelineFramesView view(&body);
    view.setModel(&model);
    view.resize(1000, 220);
    view.doItemsLayout();
    view.setTimelineZoom(1, 0);
    view.setCurrentIndex(model.index(0, 10));
    const auto selected = view.currentIndex();
    const int time = model.currentTime();
    const int width = view.columnWidth(0);
    const QPoint pos(200, 20);
    QWheelEvent wheel(pos, view.viewport()->mapToGlobal(pos), QPoint(), QPoint(0,120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(view.viewport(), &wheel);
    QVERIFY(view.columnWidth(0) > width);
    QCOMPARE(view.currentIndex(), selected);
    QCOMPARE(model.currentTime(), time);
    view.setTimelineZoom(4, 0);
    QVERIFY(view.fitTimeline(99));
    QCOMPARE(view.horizontalScrollBar()->value(), 0);
    QVERIFY(view.columnViewportPosition(99) + view.columnWidth(99) <= view.viewport()->width());
}

void TimelineModelTest::testPenZoomGesture()
{
    constructImage();
    m_layer1->setPinnedToTimeline(true);
    m_layer1->enableAnimation();
    auto channel = m_layer1->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true);
    channel->addKeyframe(6);
    channel->addKeyframe(8);
    m_shapeController->setImage(m_image);
    KisAnimTimelineFramesModel model(nullptr);
    model.setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    QWidget body;
    KisAnimTimelineFramesView view(&body);
    view.setModel(&model);
    view.resize(1000, 220);
    view.doItemsLayout();
    KisAnimTimelineSummary summary(&view, &model, &body);
    summary.resize(1000, 24);
    int row = -1;
    for (int candidate = 0; candidate < model.rowCount(); ++candidate) {
        if (model.nodeAt(model.index(candidate, 6)).data() == m_layer1.data()) row = candidate;
    }
    QVERIFY(row >= 0);
    view.setCurrentIndex(model.index(row, 6));
    const auto selected = view.selectionModel()->selectedIndexes();
    const int time = model.currentTime();
    const auto key = channel->keyframeAt(8);
    const int undoIndex = m_doc->undoStack()->index();

    const QList<QWidget *> surfaces{view.viewport(), &summary, view.horizontalHeader()->viewport()};
    for (auto surface : surfaces) {
        view.setTimelineZoom(1, 0);
        view.horizontalScrollBar()->setValue(100);
        const QPoint position(300, 12);
        const auto global = surface->mapToGlobal(position);
        const int anchor = view.viewport()->mapFromGlobal(global).x();
        const qreal frame = qreal(view.horizontalScrollBar()->value() + anchor) / view.columnWidth(0);
        QKeyEvent space(QEvent::ShortcutOverride, Qt::Key_Space, Qt::ControlModifier);
        QApplication::sendEvent(&view, &space);
        QMouseEvent press(QEvent::MouseButtonPress, position, global,
                          Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
        QApplication::sendEvent(surface, &press);
        QVERIFY(view.isTimelineZoomActive());
        const QPoint up = position - QPoint(0,100);
        QMouseEvent move(QEvent::MouseMove, up, global - QPoint(0,100),
                         Qt::NoButton, Qt::LeftButton, Qt::ControlModifier);
        QApplication::sendEvent(surface, &move);
        QVERIFY(view.columnWidth(0) > 18);
        QMouseEvent barrelPress(QEvent::MouseButtonPress, up, global-QPoint(0,100),
                                Qt::RightButton, Qt::LeftButton | Qt::RightButton, Qt::ControlModifier);
        QApplication::sendEvent(surface, &barrelPress);
        QMouseEvent barrelRelease(QEvent::MouseButtonRelease, up, global-QPoint(0,100),
                                  Qt::RightButton, Qt::LeftButton, Qt::ControlModifier);
        QApplication::sendEvent(surface, &barrelRelease);
        QVERIFY(view.isTimelineZoomActive());
        const qreal anchored = qreal(view.horizontalScrollBar()->value() + anchor) / view.columnWidth(0);
        QVERIFY(qAbs(anchored-frame) <= qreal(1)/view.columnWidth(0));
        // Releasing keyboard modifiers before the pen must still end the gesture cleanly.
        QKeyEvent releaseSpace(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(&view, &releaseSpace);
        QMouseEvent moveBack(QEvent::MouseMove, position, global,
                             Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(surface, &moveBack);
        QCOMPARE(view.columnWidth(0), 18);
        QMouseEvent release(QEvent::MouseButtonRelease, position, global,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(surface, &release);
        QVERIFY(!view.isTimelineZoomActive());
        QCOMPARE(view.selectionModel()->selectedIndexes(), selected);
        QCOMPARE(model.currentTime(), time);
        QCOMPARE(channel->keyframeAt(8), key);
        QCOMPARE(m_doc->undoStack()->index(), undoIndex);
        QVERIFY(surface->cursor().shape() != Qt::SizeVerCursor);
    }

    QKeyEvent space(QEvent::ShortcutOverride, Qt::Key_Space, Qt::ControlModifier);
    QApplication::sendEvent(&view, &space);
    QMouseEvent press(QEvent::MouseButtonPress, QPoint(200,20), view.viewport()->mapToGlobal(QPoint(200,20)),
                      Qt::LeftButton, Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(view.viewport(), &press);
    QVERIFY(view.isTimelineZoomActive());
    QFocusEvent lostFocus(QEvent::FocusOut);
    QApplication::sendEvent(&view, &lostFocus);
    QVERIFY(!view.isTimelineZoomActive());
}

struct TestingInterface : KisAnimTimelineFramesModel::NodeManipulationInterface
{
    TestingInterface(KisImageSP image) : m_image(image) {}

    KisLayerSP addPaintLayer() const override {
        KisNodeSP parent = m_image->root();
        KisNodeSP after = parent->lastChild();


        KisPaintLayerSP layer =
            new KisPaintLayer(const_cast<KisImage*>(m_image.data()),
                              m_image->nextLayerName(),
                              OPACITY_OPAQUE_U8,
                              m_image->colorSpace());

        m_image->undoAdapter()->addCommand(
            new KisImageLayerAddCommand(m_image, layer,
                                        parent,
                                        after,
                                        false, false));

        return layer;
    }

    void removeNode(KisNodeSP node) const override {
        m_image->undoAdapter()->addCommand(
            new KisImageLayerRemoveCommand(m_image, node));
    }

    bool setNodeProperties(KisNodeSP, KisImageSP, KisBaseNode::PropertyList) const override
    {
        return false;
    }

private:
    KisImageSP m_image;
};

void TimelineModelTest::testView()
{
#ifndef ENABLE_GUI_TESTS
    return;
#endif

    QDialog dlg;

    QFont font;
    font.setPointSizeF(9);
    dlg.setFont(font);

    QSpinBox *intFps = new KisIntParseSpinBox(&dlg);
    intFps->setValue(12);

    QSpinBox *intTime = new KisIntParseSpinBox(&dlg);
    intTime->setValue(0);
    intTime->setMaximum(10000);

    QSpinBox *intLayer = new KisIntParseSpinBox(&dlg);
    intLayer->setValue(0);
    intLayer->setMaximum(100);

    KisAnimTimelineFramesView *framesTable = new KisAnimTimelineFramesView(&dlg);

    KisAnimTimelineFramesModel *model = new KisAnimTimelineFramesModel(&dlg);

    constructImage();
    addSelectionMasks();
    m_shapeController->setImage(m_image);

    m_image->animationInterface()->requestTimeSwitchWithUndo(4);

    framesTable->setModel(model);

    model->setDummiesFacade(m_shapeController, m_image, m_displayModeAdapter);
    model->setNodeManipulationInterface(new TestingInterface(m_image));

    m_layer1->enableAnimation();
    m_layer1->setPinnedToTimeline(true);

    connect(intFps, SIGNAL(valueChanged(int)),
            m_image->animationInterface(), SLOT(setFramerate(int)));

    connect(intTime, SIGNAL(valueChanged(int)),
            SLOT(setCurrentTime(int)));

    connect(m_image->animationInterface(), SIGNAL(sigUiTimeChanged(int)),
            intTime, SLOT(setValue(int)));

    connect(intLayer, SIGNAL(valueChanged(int)),
            SLOT(setCurrentLayer(int)));

    connect(this, SIGNAL(sigRequestNodeChange(KisNodeSP)),
            model, SLOT(slotCurrentNodeChanged(KisNodeSP)));

    connect(model, SIGNAL(requestCurrentNodeChanged(KisNodeSP)),
            this, SLOT(slotGuiChangedNode(KisNodeSP)));

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    layout->addWidget(intFps);
    layout->addWidget(intTime);
    layout->addWidget(intLayer);
    layout->addWidget(framesTable);

    layout->setStretch(0, 0);
    layout->setStretch(1, 0);
    layout->setStretch(2, 0);
    layout->setStretch(3, 1);

    dlg.resize(600, 400);

    dlg.exec();
}

void TimelineModelTest::setCurrentTime(int time)
{
    m_image->animationInterface()->requestTimeSwitchWithUndo(time);
}

KisNodeDummy* findNodeFromRowAny(KisNodeDummy *root, int &startCount)
{
    if (!startCount) {
        return root;
    }
    startCount--;

    KisNodeDummy *dummy = root->lastChild();
    while (dummy) {
        KisNodeDummy *found = findNodeFromRowAny(dummy, startCount);
        if (found) return found;

        dummy = dummy->prevSibling();
    }

    return 0;
}


void TimelineModelTest::setCurrentLayer(int row)
{
    KisNodeDummy *root = m_shapeController->rootDummy();
    KisNodeDummy *dummy = findNodeFromRowAny(root, row);
    if (!dummy) {
        qDebug() << "WARNING: Cannot find a node at pos" << row;
        return;
    } else {
        qDebug() << "NonGUI changed active node: " << dummy->node()->name();
    }

    Q_EMIT sigRequestNodeChange(dummy->node());
}

void TimelineModelTest::slotGuiChangedNode(KisNodeSP node)
{
    qDebug() << "GUI changed active node:" << node->name();
}

#include "kis_equalizer_column.h"
#include "kis_equalizer_slider.h"
#include "kis_equalizer_widget.h"

void TimelineModelTest::testOnionSkins()
{
#ifndef ENABLE_GUI_TESTS
    return;
#endif
    QDialog dlg;

    QFont font;
    font.setPointSizeF(9);
    dlg.setFont(font);

    QHBoxLayout *layout = new QHBoxLayout(&dlg);

    KisEqualizerWidget *w = new KisEqualizerWidget(10, &dlg);
    connect(w, SIGNAL(sigConfigChanged()), SLOT(slotBang()));

    layout->addWidget(w);

    dlg.resize(600, 400);
    dlg.exec();

}

void TimelineModelTest::slotBang()
{
    ENTER_FUNCTION() << "!!!!";
}

KISTEST_MAIN(TimelineModelTest)
