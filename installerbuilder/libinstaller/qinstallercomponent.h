/**************************************************************************
**
* *This file is part of Qt SDK**
**
* *Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
* *Contact:  Nokia Corporation qt-info@nokia.com**
**
* *GNU Lesser General Public License Usage
**
* *This file may be used under the terms of the GNU Lesser General Public
* *License version 2.1 as published by the Free Software Foundation and
* *appearing in the file LICENSE.LGPL included in the packaging of this file.
* *Please review the following information to ensure the GNU Lesser General
* *Public License version 2.1 requirements will be met:
* *http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
* *In addition, as a special exception, Nokia gives you certain additional
* *rights. These rights are described in the Nokia Qt LGPL Exception version
* *1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
* *If you are unsure which license is appropriate for your use, please contact
* *(qt-info@nokia.com).
**
**************************************************************************/
#ifndef QINSTALLER_COMPONENT_H
#define QINSTALLER_COMPONENT_H

#include "installer_global.h"

#include <QtCore/QDir>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <QtScript/QScriptable>
#include <QtScript/QScriptValueList>

namespace KDUpdater {
    class Update;
    class UpdateOperation;
    struct PackageInfo;
}

namespace QInstaller {
class Installer;
class InstallerPrivate;

class INSTALLER_EXPORT Component : public QObject, public QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString displayName READ displayName)
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected)
    Q_PROPERTY(bool autoCreateOperations READ autoCreateOperations WRITE setAutoCreateOperations)
    Q_PROPERTY(QStringList archives READ archives)
    Q_PROPERTY(QStringList userInterfaces READ userInterfaces)
    Q_PROPERTY(QStringList dependencies READ dependencies)
    Q_PROPERTY(bool fromOnlineRepository READ isFromOnlineRepository)
    Q_PROPERTY(QUrl repositoryUrl READ repositoryUrl)
    Q_PROPERTY(bool removeBeforeUpdate READ removeBeforeUpdate WRITE setRemoveBeforeUpdate)
    Q_PROPERTY(bool installed READ isInstalled)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)

    Q_DISABLE_COPY(Component);
    friend class QInstaller::Installer;
    friend class QInstaller::InstallerPrivate;

