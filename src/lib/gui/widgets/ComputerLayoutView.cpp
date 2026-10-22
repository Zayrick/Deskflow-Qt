/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerLayoutView.h"

#include "dialogs/ComputerSettingsDialog.h"
#include "gui/config/ComputerLinks.h"
#include "gui/config/ComputerList.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

/// Canvas size shown when there is nothing to fit, so a dropped computer has a sensible size.
const QSizeF kEmptyCanvas{1920.0 * 3, 1080.0 * 2};
/// Extra room around the computers, relative to the larger side of their bounding rectangle.
constexpr double kFitMargin = 0.12;
/// Computers are never made smaller than this on screen while resizing, in widget pixels.
constexpr double kMinWidgetSize = 24.0;
/// Edges closer than this (canvas units) count as aligned when collecting guide lines.
constexpr double kGuideTolerance = 0.75;

std::optional<Computer> decodeComputer(const QMimeData *mimeData)
{
  if (mimeData == nullptr || !mimeData->hasFormat(ComputerLayoutView::mimeType()))
    return std::nullopt;

  QByteArray data = mimeData->data(ComputerLayoutView::mimeType());
  QDataStream stream(&data, QIODevice::ReadOnly);
  Computer computer;
  stream >> computer;
  if (stream.status() != QDataStream::Ok)
    return std::nullopt;
  return computer;
}

QColor mix(const QColor &a, const QColor &b, double ratio)
{
  const auto blend = [ratio](int x, int y) { return static_cast<int>(std::lround(x + (y - x) * ratio)); };
  return QColor(blend(a.red(), b.red()), blend(a.green(), b.green()), blend(a.blue(), b.blue()));
}

/// @return the smallest correction (canvas units) that aligns one of @p edges with one of @p targets, if any is within
/// @p maxDistance
std::optional<double> nearestSnap(const QList<double> &edges, const QList<double> &targets, double maxDistance)
{
  std::optional<double> best;
  for (const auto edge : edges) {
    for (const auto target : targets) {
      const double diff = target - edge;
      if (std::abs(diff) <= maxDistance && (!best || std::abs(diff) < std::abs(*best)))
        best = diff;
    }
  }
  return best;
}

} // namespace

