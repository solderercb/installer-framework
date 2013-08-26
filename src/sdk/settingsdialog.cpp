/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <kdupdaterfiledownloader.h>
#include <kdupdaterfiledownloaderfactory.h>
#include <packagemanagercore.h>
#include <productkeycheck.h>

#include <QtCore/QFile>

#include <QItemSelectionModel>
#include <QMessageBox>
#include <QTreeWidget>

#include <QtXml/QDomDocument>

using namespace QInstaller;


// -- TestRepositoryJob

TestRepository::TestRepository(QObject *parent)
    : KDJob(parent)
    , m_downloader(0)
{
    setTimeout(10000);
    setAutoDelete(false);
    setCapabilities(Cancelable);
}

TestRepository::~TestRepository()
{
    if (m_downloader)
        m_downloader->deleteLater();
}

Repository TestRepository::repository() const
{
    return m_repository;
}

void TestRepository::setRepository(const Repository &repository)
{
    cancel();

    setError(NoError);
    setErrorString(QString());
    m_repository = repository;
}

void TestRepository::doStart()
{
    if (m_downloader)
        m_downloader->deleteLater();

    const QUrl url = m_repository.url();
    if (url.isEmpty()) {
        emitFinishedWithError(InvalidUrl, tr("Empty repository URL."));
        return;
    }

    m_downloader = KDUpdater::FileDownloaderFactory::instance().create(url.scheme(), this);
    if (!m_downloader) {
        emitFinishedWithError(InvalidUrl, tr("URL scheme not supported: %1 (%2).")
            .arg(url.scheme(), url.toString()));
        return;
    }

    QAuthenticator auth;
    auth.setUser(m_repository.username());
    auth.setPassword(m_repository.password());
    m_downloader->setAuthenticator(auth);

    connect(m_downloader, SIGNAL(downloadCompleted()), this, SLOT(downloadCompleted()));
    connect(m_downloader, SIGNAL(downloadAborted(QString)), this, SLOT(downloadAborted(QString)),
        Qt::QueuedConnection);
    connect(m_downloader, SIGNAL(authenticatorChanged(QAuthenticator)), this,
        SLOT(onAuthenticatorChanged(QAuthenticator)));

    m_downloader->setAutoRemoveDownloadedFile(true);
    m_downloader->setUrl(QUrl(url.toString() + QString::fromLatin1("/Updates.xml")));

    m_downloader->download();
}

void TestRepository::doCancel()
{
    if (m_downloader) {
        QString errorString = m_downloader->errorString();
        if (errorString.isEmpty())
            errorString = tr("Got a timeout while testing: '%1'").arg(m_repository.displayname());
        // at the moment the download sends downloadCompleted() if we cancel it, so just
        disconnect(m_downloader, 0, this, 0);
        m_downloader->cancelDownload();
        emitFinishedWithError(KDJob::Canceled, errorString);
    }
}

void TestRepository::downloadCompleted()
{
    QString errorMsg;
    int error = DownloadError;

    if (m_downloader->isDownloaded()) {
        QFile file(m_downloader->downloadedFileName());
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            QString errorMsg;
            if (!doc.setContent(&file, &errorMsg)) {
                error = InvalidUpdatesXml;
                errorMsg = tr("Could not parse Updates.xml! Error: %1.").arg(errorMsg);
            } else {
                error = NoError;
            }
        } else {
            errorMsg = tr("Updates.xml could not be opened for reading!");
        }
    } else {
        errorMsg = tr("Updates.xml could not be found on server!");
    }

    if (error > NoError)
        emitFinishedWithError(error, errorMsg);
    else
        emitFinished();

    m_downloader->deleteLater();
    m_downloader = 0;
}

void TestRepository::downloadAborted(const QString &reason)
{
    emitFinishedWithError(DownloadError, reason);
}

void TestRepository::onAuthenticatorChanged(const QAuthenticator &authenticator)
{
    m_repository.setUsername(authenticator.user());
    m_repository.setPassword(authenticator.password());
}


// -- PasswordDelegate

void PasswordDelegate::showPasswords(bool show)
{
    m_showPasswords = show;
}

void PasswordDelegate::disableEditing(bool disable)
{
    m_disabledEditor = disable;
}

QString PasswordDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    const QString tmp = QStyledItemDelegate::displayText(value, locale);
    if (m_showPasswords)
        return tmp;
    return QString(tmp.length(), QChar(0x25CF));
}

QWidget *PasswordDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &)
    const
{
    if (m_disabledEditor)
        return 0;

    QLineEdit *lineEdit = new QLineEdit(parent);
    lineEdit->setEchoMode(m_showPasswords ? QLineEdit::Normal : QLineEdit::Password);
    return lineEdit;
}


// -- RepositoryItem

