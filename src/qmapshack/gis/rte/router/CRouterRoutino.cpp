/**********************************************************************************************
    Copyright (C) 2014 Oliver Eichler oliver.eichler@gmx.de

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

**********************************************************************************************/

#include "canvas/CCanvas.h"
#include "CMainWindow.h"
#include "gis/CGisWorkspace.h"
#include "gis/rte/CGisItemRte.h"
#include "gis/rte/router/CRouterRoutino.h"
#include "gis/rte/router/routino/CRouterRoutinoPathSetup.h"
#include "helpers/CProgressDialog.h"
#include "helpers/CSettings.h"
#include "setup/IAppSetup.h"
#include <proj_api.h>
#include <QtWidgets>
#include <routino.h>


QPointer<CProgressDialog> CRouterRoutino::progress;


int ProgressFunc(double complete)
{
    if(CRouterRoutino::progress.isNull())
    {
        return true;
    }

    CRouterRoutino::progress->setValue(complete*100);

    return !CRouterRoutino::progress->wasCanceled();
}

CRouterRoutino * CRouterRoutino::pSelf = nullptr;

CRouterRoutino::CRouterRoutino(QWidget *parent)
    : IRouter(true, parent)
{
    pSelf = this;
    setupUi(this);

    connect(labelHelp, &QLabel::linkActivated, &CMainWindow::self(), static_cast<void (CMainWindow::*)(const QString&)>(&CMainWindow::slotLinkActivated));

    if(Routino_CheckAPIVersion() != ROUTINO_ERROR_NONE)
    {
        QMessageBox::warning(this, tr("Warning..."), tr("Found Routino with a wrong version. Expected %1 found %2").arg(ROUTINO_API_VERSION).arg(Routino_APIVersion), QMessageBox::Ok);
        return;
    }

    comboMode->addItem(tr("Shortest"));
    comboMode->addItem(tr("Quickest"));

    int res = 0;
    IAppSetup *setup = IAppSetup::getPlatformInstance();
    res = Routino_ParseXMLProfiles(setup->routinoPath("profiles.xml").toUtf8());
    if(res)
    {
        QMessageBox::critical(this, "Routino...", xlateRoutinoError(Routino_errno), QMessageBox::Abort);
        return;
    }

    res = Routino_ParseXMLTranslations(setup->routinoPath("translations.xml").toUtf8());
    if(res)
    {
        QMessageBox::critical(this, "Routino...", xlateRoutinoError(Routino_errno), QMessageBox::Abort);
        return;
    }

    comboProfile->addItem(tr("Foot"),       "foot");
    comboProfile->addItem(tr("Horse"),      "horse");
    comboProfile->addItem(tr("Wheelchair"), "wheelchair");
    comboProfile->addItem(tr("Bicycle"),    "bicycle");
    comboProfile->addItem(tr("Moped"),      "moped");
    comboProfile->addItem(tr("Motorcycle"), "motorcycle");
    comboProfile->addItem(tr("Motorcar"),   "motorcar");
    comboProfile->addItem(tr("Goods"),      "goods");

    comboLanguage->addItem(tr("English"),   "en");
    comboLanguage->addItem(tr("German"),    "de");
    comboLanguage->addItem(tr("French"),    "fr");
    comboLanguage->addItem(tr("Hungarian"), "hu");
    comboLanguage->addItem(tr("Dutch"),     "nl");
    comboLanguage->addItem(tr("Russian"),   "ru");
    comboLanguage->addItem(tr("Polish"),    "pl");

    connect(toolSetupPaths, &QToolButton::clicked, this, &CRouterRoutino::slotSetupPaths);

    SETTINGS;
    dbPaths = cfg.value("Route/routino/paths", dbPaths).toStringList();
    buildDatabaseList();

    comboProfile->setCurrentIndex(cfg.value("Route/routino/profile", 0).toInt());
    comboLanguage->setCurrentIndex(cfg.value("Route/routino/language", 0).toInt());
    comboMode->setCurrentIndex(cfg.value("Route/routino/mode", 0).toInt());
    comboDatabase->setCurrentIndex(cfg.value("Route/routino/database", 0).toInt());

    updateHelpText();
}

CRouterRoutino::~CRouterRoutino()
{
    SETTINGS;
    cfg.setValue("Route/routino/paths", dbPaths);
    cfg.setValue("Route/routino/profile", comboProfile->currentIndex());
    cfg.setValue("Route/routino/language", comboLanguage->currentIndex());
    cfg.setValue("Route/routino/mode", comboMode->currentIndex());
    cfg.setValue("Route/routino/database", comboDatabase->currentIndex());

    freeDatabaseList();
    Routino_FreeXMLProfiles();
    Routino_FreeXMLTranslations();
}