ComputerLayoutView::ComputerLayoutView(QWidget *parent) : QWidget(parent)
{
  setAcceptDrops(true);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(320, 200);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ComputerLayoutView::setComputers(ComputerList *computers)
{
  m_computers = computers;
  m_selected = -1;
  m_dragMode = DragMode::None;
  m_frozenTransform.reset();
  m_guidesX.clear();
  m_guidesY.clear();
  update();
}

QString ComputerLayoutView::mimeType()
{
  return QStringLiteral("application/x-deskflow-computer");
}

QList<QRect> ComputerLayoutView::localScreens()
{
  QList<QRect> screens;
  for (const auto *screen : QGuiApplication::screens())
    screens.append(screen->geometry());
  return screens;
}

QSize ComputerLayoutView::localDesktopSize()
{
  QRect desktop;
  for (const auto &screen : localScreens())
    desktop = desktop.united(screen);
  return desktop.isEmpty() ? Computer::kDefaultResolution : desktop.size();
}

ComputerLayoutView::Transform ComputerLayoutView::fitTransform() const
{
  QRectF bounds;
  if (m_computers != nullptr)
    bounds = m_computers->boundingRect();
  if (m_dropPreview)
    bounds = bounds.isEmpty() ? *m_dropPreview : bounds.united(*m_dropPreview);
  if (bounds.isEmpty())
    bounds = QRectF(QPointF(0, 0), kEmptyCanvas);

  const double margin = std::max(bounds.width(), bounds.height()) * kFitMargin;
  bounds.adjust(-margin, -margin, margin, margin);

  const QRectF available = QRectF(rect()).adjusted(kPadding, kPadding, -kPadding, -kPadding);
  Transform t;
  if (available.width() <= 0 || available.height() <= 0)
    return t;

  t.scale = std::min(available.width() / bounds.width(), available.height() / bounds.height());
  t.offset = available.center() - bounds.center() * t.scale;
  return t;
}

ComputerLayoutView::Transform ComputerLayoutView::transform() const
{
  return m_frozenTransform.value_or(fitTransform());
}

qsizetype ComputerLayoutView::computerAt(const QPointF &widgetPos) const
{
  if (m_computers == nullptr)
    return -1;

  const auto t = transform();
  // the selected computer is drawn on top of the others, so it is hit first
  if (m_selected >= 0 && m_selected < m_computers->size() &&
      t.toWidget(m_computers->at(m_selected).rect()).contains(widgetPos))
    return m_selected;

  for (auto i = m_computers->size() - 1; i >= 0; --i) {
    if (t.toWidget(m_computers->at(i).rect()).contains(widgetPos))
      return i;
  }
  return -1;
}

QRectF ComputerLayoutView::handleRect(qsizetype index) const
{
  if (m_computers == nullptr || index < 0 || index >= m_computers->size())
    return {};

  const auto r = transform().toWidget(m_computers->at(index).rect());
  return QRectF(r.right() - kHandleSize, r.bottom() - kHandleSize, kHandleSize, kHandleSize);
}

bool ComputerLayoutView::isRemovable(qsizetype index) const
{
  return m_computers != nullptr && index >= 0 && index < m_computers->size() && !m_computers->at(index).isServer();
}

QList<double> ComputerLayoutView::snapTargets(qsizetype ignoreIndex, bool horizontal) const
{
  QList<double> targets;
  if (m_computers == nullptr)
    return targets;

  for (qsizetype i = 0; i < m_computers->size(); ++i) {
    if (i == ignoreIndex)
      continue;
    const auto r = m_computers->at(i).rect();
    if (horizontal)
      targets << r.left() << r.center().x() << r.right();
    else
      targets << r.top() << r.center().y() << r.bottom();
  }
  return targets;
}

QPoint ComputerLayoutView::snapMove(const QRectF &rect, qsizetype ignoreIndex) const
{
  const double maxDistance = kSnapDistance / transform().scale;
  QPointF topLeft = rect.topLeft();

  const QList<double> edgesX{rect.left(), rect.center().x(), rect.right()};
  if (const auto dx = nearestSnap(edgesX, snapTargets(ignoreIndex, true), maxDistance))
    topLeft.rx() += *dx;

  const QList<double> edgesY{rect.top(), rect.center().y(), rect.bottom()};
  if (const auto dy = nearestSnap(edgesY, snapTargets(ignoreIndex, false), maxDistance))
    topLeft.ry() += *dy;

  return topLeft.toPoint();
}

void ComputerLayoutView::collectGuides(const QRectF &rect, qsizetype ignoreIndex)
{
  m_guidesX.clear();
  m_guidesY.clear();

  const auto collect = [](const QList<double> &edges, const QList<double> &targets, QList<double> &guides) {
    for (const auto target : targets) {
      const bool aligned =
          std::ranges::any_of(edges, [target](double edge) { return std::abs(edge - target) <= kGuideTolerance; });
      if (aligned && !guides.contains(target))
        guides.append(target);
    }
  };

  collect({rect.left(), rect.center().x(), rect.right()}, snapTargets(ignoreIndex, true), m_guidesX);
  collect({rect.top(), rect.center().y(), rect.bottom()}, snapTargets(ignoreIndex, false), m_guidesY);
}

void ComputerLayoutView::moveSelected(const QPointF &widgetPos)
{
  auto &computer = (*m_computers)[m_selected];
  const QPointF delta = (widgetPos - m_pressPos) / transform().scale;
  const QRectF proposed(QPointF(m_originalPosition) + delta, computer.rect().size());

  computer.setPosition(snapMove(proposed, m_selected));
  collectGuides(computer.rect(), m_selected);
}

void ComputerLayoutView::resizeSelected(const QPointF &widgetPos)
{
  auto &computer = (*m_computers)[m_selected];
  const auto t = transform();
  const QSizeF resolution(computer.resolution());
  const QPointF topLeft(computer.position());

  // where the bottom right corner would be without snapping, keeping the offset from where the handle was grabbed
  const QPointF originalCorner = topLeft + QPointF(resolution.width(), resolution.height()) * m_originalScale;
  const QPointF corner = originalCorner + (widgetPos - m_pressPos) / t.scale;

  // project the corner onto the diagonal, so the aspect ratio stays locked
  const QPointF extent = corner - topLeft;
  const double diagonal = resolution.width() * resolution.width() + resolution.height() * resolution.height();
  double scale = (extent.x() * resolution.width() + extent.y() * resolution.height()) / diagonal;

  // snap the right / bottom edges and the centre to the other computers
  const double maxDistance = kSnapDistance / t.scale;
  std::optional<double> snapped;
  double snappedDistance = std::numeric_limits<double>::max();
  const auto trySnap = [&](double edge, double target, double candidateScale) {
    const double distance = std::abs(target - edge);
    if (distance <= maxDistance && distance < snappedDistance && candidateScale > 0) {
      snappedDistance = distance;
      snapped = candidateScale;
    }
  };
  const double width = resolution.width() * scale;
  const double height = resolution.height() * scale;
  for (const auto target : snapTargets(m_selected, true)) {
    trySnap(topLeft.x() + width, target, (target - topLeft.x()) / resolution.width());
    trySnap(topLeft.x() + width / 2, target, 2 * (target - topLeft.x()) / resolution.width());
  }
  for (const auto target : snapTargets(m_selected, false)) {
    trySnap(topLeft.y() + height, target, (target - topLeft.y()) / resolution.height());
    trySnap(topLeft.y() + height / 2, target, 2 * (target - topLeft.y()) / resolution.height());
  }
  if (snapped)
    scale = *snapped;

  const double minScale = kMinWidgetSize / (t.scale * std::min(resolution.width(), resolution.height()));
  computer.setScale(std::max(scale, minScale));
  collectGuides(computer.rect(), m_selected);
}

void ComputerLayoutView::finishDrag()
{
  auto &computer = (*m_computers)[m_selected];
  if (m_computers->overlapsAny(computer.rect(), m_selected) && !m_computers->resolveOverlap(m_selected)) {
    computer.setPosition(m_originalPosition);
    computer.setScale(m_originalScale);
  }

  if (computer.position() != m_originalPosition || !qFuzzyCompare(computer.scale(), m_originalScale))
    Q_EMIT computersChanged();
}

void ComputerLayoutView::startRemoveDrag()
{
  const auto index = m_selected;
  auto &computer = (*m_computers)[index];
  computer.setPosition(m_originalPosition);
  computer.setScale(m_originalScale);

  m_dragMode = DragMode::None;
  m_frozenTransform.reset();
  m_guidesX.clear();
  m_guidesY.clear();
  update();

  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream << computer;

  auto *mimeData = new QMimeData;
  mimeData->setData(mimeType(), data);

  auto *drag = new QDrag(this);
  drag->setMimeData(mimeData);
  if (!computer.pixmap().isNull()) {
    drag->setPixmap(computer.pixmap());
    drag->setHotSpot(QPoint(computer.pixmap().width() / 2, computer.pixmap().height() / 2));
  }

  // drops onto this view are refused, so the list is unchanged while the drag runs
  if (drag->exec(Qt::MoveAction, Qt::MoveAction) == Qt::MoveAction)
    removeComputer(index);
}

void ComputerLayoutView::editComputer(qsizetype index)
{
  if (m_computers == nullptr || index < 0 || index >= m_computers->size())
    return;

  auto &computer = (*m_computers)[index];
  const Computer original = computer;

  ComputerSettingsDialog dialog(this, &computer, m_computers);
  if (dialog.exec() != QDialog::Accepted)
    return;

  // a new size can make the computer overlap its neighbours
  if (m_computers->overlapsAny(computer.rect(), index) && !m_computers->resolveOverlap(index)) {
    computer.setPosition(original.position());
    computer.setResolution(original.resolution());
    computer.setScale(original.scale());
  }

  Q_EMIT computersChanged();
  update();
}

void ComputerLayoutView::resetScale(qsizetype index)
{
  if (m_computers == nullptr || index < 0 || index >= m_computers->size())
    return;

  auto &computer = (*m_computers)[index];
  const double originalScale = computer.scale();
  const QPoint originalPosition = computer.position();

  computer.setScale(1.0);
  if (m_computers->overlapsAny(computer.rect(), index) && !m_computers->resolveOverlap(index)) {
    computer.setScale(originalScale);
    computer.setPosition(originalPosition);
    return;
  }

  Q_EMIT computersChanged();
  update();
}

void ComputerLayoutView::removeComputer(qsizetype index)
{
  if (!isRemovable(index))
    return;

  m_computers->removeAt(index);
  m_selected = -1;
  Q_EMIT computersChanged();
  update();
}

void ComputerLayoutView::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), palette().color(QPalette::Base));

  if (m_computers == nullptr || m_computers->isEmpty()) {
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, tr("Drag a new computer here"));
    if (!m_dropPreview)
      return;
  }

  const auto t = transform();

  if (m_computers != nullptr) {
    for (qsizetype i = 0; i < m_computers->size(); ++i) {
      if (i != m_selected)
        drawComputer(painter, i);
    }

    // edges the cursor can cross, drawn below the selection so it stays readable
    QPen edgePen(QColor(46, 160, 67), 4, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(edgePen);
    for (const auto &edge : deskflow::gui::sharedEdges(*m_computers))
      painter.drawLine(QLineF(t.toWidget(edge.p1()), t.toWidget(edge.p2())));

    if (m_selected >= 0 && m_selected < m_computers->size())
      drawComputer(painter, m_selected);
  }

  if (m_dropPreview) {
    const auto highlight = palette().color(QPalette::Highlight);
    QColor fill = highlight;
    fill.setAlpha(60);
    painter.setPen(QPen(highlight, 1.5, Qt::DashLine));
    painter.setBrush(fill);
    painter.drawRect(t.toWidget(*m_dropPreview));
    painter.setBrush(Qt::NoBrush);
  }

  drawGuides(painter);
}