RepositoryItem::RepositoryItem(const QString &label)
    : QTreeWidgetItem(QTreeWidgetItem::UserType)
{
    setText(0, label);
    m_repo = QInstaller::Repository(QUrl(), true);
}

RepositoryItem::RepositoryItem(const Repository &repo)
    : QTreeWidgetItem(QTreeWidgetItem::UserType)
    , m_repo(repo)
{
    if (!repo.isDefault())
        setFlags(flags() | Qt::ItemIsEditable);
}

QVariant RepositoryItem::data(int column, int role) const
{
    const QVariant &data = QTreeWidgetItem::data(column, role);

    switch (role) {
        case Qt::UserRole: {
            if (column == 0)
                return m_repo.isDefault();
        }   break;

        case Qt::CheckStateRole: {
            if (column == 1)
                return (m_repo.isEnabled() ? Qt::Checked : Qt::Unchecked);
        }   break;

        case Qt::EditRole:
        case Qt::DisplayRole:{
            switch (column) {
                case 0:
                    return data.toString().isEmpty() ? QLatin1String(" ") : data;
                case 2:
                    return m_repo.username();
                case 3:
                    return m_repo.password();
                case 4:
                    return m_repo.displayname();
                default:
                    break;
            };
        }   break;

        case Qt::ToolTipRole:
            switch (column) {
                case 1:
                    return SettingsDialog::tr("Check this to use repository during fetch.");
                case 2:
                    return SettingsDialog::tr("Add the username to authenticate on the server.");
                case 3:
                    return SettingsDialog::tr("Add the password to authenticate on the server.");
                case 4:
                    return SettingsDialog::tr("The servers URL that contains a valid repository.");
                default:
                    return QVariant();
            }   break;
        break;
    };

    return data;
}

void RepositoryItem::setData(int column, int role, const QVariant &value)
{
    switch (role) {
        case Qt::EditRole: {
            switch (column) {
                case 2:
                    m_repo.setUsername(value.toString());
                    break;
                case 3:
                    m_repo.setPassword(value.toString());
                    break;
                case 4:
                    m_repo.setUrl(QUrl::fromUserInput(value.toString()));
                    break;
                default:
                    break;
            };
        }   break;

        case Qt::CheckStateRole: {
            if (column == 1)
                m_repo.setEnabled(Qt::CheckState(value.toInt()) == Qt::Checked);
        }   break;

        default:
            break;
    }
    QTreeWidgetItem::setData(column, role, value);
}

QSet<Repository> RepositoryItem::repositories() const
{
    QSet<Repository> set;
    for (int i = 0; i < childCount(); ++i) {
        if (QTreeWidgetItem *item = child(i)) {
            if (item->type() == QTreeWidgetItem::UserType) {
                if (RepositoryItem *repoItem = static_cast<RepositoryItem*> (item))
                    set.insert(repoItem->repository());
            }
        }
    }
    return set;
}


// -- SettingsDialog

