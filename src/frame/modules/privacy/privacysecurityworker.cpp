// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "applicationitem.h"
#include "privacysecurityworker.h"
#include "privacysecuritydataproxy.h"

#include <polkit-qt5-1/PolkitQt1/Authority>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFuture>
#include <QtConcurrent>
#include <QFutureWatcher>

PrivacySecurityWorker::PrivacySecurityWorker(PrivacySecurityModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_dataProxy(new PrivacySecurityDataProxy(this))
    , m_checkAuthorizationing(false)
{
    connect(m_dataProxy, &PrivacySecurityDataProxy::fileArmorExistsChanged, this, &PrivacySecurityWorker::fileArmorExistsChanged);
}

PrivacySecurityWorker::~PrivacySecurityWorker()
{
}

bool PrivacySecurityWorker::existsFileArmor() const
{
    return m_dataProxy->existsFileArmor();
}

void PrivacySecurityWorker::activate()
{
    if (!m_pathList.isEmpty())
        return;
    m_dataProxy->init();
    connect(m_model, &PrivacySecurityModel::requestSetPremissionMode, this, &PrivacySecurityWorker::setPermissionMode);
    connect(m_model, &PrivacySecurityModel::requestUpdateCacheBlacklist, this, &PrivacySecurityWorker::updateCacheBlacklist);
    connect(m_dataProxy, &PrivacySecurityDataProxy::fileAppsChanged, this, &PrivacySecurityWorker::onFileAppsChanged);
    connect(m_dataProxy, &PrivacySecurityDataProxy::fileModeChanged, this, &PrivacySecurityWorker::onFileModeChanged);
    connect(m_dataProxy, &PrivacySecurityDataProxy::cameraAppsChanged, this, &PrivacySecurityWorker::onCameraAppsChanged);
    connect(m_dataProxy, &PrivacySecurityDataProxy::cameraModeChanged, this, &PrivacySecurityWorker::onCameraModeChanged);

    QString envPath = QProcessEnvironment::systemEnvironment().value("PATH");
    m_pathList = envPath.split(':');
    m_dataProxy->getAllItemInfos();
    connect(m_dataProxy, &PrivacySecurityDataProxy::itemInfosChanged, this, &PrivacySecurityWorker::onItemInfosChanged);
    connect(m_dataProxy, &PrivacySecurityDataProxy::itemChanged, this, &PrivacySecurityWorker::onItemChanged);

    QStringList folders = { m_model->premissiontoPath(PermissionType::DocumentsFoldersPermission), m_model->premissiontoPath(PermissionType::PicturesFoldersPermission) };
    m_dataProxy->cameraGetMode();
    m_dataProxy->cameraGetApps();
    for (auto &&folder : folders) {
        m_dataProxy->fileGetMode(folder);
        m_dataProxy->fileGetApps(folder);
    }

    m_model->onCacheBlacklistChanged(m_dataProxy->getCacheBlacklist());
}

void PrivacySecurityWorker::deactivate()
{
    checkAuthorizationCancel();
}

QString PrivacySecurityWorker::getAppPath(const QString &filePath)
{
    static QStringList s_excludeBin = {
        "gio", "dbus-send", "python"
    };
    QString appPath;
    QFile file(filePath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        while (!file.atEnd()) {
            QString line = file.readLine();
            if (line.startsWith("Exec=")) {
                QString app;
                if (line.startsWith("Exec=\"")) {
                    app = line.split("\"").at(1).trimmed();
                } else {
                    app = line.split(' ').first().mid(5).trimmed().remove("\"");
                }
                if (app.endsWith(".sh") || s_excludeBin.contains(app)) {
                    break;
                } else if (QFile::exists(app)) {
                    appPath = app;
                } else {
                    for (auto &&binDir : m_pathList) {
                        QDir bin(binDir);
                        if (bin.exists(app)) {
                            appPath = bin.absolutePath() + "/" + app;
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    file.close();
    return appPath;
}

void PrivacySecurityWorker::updateCheckAuthorizationing(bool checking)
{
    if (m_checkAuthorizationing != checking) {
        m_checkAuthorizationing = checking;
        if (m_checkAuthorizationing) {
            // true情况延时发信号，防止界面频繁禁用
            QTimer::singleShot(100, this, [this]() {
                if (m_checkAuthorizationing) {
                    Q_EMIT checkAuthorization(m_checkAuthorizationing);
                }
            });
        } else {
            Q_EMIT checkAuthorization(m_checkAuthorizationing);
        }
    }
}

void PrivacySecurityWorker::updateAppPath()
{
    qInfo() << "update appPath begin";
    for (auto &&item : m_model->applictionItems()) {
        QString path = getAppPath(item->id());
        if (path.isEmpty()) {
            m_model->removeApplictionItem(item->id());
        } else {
            item->onAppPathChanged(path);
        }
    }
    qInfo() << "update appPath end";
}

void PrivacySecurityWorker::onItemInfosChanged(const AppItemInfoList &itemList)
{
    // 批量加数据，先阻塞信号，再给出完成信号
    qInfo() << "update item begin";
    m_model->dataUpdateFinished(true);
    for (auto &&item : itemList) {
        addAppItem(item);
    }
    m_model->dataUpdateFinished(false);
    // 延时获取appPath
    QMetaObject::invokeMethod(this, "updateAppPath", Qt::QueuedConnection);
    qInfo() << "update item end";
}

void PrivacySecurityWorker::onItemChanged(const QString &status, const AppItemInfo &itemInfo, qlonglong categoryID)
{
    if (status == "created") {
        addAppItem(itemInfo);
    } else if (status == "deleted") {
        m_model->removeApplictionItem(itemInfo.Path);
    }
}

void PrivacySecurityWorker::addAppItem(const AppItemInfo &itemInfo)
{
    // 不展示的应用
    static QStringList s_excludeApp = {
        "dde-computer", "dde-control-center", "dde-file-manager", "dde-trash", "deepin-manual"
    };
    if (s_excludeApp.contains(itemInfo.ID)) {
        return;
    }
    ApplicationItem *appItem = new ApplicationItem();
    appItem->onIdChanged(itemInfo.Path);
    appItem->onNameChanged(itemInfo.Name);
    if (m_model->addApplictionItem(appItem)) {
        appItem->onIconChanged(itemInfo.Icon);
        m_model->updatePermission(appItem);
        connect(appItem, &ApplicationItem::requestSetPremissionEnabled, this, &PrivacySecurityWorker::setAppPermissionEnable);
    } else {
        delete appItem;
    }
}

void PrivacySecurityWorker::onFileAppsChanged(const QString &file, const QPair<QStringList, bool> &apps)
{
    if (apps.second)
        return;
    m_model->onAppPremissionEnabledChanged(file, apps.first);
}

void PrivacySecurityWorker::onFileModeChanged(const QString &file, int mode)
{
    int premission = m_model->pathtoPremission(file, true);
    if (premission == 0)
        return;

    m_model->onPremissionModeChanged(premission, mode);
}

void PrivacySecurityWorker::onCameraAppsChanged(const QPair<QStringList, bool> &apps)
{
    if (apps.second)
        return;
    m_model->onAppPremissionEnabledChanged("camera", apps.first);
}

void PrivacySecurityWorker::onCameraModeChanged(int mode)
{
    m_model->onPremissionModeChanged(PermissionType::CameraPermission, mode);
}

void PrivacySecurityWorker::setAppPermissionEnableByCheck(bool ok)
{
    while (!m_cacheAppPermission.isEmpty()) {
        auto &&it = m_cacheAppPermission.takeFirst();
        ApplicationItem *item = it.first;
        int premission = it.second.first;
        bool enabled = it.second.second;
        if (ok) {
            // 设置App权限
            QString file = m_model->premissiontoPath(premission);
            if (file.isEmpty()) {
                item->onPremissionEnabledChanged(premission, enabled);
                item->onPremissionEnabledChanged(premission, !enabled); // 触发信号
                continue;
            }
            QStringList blacklist = m_model->blacklist(file);
            QStringList executablePaths = item->executablePaths();
            if (executablePaths.isEmpty()) {
                QString path = item->appPath();
                if (path.isEmpty()) {
                    path = getAppPath(item->id());
                    item->onAppPathChanged(path);
                }
                executablePaths = m_dataProxy->getExecutable(path);
                item->onExecutablePathsChanged(executablePaths);
            }
            if (enabled) {
                for (auto it = blacklist.begin(); it != blacklist.end();) {
                    if (executablePaths.contains(*it)) {
                        it = blacklist.erase(it);
                    } else {
                        it++;
                    }
                }
            } else {
                blacklist.append(executablePaths);
            }
            blacklist.removeDuplicates();
            switch (premission) {
            case PermissionType::CameraPermission:
                m_dataProxy->cameraEnable(blacklist, false);
                break;
            default:
                m_dataProxy->fileEnable(file, blacklist, false);
                break;
            }
        } else {
            item->onPremissionEnabledChanged(premission, enabled);
            item->onPremissionEnabledChanged(premission, !enabled); // 触发信号
        }
    }
}

void PrivacySecurityWorker::setPermissionMode(int premission, int mode)
{
    if (m_checkAuthorizationing)
        return;
    connect(PolkitQt1::Authority::instance(), &PolkitQt1::Authority::checkAuthorizationFinished, this, [this, premission, mode](PolkitQt1::Authority::Result authenticationResult) {
        disconnect(PolkitQt1::Authority::instance(), nullptr, this, nullptr);
        updateCheckAuthorizationing(false);
        if (PolkitQt1::Authority::Result::Yes != authenticationResult) {
            m_model->onPremissionModeChanged(premission, mode);
            m_model->onPremissionModeChanged(premission, mode == PrivacySecurityDataProxy::AllDisabled ? PrivacySecurityDataProxy::ByCustom : PrivacySecurityDataProxy::AllDisabled); // 触发信号
            return;
        }
        // 设置总权限
        switch (premission) {
        case PermissionType::FoldersPermission:
            m_dataProxy->fileSetMode(m_model->premissiontoPath(PermissionType::DocumentsFoldersPermission), mode);
            m_dataProxy->fileSetMode(m_model->premissiontoPath(PermissionType::PicturesFoldersPermission), mode);
            break;
        case PermissionType::CameraPermission:
            m_dataProxy->cameraSetMode(mode);
            break;
        default:
            m_model->onPremissionModeChanged(premission, mode);
            m_model->onPremissionModeChanged(premission, mode == PrivacySecurityDataProxy::AllDisabled ? PrivacySecurityDataProxy::ByCustom : PrivacySecurityDataProxy::AllDisabled); // 触发信号
            return;
        }
    });
    updateCheckAuthorizationing(true);
    PolkitQt1::Authority::instance()->checkAuthorization("com.deepin.daemon.accounts.user-administration", PolkitQt1::UnixProcessSubject(getpid()), PolkitQt1::Authority::AllowUserInteraction);
}

void PrivacySecurityWorker::setAppPermissionEnable(int premission, bool enabled, ApplicationItem *item)
{
    m_cacheAppPermission.append({ item, { premission, enabled } });
    if (m_checkAuthorizationing)
        return;
    connect(PolkitQt1::Authority::instance(), &PolkitQt1::Authority::checkAuthorizationFinished, this, [this](PolkitQt1::Authority::Result authenticationResult) {
        disconnect(PolkitQt1::Authority::instance(), nullptr, this, nullptr);
        updateCheckAuthorizationing(false);
        setAppPermissionEnableByCheck(PolkitQt1::Authority::Result::Yes == authenticationResult);
    });
    updateCheckAuthorizationing(true);
    PolkitQt1::Authority::instance()->checkAuthorization("com.deepin.daemon.accounts.user-administration", PolkitQt1::UnixProcessSubject(getpid()), PolkitQt1::Authority::AllowUserInteraction);
}

void PrivacySecurityWorker::checkAuthorizationCancel()
{
    if (m_checkAuthorizationing) {
        //        PolkitQt1::Authority::instance()->checkAuthorizationCancel(); // 取消后不能再拉起
        disconnect(PolkitQt1::Authority::instance(), nullptr, this, nullptr);
        updateCheckAuthorizationing(false);
    }
}

void PrivacySecurityWorker::updateCacheBlacklist(const QMap<QString, QStringList> &cacheBlacklist)
{
    m_dataProxy->setCacheBlacklist(cacheBlacklist);
}