void ComputerLayoutView::drawComputer(QPainter &painter, qsizetype index) const
{
  const auto &computer = m_computers->at(index);
  const auto r = transform().toWidget(computer.rect());
  const bool selected = index == m_selected;
  const bool overlapping = selected && m_dragMode != DragMode::None && m_computers->overlapsAny(computer.rect(), index);

  const auto base = palette().color(QPalette::Base);
  const auto highlight = palette().color(QPalette::Highlight);
  const QColor fill =
      computer.isServer() ? mix(base, highlight, 0.35) : mix(base, palette().color(QPalette::Button), 0.9);

  painter.save();
  if (computer.isServer())
    drawLocalScreens(painter, r, fill);
  else
    painter.fillRect(r, fill);

  QPen border(palette().color(QPalette::Mid), 1);
  if (overlapping)
    border = QPen(QColor(220, 50, 47), 2);
  else if (selected)
    border = QPen(highlight, 2);
  painter.setPen(border);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));

  // label
  painter.setClipRect(r.adjusted(2, 2, -2, -2));
  auto nameFont = painter.font();
  nameFont.setBold(true);
  const auto detailFont = painter.font();
  const QFontMetricsF nameMetrics(nameFont);
  const QFontMetricsF detailMetrics(detailFont);
  const QString name = nameMetrics.elidedText(computer.name(), Qt::ElideMiddle, r.width() - 8);

  QString details = QStringLiteral("%1 × %2").arg(computer.resolution().width()).arg(computer.resolution().height());
  if (!qFuzzyCompare(computer.scale(), 1.0))
    details += QStringLiteral(" · %1%").arg(qRound(computer.scale() * 100));
  details = detailMetrics.elidedText(details, Qt::ElideRight, r.width() - 8);

  const double lineHeight = nameMetrics.height();
  const bool showDetails = r.height() > lineHeight * 2.5;
  const double textHeight = showDetails ? lineHeight + detailMetrics.height() : lineHeight;
  QRectF textRect(r.left(), r.center().y() - textHeight / 2, r.width(), lineHeight);
  painter.setPen(palette().color(QPalette::Text));
  painter.setFont(nameFont);
  painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, name);
  if (showDetails) {
    painter.setFont(detailFont);
    painter.setPen(palette().color(QPalette::PlaceholderText));
    textRect.translate(0, lineHeight);
    textRect.setHeight(detailMetrics.height());
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, details);
  }
  painter.setClipping(false);

  if (selected) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(highlight);
    painter.drawRect(handleRect(index));
  }
  painter.restore();
}