SettingsDialog::SettingsDialog(PackageManagerCore *core, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::SettingsDialog)
    , m_core(core)
    , m_showPasswords(false)
{
    m_ui->setupUi(this);
    setupRepositoriesTreeWidget();

    const Settings &settings = m_core->settings();
    switch (settings.proxyType()) {
        case Settings::NoProxy:
            m_ui->m_noProxySettings->setChecked(true);
            break;
        case Settings::SystemProxy:
            m_ui->m_systemProxySettings->setChecked(true);
            break;
        case Settings::UserDefinedProxy:
            m_ui->m_manualProxySettings->setChecked(true);
            break;
        default:
            m_ui->m_noProxySettings->setChecked(true);
            Q_ASSERT_X(false, Q_FUNC_INFO, "Unknown proxy type given!");
    }

    const QNetworkProxy &ftpProxy = settings.ftpProxy();
    m_ui->m_ftpProxy->setText(ftpProxy.hostName());
    m_ui->m_ftpProxyPort->setValue(ftpProxy.port());
    m_ui->m_ftpProxyUser->setText(ftpProxy.user());
    m_ui->m_ftpProxyPass->setText(ftpProxy.password());
    m_ui->m_ftpProxyNeedsAuth->setChecked(!ftpProxy.user().isEmpty() | !ftpProxy.password().isEmpty());

    const QNetworkProxy &httpProxy = settings.httpProxy();
    m_ui->m_httpProxy->setText(httpProxy.hostName());
    m_ui->m_httpProxyPort->setValue(httpProxy.port());
    m_ui->m_httpProxyUser->setText(httpProxy.user());
    m_ui->m_httpProxyPass->setText(httpProxy.password());
    m_ui->m_httpProxyNeedsAuth->setChecked(!httpProxy.user().isEmpty() | !httpProxy.password().isEmpty());

    connect(m_ui->m_addRepository, SIGNAL(clicked()), this, SLOT(addRepository()));
    connect(m_ui->m_showPasswords, SIGNAL(clicked()), this, SLOT(updatePasswords()));
    connect(m_ui->m_removeRepository, SIGNAL(clicked()), this, SLOT(removeRepository()));
    connect(m_ui->m_useTmpRepositories, SIGNAL(clicked(bool)), this, SLOT(useTmpRepositoriesOnly(bool)));
    connect(m_ui->m_repositoriesView, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
        this, SLOT(currentRepositoryChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
    connect(m_ui->m_testRepository, SIGNAL(clicked()), this, SLOT(testRepository()));

    useTmpRepositoriesOnly(settings.hasReplacementRepos());
    m_ui->m_useTmpRepositories->setChecked(settings.hasReplacementRepos());
    m_ui->m_useTmpRepositories->setEnabled(settings.hasReplacementRepos());
    m_ui->m_repositoriesView->setCurrentItem(m_rootItems.at(settings.hasReplacementRepos()));

    if (!settings.repositorySettingsPageVisible()) {
        // workaround a inconvenience that the page won't hide inside a QTabWidget
        m_ui->m_repositories->setParent(this);
        m_ui->m_repositories->setVisible(settings.repositorySettingsPageVisible());
    }
}

void SettingsDialog::accept()
{
    bool settingsChanged = false;
    Settings newSettings;
    const Settings &settings = m_core->settings();

    // set possible updated default repositories
    newSettings.setDefaultRepositories((dynamic_cast<RepositoryItem*> (m_rootItems.at(0)))->repositories());
    settingsChanged |= (settings.defaultRepositories() != newSettings.defaultRepositories());

    // set possible new temporary repositories
    newSettings.setTemporaryRepositories((dynamic_cast<RepositoryItem*> (m_rootItems.at(1)))->repositories(),
        m_ui->m_useTmpRepositories->isChecked());
    settingsChanged |= (settings.temporaryRepositories() != newSettings.temporaryRepositories());
    settingsChanged |= (settings.hasReplacementRepos() != newSettings.hasReplacementRepos());

    // set possible new user repositories
    newSettings.setUserRepositories((dynamic_cast<RepositoryItem*> (m_rootItems.at(2)))->repositories());
    settingsChanged |= (settings.userRepositories() != newSettings.userRepositories());

    // update proxy type
    newSettings.setProxyType(Settings::NoProxy);
    if (m_ui->m_systemProxySettings->isChecked())
        newSettings.setProxyType(Settings::SystemProxy);
    else if (m_ui->m_manualProxySettings->isChecked())
        newSettings.setProxyType(Settings::UserDefinedProxy);
    settingsChanged |= settings.proxyType() != newSettings.proxyType();

    if (newSettings.proxyType() == Settings::UserDefinedProxy) {
        // update ftp proxy settings
        newSettings.setFtpProxy(QNetworkProxy(QNetworkProxy::HttpProxy, m_ui->m_ftpProxy->text(),
            m_ui->m_ftpProxyPort->value(), m_ui->m_ftpProxyUser->text(), m_ui->m_ftpProxyPass->text()));
        settingsChanged |= (settings.ftpProxy() != newSettings.ftpProxy());

        // update http proxy settings
        newSettings.setHttpProxy(QNetworkProxy(QNetworkProxy::HttpProxy, m_ui->m_httpProxy->text(),
            m_ui->m_httpProxyPort->value(), m_ui->m_httpProxyUser->text(), m_ui->m_httpProxyPass->text()));
        settingsChanged |= (settings.httpProxy() != newSettings.httpProxy());
    }

    if (settingsChanged)
        emit networkSettingsChanged(newSettings);

    QDialog::accept();
}

// -- private slots

void SettingsDialog::addRepository()
{
    int index = 0;
    QTreeWidgetItem *parent = m_ui->m_repositoriesView->currentItem();
    if (parent && !m_rootItems.contains(parent)) {
        parent = parent->parent();
        index = parent->indexOfChild(m_ui->m_repositoriesView->currentItem());
    }

    if (parent) {
        Repository repository;
        repository.setEnabled(true);
        RepositoryItem *item = new RepositoryItem(repository);
        parent->insertChild(index, item);
        m_ui->m_repositoriesView->editItem(item, 4);
        m_ui->m_repositoriesView->scrollToItem(item);
        m_ui->m_repositoriesView->setCurrentItem(item);

        if (parent == m_rootItems.value(1))
            m_ui->m_useTmpRepositories->setEnabled(parent->childCount() > 0);
    }
}

void SettingsDialog::testRepository()
{
    RepositoryItem *current = dynamic_cast<RepositoryItem*> (m_ui->m_repositoriesView->currentItem());
    if (current && !m_rootItems.contains(current)) {
        m_ui->tabWidget->setEnabled(false);
        m_ui->buttonBox->setEnabled(false);

        m_testRepository.setRepository(current->repository());
        m_testRepository.start();
        m_testRepository.waitForFinished();
        current->setRepository(m_testRepository.repository());

        if (m_testRepository.error() > KDJob::NoError) {
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setWindowModality(Qt::WindowModal);
            msgBox.setDetailedText(m_testRepository.errorString());
            msgBox.setText(tr("There was an error testing this repository."));
            msgBox.setInformativeText(tr("Do you want to disable the tested repository?"));

            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);

            if (msgBox.exec() == QMessageBox::Yes)
                current->setData(1, Qt::CheckStateRole, Qt::Unchecked);
        }

        m_ui->tabWidget->setEnabled(true);
        m_ui->buttonBox->setEnabled(true);
    }
}

void SettingsDialog::updatePasswords()
{
    m_showPasswords = !m_showPasswords;
    m_delegate->showPasswords(m_showPasswords);
    m_ui->m_showPasswords->setText(m_showPasswords ? tr("Hide Passwords") : tr("Show Passwords"));

    // force an tree view update so the delegate has to repaint
    m_ui->m_repositoriesView->viewport()->update();
}

void SettingsDialog::removeRepository()
{
    QTreeWidgetItem *item = m_ui->m_repositoriesView->currentItem();
    if (item && !m_rootItems.contains(item)) {
        QTreeWidgetItem *parent = item->parent();
        if (parent) {
            delete parent->takeChild(parent->indexOfChild(item));
            if (parent == m_rootItems.value(1) && parent->childCount() <= 0) {
                useTmpRepositoriesOnly(false);
                m_ui->m_useTmpRepositories->setChecked(false);
                m_ui->m_useTmpRepositories->setEnabled(false);
            }
        }
    }
}

void SettingsDialog::useTmpRepositoriesOnly(bool use)
{
    m_rootItems.at(0)->setDisabled(use);
    m_rootItems.at(2)->setDisabled(use);
}

void SettingsDialog::currentRepositoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (current) {
        const int index = m_rootItems.at(0)->indexOfChild(current);
        m_ui->m_testRepository->setEnabled(!m_rootItems.contains(current));
        m_ui->m_removeRepository->setEnabled(!current->data(0, Qt::UserRole).toBool());
        m_ui->m_addRepository->setEnabled((current != m_rootItems.at(0)) & (index == -1));
    }
}

