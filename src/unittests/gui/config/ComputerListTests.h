/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ComputerListTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void appendToRight();
  void touchingIsNotOverlapping();
  void resolveOverlapPushesOut();
  void boundingRect();
};