QString CRouterRoutino::xlateRoutinoError(int err)
{
    switch(err)
    {
    case ROUTINO_ERROR_NO_DATABASE:
        return tr("A function was called without the database variable set.");

    case ROUTINO_ERROR_NO_PROFILE:
        return tr("A function was called without the profile variable set.");

    case ROUTINO_ERROR_NO_TRANSLATION:
        return tr("A function was called without the translation variable set.");

    case ROUTINO_ERROR_NO_DATABASE_FILES:
        return tr("The specified database to load did not exist.");

    case ROUTINO_ERROR_BAD_DATABASE_FILES:
        return tr("The specified database could not be loaded.");

    case ROUTINO_ERROR_NO_PROFILES_XML:
        return tr("The specified profiles XML file did not exist.");

    case ROUTINO_ERROR_BAD_PROFILES_XML:
        return tr("The specified profiles XML file could not be loaded.");

    case ROUTINO_ERROR_NO_TRANSLATIONS_XML:
        return tr("The specified translations XML file did not exist.");

    case ROUTINO_ERROR_BAD_TRANSLATIONS_XML:
        return tr("The specified translations XML file could not be loaded.");

    case ROUTINO_ERROR_NO_SUCH_PROFILE:
        return tr("The requested profile name does not exist in the loaded XML file.");

    case ROUTINO_ERROR_NO_SUCH_TRANSLATION:
        return tr("The requested translation language does not exist in the loaded XML file.");

    case ROUTINO_ERROR_NO_NEARBY_HIGHWAY:
        return tr("In the routing database there is no highway near the coordinates to place a waypoint.");

    case ROUTINO_ERROR_PROFILE_DATABASE_ERR:
        return tr("The profile and database do not work together.");

    case ROUTINO_ERROR_NOTVALID_PROFILE:
        return tr("The profile being used has not been validated.");

    case ROUTINO_ERROR_BAD_USER_PROFILE:
        return tr("The user specified profile contained invalid data.");

    case ROUTINO_ERROR_BAD_OPTIONS:
        return tr("The routing options specified are not consistent with each other.");

    case ROUTINO_ERROR_WRONG_API_VERSION:
        return tr("There is a mismatch between the library and caller API version.");

    case ROUTINO_ERROR_PROGRESS_ABORTED:
        return tr("Route calculation was aborted by user.");
    }

    if(ROUTINO_ERROR_NO_ROUTE_1 <= err)
    {
        int n = err - 1000;
        return tr("A route could not be found to waypoint %1.").arg(n);
    }

    return tr("Unknown error: %1").arg(err);
}

bool CRouterRoutino::hasFastRouting()
{
    return IRouter::hasFastRouting() && (comboDatabase->count() != 0);
}

QString CRouterRoutino::getOptions()
{
    QString str;

    str  = tr("profile \"%1\"").arg(comboProfile->currentText());
    str += tr(", mode \"%1\"").arg(comboMode->currentText());
    return str;
}

void CRouterRoutino::setupPath(const QString& path)
{
    if(dbPaths.contains(path))
    {
        return;
    }

    dbPaths << path;
    buildDatabaseList();
    updateHelpText();
}

void CRouterRoutino::slotSetupPaths()
{
    CRouterRoutinoPathSetup dlg(dbPaths);
    dlg.exec();

    buildDatabaseList();
    updateHelpText();
}

void CRouterRoutino::buildDatabaseList()
{
    QRegExp re("(.*)-segments.mem");
    freeDatabaseList();

    for(const QString &path : dbPaths)
    {
        QDir dir(path);
        for(const QString &filename : dir.entryList(QStringList("*segments.mem"), QDir::Files|QDir::Readable, QDir::Name))
        {
            QString prefix;
            if(re.exactMatch(filename))
            {
                prefix = re.cap(1);
            }
            else
            {
                continue;
            }

#ifdef Q_OS_WIN
            Routino_Database * data = Routino_LoadDatabase(dir.absolutePath().toLocal8Bit(), prefix.toLocal8Bit());
#else
            Routino_Database * data = Routino_LoadDatabase(dir.absolutePath().toUtf8(), prefix.toUtf8());
#endif
            if(data)
            {
                comboDatabase->addItem(prefix.replace("_", " "), quint64(data));
            }
            else
            {
                QMessageBox::critical(this, "Routino...", xlateRoutinoError(Routino_errno), QMessageBox::Abort);
            }
        }
    }
}

void CRouterRoutino::freeDatabaseList()
{
    for(int i = 0; i < comboDatabase->count(); i++)
    {
        Routino_Database * data = (Routino_Database*)comboDatabase->itemData(i, Qt::UserRole).toULongLong();
        Routino_UnloadDatabase(data);
    }
    comboDatabase->clear();
}

void CRouterRoutino::updateHelpText()
{
    bool haveDB = (comboDatabase->count() != 0);

    frameHelp->setVisible(!haveDB);
    comboDatabase->setEnabled(haveDB);
}