void ComputerLayoutView::drawLocalScreens(QPainter &painter, const QRectF &rect, const QColor &fill) const
{
  const auto screens = localScreens();
  QRect desktop;
  for (const auto &screen : screens)
    desktop = desktop.united(screen);

  if (screens.size() <= 1 || desktop.isEmpty()) {
    painter.fillRect(rect, fill);
    return;
  }

  // areas of the union not covered by any display can't be reached by the cursor
  painter.fillRect(rect, palette().color(QPalette::Base));
  painter.fillRect(rect, QBrush(palette().color(QPalette::Mid), Qt::BDiagPattern));

  const double sx = rect.width() / desktop.width();
  const double sy = rect.height() / desktop.height();
  painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DotLine));
  painter.setBrush(fill);
  for (const auto &screen : screens) {
    const QRectF display(
        rect.left() + (screen.x() - desktop.x()) * sx, rect.top() + (screen.y() - desktop.y()) * sy,
        screen.width() * sx, screen.height() * sy
    );
    painter.drawRect(display);
  }
  painter.setBrush(Qt::NoBrush);
}

void ComputerLayoutView::drawGuides(QPainter &painter) const
{
  if (m_guidesX.isEmpty() && m_guidesY.isEmpty())
    return;

  const auto t = transform();
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(QColor(230, 0, 122), 1, Qt::DashLine));
  for (const auto x : m_guidesX) {
    const double wx = t.toWidget(QPointF(x, 0)).x();
    painter.drawLine(QLineF(wx, 0, wx, height()));
  }
  for (const auto y : m_guidesY) {
    const double wy = t.toWidget(QPointF(0, y)).y();
    painter.drawLine(QLineF(0, wy, width(), wy));
  }
  painter.restore();
}

