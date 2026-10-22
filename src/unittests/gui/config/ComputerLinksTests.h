/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ComputerLinksTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void fullEdge();
  void partialEdge();
  void differentSizes();
  void scaledComputer();
  void verticalEdge();
  void touchTolerance();
  void noLinkWhenApart();
  void noLinkAtCorner();
  void severalLinksOnOneEdge();
  void intervalsNeverOverlap();
  void sharedEdgesReportedOnce();
};