public:
    explicit Component(Installer *installer);
    explicit Component(KDUpdater::Update *update, Installer *installer);
    ~Component();

    enum SelectMode {
        NormalSelectMode,
        InitializeComponentTreeSelectMode
    };

    struct IsVirtual
    {
        bool operator() (const Component *comp) const
        {
            return comp->value(QLatin1String("Virtual"), QLatin1String("false"))
                .toLower() == QLatin1String("true");
        }
    };

    struct InstallPriorityLessThan
    {
        bool operator() (const Component *lhs, const Component *rhs)
        {
            const QLatin1String priority("InstallPriority");
            return lhs->value(priority).toInt() < rhs->value(priority).toInt();
        }
    };

    struct SortingPriorityLessThan
    {
        bool operator() (const Component *lhs, const Component *rhs) const
        {
            const QLatin1String priority("SortingPriority");
            return lhs->value(priority).toInt() < rhs->value(priority).toInt();
        }
    };

    void loadDataFromPackageInfo(const KDUpdater::PackageInfo &packageInfo);
    void loadDataFromUpdate(KDUpdater::Update* update);
    void updateState(const bool selected);

    QHash<QString, QString> variables() const;
    Q_INVOKABLE void setValue(const QString &key, const QString &value);
    Q_INVOKABLE QString value(const QString &key, const QString &defaultValue = QString()) const;

    QStringList archives() const;
    Installer *installer() const;

    void appendComponent(Component *component);
    Component *parentComponent(RunModes runMode = AllMode) const;
    QList<Component*> components(bool recursive = false, RunModes runMode = AllMode) const;

    void loadComponentScript(const QString &fileName);
    void loadTranslations(const QDir &directory, const QStringList &qms);
    void loadUserInterfaces(const QDir &directory, const QStringList &uis);
    void loadLicenses(const QString &directory, const QHash<QString, QVariant> &hash);
    void markAsPerformedInstallation();

    QStringList userInterfaces() const;
    QHash<QString, QPair<QString, QString> > licenses() const;
    Q_INVOKABLE QWidget *userInterface(const QString &name) const;
    Q_INVOKABLE virtual void createOperations();
    Q_INVOKABLE virtual void createOperationsForArchive(const QString &archive);
    Q_INVOKABLE virtual void createOperationsForPath(const QString &path);

    Q_INVOKABLE QList<QPair<QString, bool> > pathesForUninstallation() const;
    Q_INVOKABLE void registerPathForUninstallation(const QString &path, bool wipe = false);

    QList<KDUpdater::UpdateOperation*> operations() const;

    void addOperation(KDUpdater::UpdateOperation *operation);
    Q_INVOKABLE bool addOperation(const QString &operation, const QString &parameter1 = QString(),
        const QString &parameter2 = QString(), const QString &parameter3 = QString(),
        const QString &parameter4 = QString(), const QString &parameter5 = QString(),
        const QString &parameter6 = QString(), const QString &parameter7 = QString(),
        const QString &parameter8 = QString(), const QString &parameter9 = QString(),
        const QString &parameter10 = QString());

    void addElevatedOperation(KDUpdater::UpdateOperation *operation);
    Q_INVOKABLE bool addElevatedOperation(const QString &operation,
        const QString &parameter1 = QString(), const QString &parameter2 = QString(),
        const QString &parameter3 = QString(), const QString &parameter4 = QString(),
        const QString &parameter5 = QString(), const QString &parameter6 = QString(),
        const QString &parameter7 = QString(), const QString &parameter8 = QString(),
        const QString &parameter9 = QString(), const QString &parameter10 = QString());

    QStringList downloadableArchives() const;
    Q_INVOKABLE void addDownloadableArchive(const QString &path);
    Q_INVOKABLE void removeDownloadableArchive(const QString &path);

    QStringList stopProcessForUpdateRequests() const;
    Q_INVOKABLE void addStopProcessForUpdateRequest(const QString &process);
    Q_INVOKABLE void removeStopProcessForUpdateRequest(const QString &process);
    Q_INVOKABLE void setStopProcessForUpdateRequest(const QString &process, bool requested);

    QString name() const;
    QString displayName() const;

    QUrl repositoryUrl() const;
    void setRepositoryUrl(const QUrl &url);

    bool removeBeforeUpdate() const;
    void setRemoveBeforeUpdate(bool removeBeforeUpdate);

    Q_INVOKABLE bool isFromOnlineRepository() const;

    QStringList dependencies() const;

    bool autoCreateOperations() const;
    bool isSelected(RunModes runMode = AllMode) const;
    Q_INVOKABLE bool isInstalled() const;
    Q_INVOKABLE bool installationRequested() const;
    Q_INVOKABLE bool uninstallationRequested() const;
    Q_INVOKABLE bool wasInstalled() const;
    Q_INVOKABLE bool wasUninstalled() const;
    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool operationsCreatedSuccessfully() const;

    Qt::CheckState checkState(RunModes runMode = AllMode) const;

    void languageChanged();

    QList<Component*> dependees() const;

public Q_SLOTS:
    void setAutoCreateOperations(bool autoCreateOperations);
    void setSelected(bool selected, RunModes runMode = AllMode, SelectMode selectMode = NormalSelectMode);

Q_SIGNALS:
    void loaded();
    void selectedChanged(bool selected);
    void valueChanged(const QString &key, const QString &value);

protected:
    QScriptValue callScriptMethod(const QString &name,
        const QScriptValueList &parameters = QScriptValueList());

private:
    KDUpdater::UpdateOperation *createOperation(const QString &operation,
        const QString &parameter1 = QString(), const QString &parameter2 = QString(),
        const QString &parameter3 = QString(), const QString &parameter4 = QString(),
        const QString &parameter5 = QString(), const QString &parameter6 = QString(),
        const QString &parameter7 = QString(), const QString &parameter8 = QString(),
        const QString &parameter9 = QString(), const QString &parameter10 = QString());

private:
    class Private;
    Private *const d;
};

}   // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::Component*);

#endif // QINSTALLER_COMPONENT_H