void ComputerLayoutView::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || m_computers == nullptr) {
    QWidget::mousePressEvent(event);
    return;
  }

  setFocus(Qt::MouseFocusReason);
  const auto pos = event->position();

  if (m_selected >= 0 && handleRect(m_selected).contains(pos)) {
    m_dragMode = DragMode::Resize;
  } else {
    m_selected = computerAt(pos);
    m_dragMode = m_selected >= 0 ? DragMode::Move : DragMode::None;
  }

  if (m_dragMode != DragMode::None) {
    const auto &computer = m_computers->at(m_selected);
    m_pressPos = pos;
    m_originalPosition = computer.position();
    m_originalScale = computer.scale();
    m_frozenTransform = fitTransform();
    m_dragStarted = false;
  }
  update();
}

void ComputerLayoutView::mouseMoveEvent(QMouseEvent *event)
{
  const auto pos = event->position();

  if (m_dragMode == DragMode::None) {
    if (m_selected >= 0 && handleRect(m_selected).contains(pos))
      setCursor(Qt::SizeFDiagCursor);
    else if (computerAt(pos) >= 0)
      setCursor(Qt::OpenHandCursor);
    else
      unsetCursor();
    return;
  }

  if (!(event->buttons() & Qt::LeftButton))
    return;

  if (!m_dragStarted) {
    if ((pos - m_pressPos).manhattanLength() < kDragThreshold)
      return;
    m_dragStarted = true;
  }

  if (m_dragMode == DragMode::Move) {
    // leaving the view hands the computer over to a regular drag, so it can be dropped onto the trash
    if (!QRectF(rect()).contains(pos) && isRemovable(m_selected)) {
      startRemoveDrag();
      return;
    }
    setCursor(Qt::ClosedHandCursor);
    moveSelected(pos);
  } else {
    resizeSelected(pos);
  }
  update();
}

void ComputerLayoutView::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || m_dragMode == DragMode::None) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  if (m_dragStarted)
    finishDrag();

  m_dragMode = DragMode::None;
  m_dragStarted = false;
  m_frozenTransform.reset();
  m_guidesX.clear();
  m_guidesY.clear();
  unsetCursor();
  update();
}