// -- private

void SettingsDialog::setupRepositoriesTreeWidget()
{
    QTreeWidget *treeWidget = m_ui->m_repositoriesView;
    treeWidget->header()->setVisible(true);
    treeWidget->setHeaderLabels(QStringList() << QString() << tr("Use") << tr("Username") << tr("Password")
         << tr("Repository"));
    m_rootItems.append(new RepositoryItem(tr("Default repositories")));
    m_rootItems.append(new RepositoryItem(tr("Temporary repositories")));
    m_rootItems.append(new RepositoryItem(tr("User defined repositories")));
    treeWidget->addTopLevelItems(m_rootItems);

    const Settings &settings = m_core->settings();
    insertRepositories(settings.userRepositories(), m_rootItems.at(2));
    insertRepositories(settings.defaultRepositories(), m_rootItems.at(0));
    insertRepositories(settings.temporaryRepositories(), m_rootItems.at(1));

    treeWidget->expandAll();
    for (int i = 0; i < treeWidget->model()->columnCount(); ++i)
        treeWidget->resizeColumnToContents(i);

#if QT_VERSION < 0x050000
    treeWidget->header()->setResizeMode(0, QHeaderView::Fixed);
    treeWidget->header()->setResizeMode(1, QHeaderView::Fixed);
#else
    treeWidget->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    treeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
#endif

    treeWidget->header()->setMinimumSectionSize(treeWidget->columnWidth(1));
    treeWidget->setItemDelegateForColumn(0, new PasswordDelegate(treeWidget));
    treeWidget->setItemDelegateForColumn(1, new PasswordDelegate(treeWidget));
    treeWidget->setItemDelegateForColumn(3, m_delegate = new PasswordDelegate(treeWidget));
    m_delegate->showPasswords(false);
    m_delegate->disableEditing(false);
}

void SettingsDialog::insertRepositories(const QSet<Repository> repos, QTreeWidgetItem *rootItem)
{
    rootItem->setFirstColumnSpanned(true);
    foreach (const Repository &repo, repos) {
        RepositoryItem *item = new RepositoryItem(repo);
        rootItem->addChild(item);
        item->setHidden(!ProductKeyCheck::instance(m_core)->isValidRepository(repo));
    }
}