void CRouterRoutino::calcRoute(const IGisItem::key_t& key)
{
    if(!mutex.tryLock())
    {
        return;
    }

    try
    {
        QTime time;
        time.start();

        CGisItemRte * rte = dynamic_cast<CGisItemRte*>(CGisWorkspace::self().getItemByKey(key));
        if(nullptr == rte)
        {
            throw QString();
        }

        Routino_Database * data = (Routino_Database*)comboDatabase->currentData(Qt::UserRole).toULongLong();
        if(nullptr == data)
        {
            throw QString();
        }

        rte->reset();

        QString strProfile  = comboProfile->currentData(Qt::UserRole).toString();
        QString strLanguage = comboLanguage->currentData(Qt::UserRole).toString();

        Routino_Profile *profile         = Routino_GetProfile(strProfile.toUtf8());
        Routino_Translation *translation = Routino_GetTranslation(strLanguage.toUtf8());

        int res = Routino_ValidateProfile(data, profile);
        if(res != 0)
        {
            throw xlateRoutinoError(Routino_errno);
        }

        int options = ROUTINO_ROUTE_LIST_HTML_ALL;
        if(comboMode->currentIndex() == 0)
        {
            options |= ROUTINO_ROUTE_SHORTEST;
        }
        if(comboMode->currentIndex() == 1)
        {
            options |= ROUTINO_ROUTE_QUICKEST;
        }

        SGisLine line;
        rte->getPolylineFromData(line);

        int idx = 0;
        QVector<Routino_Waypoint*> waypoints(line.size(), nullptr);
        for(const IGisLine::point_t &pt : line)
        {
            waypoints[idx] = Routino_FindWaypoint(data, profile, pt.coord.y()*RAD_TO_DEG, pt.coord.x()*RAD_TO_DEG);
            if(waypoints[idx] == nullptr)
            {
                throw xlateRoutinoError(Routino_errno);
            }
            idx++;
        }

        progress = new CProgressDialog(tr("Calculate route with %1").arg(getOptions()), 0, NOINT, this);

        Routino_Output * route = Routino_CalculateRoute(data, profile, translation, waypoints.data(), waypoints.size(), options, ProgressFunc);

        delete progress;

        if(nullptr != route)
        {
            rte->setResult(route, getOptions() + tr("<br/>Calculation time: %1s").arg(time.elapsed()/1000.0, 0, 'f', 2));
            Routino_DeleteRoute(route);
        }
        else
        {
            if(Routino_errno != ROUTINO_ERROR_PROGRESS_ABORTED)
            {
                throw xlateRoutinoError(Routino_errno);
            }
        }
    }
    catch(const QString& msg)
    {
        if(!msg.isEmpty())
        {
            QMessageBox::critical(this, "Routino...", msg, QMessageBox::Abort);
        }
    }

    mutex.unlock();

    CCanvas::triggerCompleteUpdate(CCanvas::eRedrawGis);
}


int CRouterRoutino::calcRoute(const QPointF& p1, const QPointF& p2, QPolygonF& coords)
{
    if(!mutex.tryLock())
    {
        return -1;
    }

    try
    {
        Routino_Database * data = (Routino_Database*)comboDatabase->currentData(Qt::UserRole).toULongLong();
        if(nullptr == data)
        {
            throw QString();
        }

        QString strProfile      = comboProfile->currentData(Qt::UserRole).toString();
        QString strLanguage     = comboLanguage->currentData(Qt::UserRole).toString();

        Routino_Profile *profile         = Routino_GetProfile(strProfile.toUtf8());
        Routino_Translation *translation = Routino_GetTranslation(strLanguage.toUtf8());

        int res = Routino_ValidateProfile(data, profile);
        if(res != 0)
        {
            throw xlateRoutinoError(Routino_errno);
        }

        int options = ROUTINO_ROUTE_LIST_HTML_ALL;
        if(comboMode->currentIndex() == 0)
        {
            options |= ROUTINO_ROUTE_SHORTEST;
        }
        if(comboMode->currentIndex() == 1)
        {
            options |= ROUTINO_ROUTE_QUICKEST;
        }

        Routino_Waypoint* waypoints[2] = {0};
        waypoints[0] = Routino_FindWaypoint(data, profile, p1.y()*RAD_TO_DEG, p1.x()*RAD_TO_DEG);
        if(waypoints[0] == nullptr)
        {
            throw xlateRoutinoError(Routino_errno);
        }

        waypoints[1] = Routino_FindWaypoint(data, profile, p2.y()*RAD_TO_DEG, p2.x()*RAD_TO_DEG);
        if(waypoints[1] == nullptr)
        {
            throw xlateRoutinoError(Routino_errno);
        }

        progress = new CProgressDialog(tr("Calculate route with %1").arg(getOptions()), 0, NOINT, this);

        Routino_Output * route = Routino_CalculateRoute(data, profile, translation, waypoints, 2, options, ProgressFunc);

        delete progress;

        if(route != nullptr)
        {
            Routino_Output * next = route;
            while(next)
            {
                if(next->type != ROUTINO_POINT_WAYPOINT)
                {
                    coords << QPointF(next->lon, next->lat);
                }
                next = next->next;
            }
            Routino_DeleteRoute(route);
        }
        else
        {
            if(Routino_errno != ROUTINO_ERROR_PROGRESS_ABORTED)
            {
                throw xlateRoutinoError(Routino_errno);
            }
            else
            {
                throw QString();
            }
        }
    }
    catch(const QString& msg)
    {
        coords.clear();

        if(!msg.isEmpty())
        {
            mutex.unlock();
            throw msg;
        }
    }

    mutex.unlock();
    return coords.size();
}