void ComputerLayoutView::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseDoubleClickEvent(event);
    return;
  }

  // the press that started the double click already set up a drag, drop it before the dialog takes over
  m_dragMode = DragMode::None;
  m_dragStarted = false;
  m_frozenTransform.reset();

  if (const auto index = computerAt(event->position()); index >= 0) {
    m_selected = index;
    editComputer(index);
  }
}

void ComputerLayoutView::contextMenuEvent(QContextMenuEvent *event)
{
  const auto index = computerAt(event->pos());
  if (index < 0)
    return;

  m_selected = index;
  update();

  QMenu menu(this);
  const auto *settings = menu.addAction(tr("Settings…"));
  auto *reset = menu.addAction(tr("Reset scale"));
  reset->setEnabled(!qFuzzyCompare(m_computers->at(index).scale(), 1.0));
  menu.addSeparator();
  auto *remove = menu.addAction(tr("Remove"));
  remove->setEnabled(isRemovable(index));

  const auto *chosen = menu.exec(event->globalPos());
  if (chosen == settings)
    editComputer(index);
  else if (chosen == reset)
    resetScale(index);
  else if (chosen == remove)
    removeComputer(index);
}

void ComputerLayoutView::keyPressEvent(QKeyEvent *event)
{
  if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && isRemovable(m_selected)) {
    removeComputer(m_selected);
    return;
  }
  QWidget::keyPressEvent(event);
}

void ComputerLayoutView::updateDropPreview(const QDropEvent *event)
{
  const auto computer = decodeComputer(event->mimeData());
  if (!computer) {
    m_dropPreview.reset();
    return;
  }

  const auto t = transform();
  const QSizeF size = computer->rect().size();
  const QPointF centre = t.toCanvas(event->position());
  const QRectF proposed(centre - QPointF(size.width() / 2, size.height() / 2), size);
  const QRectF snapped(snapMove(proposed, -1), size);
  m_dropPreview = snapped;
  collectGuides(snapped, -1);
}

void ComputerLayoutView::dragEnterEvent(QDragEnterEvent *event)
{
  if (m_computers == nullptr || event->source() == this || !event->mimeData()->hasFormat(mimeType())) {
    event->ignore();
    return;
  }

  // keep the canvas still while the new computer is moved around
  m_frozenTransform = fitTransform();
  updateDropPreview(event);
  event->acceptProposedAction();
  update();
}

void ComputerLayoutView::dragMoveEvent(QDragMoveEvent *event)
{
  if (m_computers == nullptr || event->source() == this || !event->mimeData()->hasFormat(mimeType())) {
    event->ignore();
    return;
  }

  updateDropPreview(event);
  event->acceptProposedAction();
  update();
}

void ComputerLayoutView::dragLeaveEvent(QDragLeaveEvent *event)
{
  m_dropPreview.reset();
  m_frozenTransform.reset();
  m_guidesX.clear();
  m_guidesY.clear();
  update();
  event->accept();
}

void ComputerLayoutView::dropEvent(QDropEvent *event)
{
  std::optional<QRectF> preview;
  if (m_computers != nullptr && event->source() != this) {
    updateDropPreview(event);
    preview = m_dropPreview;
  }
  auto computer = decodeComputer(event->mimeData());

  m_dropPreview.reset();
  m_frozenTransform.reset();
  m_guidesX.clear();
  m_guidesY.clear();
  update();

  if (!preview || !computer) {
    event->ignore();
    return;
  }

  ComputerSettingsDialog dialog(this, &*computer, m_computers);
  if (dialog.exec() != QDialog::Accepted) {
    event->ignore();
    return;
  }

  computer->setPosition(preview->topLeft().toPoint());
  m_computers->append(*computer);
  m_selected = m_computers->size() - 1;
  if (m_computers->overlapsAny(m_computers->last().rect(), m_selected) && !m_computers->resolveOverlap(m_selected)) {
    m_computers->removeLast();
    m_selected = m_computers->appendToRight(*computer);
  }

  event->acceptProposedAction();
  Q_EMIT computersChanged();
  update();
}
