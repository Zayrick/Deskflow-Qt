/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerLinksTests.h"

#include "gui/config/ComputerLinks.h"
#include "gui/config/ComputerList.h"

using deskflow::gui::computeLinks;
using deskflow::gui::ComputerLink;
using deskflow::gui::sharedEdges;

namespace {

Computer makeComputer(
    const QString &name, QPoint position, QSize resolution = Computer::kDefaultResolution, double scale = 1.0
)
{
  Computer computer(name);
  computer.setPosition(position);
  computer.setResolution(resolution);
  computer.setScale(scale);
  return computer;
}

/// Links of @p source formatted like the server config, in the order they were generated.
QStringList linksOf(const ComputerList &computers, const QString &source)
{
  QStringList result;
  for (const auto &link : computeLinks(computers)) {
    if (link.source == source)
      result.append(link.toString());
  }
  return result;
}

} // namespace

void ComputerLinksTests::fullEdge()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1920, 0}));

  QCOMPARE(linksOf(computers, "a"), QStringList{"right = b"});
  QCOMPARE(linksOf(computers, "b"), QStringList{"left = a"});
}

void ComputerLinksTests::partialEdge()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1920, 540}));

  QCOMPARE(linksOf(computers, "a"), QStringList{"right(50,100) = b(0,50)"});
  QCOMPARE(linksOf(computers, "b"), QStringList{"left(0,50) = a(50,100)"});
}

void ComputerLinksTests::differentSizes()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1920, 0}, QSize(1280, 1024)));

  // 1024 / 1080 = 94.8148...% rounded to two decimals
  QCOMPARE(linksOf(computers, "a"), QStringList{"right(0,94.81) = b"});
  QCOMPARE(linksOf(computers, "b"), QStringList{"left = a(0,94.81)"});
}

void ComputerLinksTests::scaledComputer()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  // 4K at half scale takes the same room on the canvas as a 1080p display
  computers.append(makeComputer("b", {1920, 0}, QSize(3840, 2160), 0.5));
  // a half scale 1080p display below a, covering its left half
  computers.append(makeComputer("c", {0, 1080}, Computer::kDefaultResolution, 0.5));

  QCOMPARE(linksOf(computers, "a"), (QStringList{"right = b", "down(0,50) = c"}));
  QCOMPARE(linksOf(computers, "b"), QStringList{"left = a"});
  QCOMPARE(linksOf(computers, "c"), QStringList{"up = a(0,50)"});
}

void ComputerLinksTests::verticalEdge()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {480, -1080}));

  QCOMPARE(linksOf(computers, "a"), QStringList{"up(25,100) = b(0,75)"});
  QCOMPARE(linksOf(computers, "b"), QStringList{"down(0,75) = a(25,100)"});
}

void ComputerLinksTests::touchTolerance()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1921, 0}));

  QCOMPARE(linksOf(computers, "a"), QStringList{"right = b"});
}

void ComputerLinksTests::noLinkWhenApart()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {2000, 0}));

  QVERIFY(computeLinks(computers).isEmpty());
  QVERIFY(sharedEdges(computers).isEmpty());
}

void ComputerLinksTests::noLinkAtCorner()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1920, 1080}));

  QVERIFY(computeLinks(computers).isEmpty());
}

void ComputerLinksTests::severalLinksOnOneEdge()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1920, 540}, QSize(1920, 540)));
  computers.append(makeComputer("c", {1920, 0}, QSize(1920, 540)));

  // sorted along the edge, independent of the list order
  QCOMPARE(linksOf(computers, "a"), (QStringList{"right(0,50) = c", "right(50,100) = b"}));
}

void ComputerLinksTests::intervalsNeverOverlap()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  // three slices that don't divide the edge evenly, plus one overlapping its neighbour
  computers.append(makeComputer("b", {1920, 0}, QSize(1000, 360)));
  computers.append(makeComputer("c", {1920, 360}, QSize(1000, 360)));
  computers.append(makeComputer("d", {1920, 700}, QSize(1000, 380)));

  QList<ComputerLink> rightOfA;
  for (const auto &link : computeLinks(computers)) {
    if (link.source == "a" && link.side == ComputerLink::Side::Right)
      rightOfA.append(link);
  }

  QCOMPARE(rightOfA.size(), qsizetype(3));
  for (qsizetype i = 0; i < rightOfA.size(); ++i) {
    QVERIFY(rightOfA[i].sourceStart >= 0);
    QVERIFY(rightOfA[i].sourceEnd <= 100);
    QVERIFY(rightOfA[i].sourceStart < rightOfA[i].sourceEnd);
    QVERIFY(rightOfA[i].targetStart < rightOfA[i].targetEnd);
    if (i > 0)
      QVERIFY(rightOfA[i - 1].sourceEnd <= rightOfA[i].sourceStart);
  }
}

void ComputerLinksTests::sharedEdgesReportedOnce()
{
  ComputerList computers;
  computers.append(makeComputer("a", {0, 0}));
  computers.append(makeComputer("b", {1920, 540}));
  computers.append(makeComputer("c", {0, 1080}));

  const auto edges = sharedEdges(computers);
  QCOMPARE(edges.size(), qsizetype(3));
  QVERIFY(edges.contains(QLineF(1920, 540, 1920, 1080)));
  QVERIFY(edges.contains(QLineF(0, 1080, 1920, 1080)));
  QVERIFY(edges.contains(QLineF(1920, 1080, 1920, 1620)));
}

QTEST_MAIN(ComputerLinksTests)
