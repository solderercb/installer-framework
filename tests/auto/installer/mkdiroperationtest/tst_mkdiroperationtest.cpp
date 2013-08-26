/**************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "init.h"
#include "kdupdaterupdateoperations.h"

#include <QDir>
#include <QObject>
#include <QTest>
#include <QFile>
#include <QTextStream>

using namespace KDUpdater;
using namespace QInstaller;

class tst_mkdiroperationtest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
       QInstaller::init();
       QString path = QDir::current().path() + QDir::toNativeSeparators("/test");
       if (QDir(path).exists()) {
           QFAIL("Remove test folder first!");
       }
    }

    void testMissingArguments()
    {
        MkdirOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments: 0 arguments given, 1 expected."));

    }

    void testCreateDirectory_data()
    {
         QTest::addColumn<QString>("directory");
         QTest::newRow("/test") << "/test";
         QTest::newRow("/test/test") << "/test/test";
         QTest::newRow("/test/test/test") << "/test/test/test";
    }

    void testCreateDirectory()
    {
        QFETCH(QString, directory);
        QString path = QDir::current().path() + QDir::toNativeSeparators(directory);

        QVERIFY2(!QDir(path).exists(), path.toLatin1());
        MkdirOperation op;
        op.setArguments(QStringList() << path);
        op.backup();
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY2(QDir(path).exists(), path.toLatin1());
        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY2(!QDir(path).exists(), path.toLatin1());
    }

    void testCreateDirectory_customFile_data()
    {
         QTest::addColumn<QString>("directory");
         QTest::addColumn<QString>("filename");
         QTest::newRow("/test") << "/test" << "/test/file.txt";
         QTest::newRow("/test/test") << "/test/test" << "/test/file.txt";
         QTest::newRow("/test/test/test") << "/test/test/test" << "/test/test/test/file.txt";
    }

    void testCreateDirectory_customFile()
    {
        QFETCH(QString, directory);
        QFETCH(QString, filename);
        QString path = QDir::current().path() + QDir::toNativeSeparators(directory);
        QString filepath = QDir::current().path() + QDir::toNativeSeparators(filename);

        QVERIFY2(!QDir(path).exists(), path.toLatin1());
        MkdirOperation op;
        op.setArguments(QStringList() << path);
        op.backup();
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY2(QDir(path).exists(), path.toLatin1());
        QFile file(filepath);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);
        out << "This file is generated by QTest\n";
        file.close();
        QVERIFY2(!op.undoOperation(), op.errorString().toLatin1());
        QVERIFY2(file.exists(), filepath.toLatin1());
        QVERIFY2(QDir(filepath).remove(filepath), "Could not remove file");
        QVERIFY2(!file.exists(), filepath.toLatin1());
        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY2(!QDir(path).exists(), path.toLatin1());
    }

    void testCreateDirectory_customFile_force_data()
    {
        testCreateDirectory_customFile_data();
    }

    void testCreateDirectory_customFile_force()
    {
        QFETCH(QString, directory);
        QFETCH(QString, filename);
        QString path = QDir::current().path() + QDir::toNativeSeparators(directory);
        QString filepath = QDir::current().path() + QDir::toNativeSeparators(filename);

        QVERIFY2(!QDir(path).exists(), path.toLatin1());
        MkdirOperation op;
        op.setArguments(QStringList() << path);
        op.setValue("forceremoval",true);
        op.backup();
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());
        QVERIFY2(QDir(path).exists(), path.toLatin1());
        QFile file(filepath);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);
        out << "This file is generated by QTest\n";
        file.close();
        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY2(!file.exists(), path.toLatin1());
    }
};

QTEST_MAIN(tst_mkdiroperationtest)

#include "tst_mkdiroperationtest.moc"
