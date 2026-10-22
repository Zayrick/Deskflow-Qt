/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QWidget>

#include <optional>

class ComputerList;
class QColor;
class QContextMenuEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;

/**
 * @brief Free form editor for the server's computer layout.
 *
 * Every computer is drawn to scale on a shared canvas, like a display arrangement. Computers are moved by dragging
 * and scaled proportionally with the handle at their bottom right corner. While dragging, edges snap to the edges and
 * centres of the other computers and the matching guide lines are shown. Edges that are shared between two computers
 * are highlighted, those are the edges the cursor can cross.
 */
class ComputerLayoutView : public QWidget
{
  Q_OBJECT

public:
  explicit ComputerLayoutView(QWidget *parent = nullptr);

  void setComputers(ComputerList *computers);

  /// Mime type used to drag computers in (new computers) and out (to the trash) of the view.
  static QString mimeType();

  /// Geometry of each display of this computer, in the desktop coordinates reported by Qt.
  static QList<QRect> localScreens();
  /// Size of the union of this computer's displays.
  static QSize localDesktopSize();

Q_SIGNALS:
  void computersChanged();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  enum class DragMode
  {
    None,
    Move,
    Resize
  };

  /// Maps canvas coordinates onto the widget.
  struct Transform
  {
    double scale = 1.0;
    QPointF offset;

    [[nodiscard]] QPointF toWidget(const QPointF &p) const
    {
      return p * scale + offset;
    }
    [[nodiscard]] QRectF toWidget(const QRectF &r) const
    {
      return QRectF(toWidget(r.topLeft()), r.size() * scale);
    }
    [[nodiscard]] QPointF toCanvas(const QPointF &p) const
    {
      return (p - offset) / scale;
    }
  };

  [[nodiscard]] Transform fitTransform() const;
  [[nodiscard]] Transform transform() const;
  [[nodiscard]] qsizetype computerAt(const QPointF &widgetPos) const;
  [[nodiscard]] QRectF handleRect(qsizetype index) const;
  [[nodiscard]] bool isRemovable(qsizetype index) const;

  [[nodiscard]] QList<double> snapTargets(qsizetype ignoreIndex, bool horizontal) const;
  /// @return top left corner for @p rect after snapping its edges and centre to the other computers
  [[nodiscard]] QPoint snapMove(const QRectF &rect, qsizetype ignoreIndex) const;
  void collectGuides(const QRectF &rect, qsizetype ignoreIndex);
  void updateDropPreview(const QDropEvent *event);
  void moveSelected(const QPointF &widgetPos);
  void resizeSelected(const QPointF &widgetPos);
  void finishDrag();
  void startRemoveDrag();
  void editComputer(qsizetype index);
  void resetScale(qsizetype index);
  void removeComputer(qsizetype index);

  void drawComputer(QPainter &painter, qsizetype index) const;
  void drawLocalScreens(QPainter &painter, const QRectF &rect, const QColor &fill) const;
  void drawGuides(QPainter &painter) const;

  ComputerList *m_computers = nullptr;
  qsizetype m_selected = -1;

  DragMode m_dragMode = DragMode::None;
  bool m_dragStarted = false;
  QPointF m_pressPos;
  QPoint m_originalPosition;
  double m_originalScale = 1.0;
  /// The view is not refitted while dragging, otherwise the canvas would move under the cursor.
  std::optional<Transform> m_frozenTransform;
  QList<double> m_guidesX;
  QList<double> m_guidesY;

  /// Where a computer dragged in from outside would be dropped, in canvas coordinates.
  std::optional<QRectF> m_dropPreview;

  static constexpr int kPadding = 24;
  static constexpr int kHandleSize = 10;
  static constexpr double kSnapDistance = 8.0;
  static constexpr double kDragThreshold = 3.0;
};
