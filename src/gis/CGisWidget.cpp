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

#include "CMainWindow.h"
#include "gis/CGisDraw.h"
#include "gis/CGisWidget.h"
#include "gis/IGisItem.h"
#include "gis/IGisProject.h"
#include "gis/gpx/CGpxProject.h"
#include "gis/ovl/CGisItemOvlArea.h"
#include "gis/qms/CQmsProject.h"
#include "gis/trk/CGisItemTrk.h"
#include "gis/wpt/CGisItemWpt.h"
#include "gis/wpt/CProjWpt.h"
#include "helpers/CSelectProjectDialog.h"
#include "helpers/CSettings.h"

#include <QtWidgets>
#include <QtXml>

CGisWidget * CGisWidget::pSelf = 0;

CGisWidget::CGisWidget(QMenu *menuProject, QWidget *parent)
    : QWidget(parent)
{
    pSelf = this;
    setupUi(this);

    treeWks->setExternalMenu(menuProject);

    SETTINGS;
    treeWks->header()->resizeSection(0, cfg.value("Workspace/treeWks/colum0/size", 100).toInt());

    connect(treeWks, SIGNAL(sigChanged()), SIGNAL(sigChanged()));
}

CGisWidget::~CGisWidget()
{
    SETTINGS;
    cfg.setValue("Workspace/treeWks/colum0/size", treeWks->header()->sectionSize(0));
}

void CGisWidget::loadGisProject(const QString& filename)
{

    // add project to workspace
    QApplication::setOverrideCursor(Qt::WaitCursor);
    IGisItem::mutexItems.lock();
    IGisProject * item = 0;
    QString suffix = QFileInfo(filename).suffix().toLower();
    if(suffix == "gpx")
    {
        item = new CGpxProject(filename, treeWks);
    }
    else if(suffix == "qms")
    {
        item = new CQmsProject(filename, treeWks);
    }

    if(item && !item->isValid())
    {
        delete item;
    }

    // skip if project is already loaded
    if(item && treeWks->hasProject(item))
    {
        delete item;
    }

    IGisItem::mutexItems.unlock();
    QApplication::restoreOverrideCursor();

    emit sigChanged();
}


void CGisWidget::slotSaveAll()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    IGisItem::mutexItems.lock();
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        IGisProject * item = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(item == 0)
        {
            continue;
        }
        item->save();
    }
    IGisItem::mutexItems.unlock();
    QApplication::restoreOverrideCursor();
}

IGisProject * CGisWidget::selectProject()
{
    QString key, name;
    CSelectProjectDialog::type_e type;

    CSelectProjectDialog dlg(key, name, type, treeWks);
    dlg.exec();

    IGisProject * project = 0;
    if(!key.isEmpty())
    {
        IGisItem::mutexItems.lock();
        for(int i = 0; i < treeWks->topLevelItemCount(); i++)
        {
            project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
            if(project == 0)
            {
                continue;
            }
            if(key == project->getKey())
            {
                break;
            }
        }
        IGisItem::mutexItems.unlock();
    }
    else if(!name.isEmpty())
    {
        IGisItem::mutexItems.lock();
        if(type == CSelectProjectDialog::eTypeGpx)
        {
            project = new CGpxProject(name, treeWks);
        }
        else if (type == CSelectProjectDialog::eTypeQms)
        {
            project = new CQmsProject(name, treeWks);
        }

        IGisItem::mutexItems.unlock();
    }

    return(project);
}

void CGisWidget::getItemsByPos(const QPointF& pos, QList<IGisItem*>& items)
{
    IGisItem::mutexItems.lock();
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }
        project->getItemByPos(pos, items);
    }
    IGisItem::mutexItems.unlock();
}

IGisItem * CGisWidget::getItemByKey(const QString& key)
{
    IGisItem * item = 0;
    IGisItem::mutexItems.lock();
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }
        item = project->getItemByKey(key);
        if(item != 0)
        {
            break;
        }
    }
    IGisItem::mutexItems.unlock();
    return(item);
}

void CGisWidget::delItemByKey(const QString& key)
{
    IGisItem::mutexItems.lock();
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }
        project->delItemByKey(key);
    }

    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::editItemByKey(const QString& key)
{
    IGisItem::mutexItems.lock();
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }
        project->editItemByKey(key);
    }

    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::projWptByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemWpt * wpt = dynamic_cast<CGisItemWpt*>(getItemByKey(key));
    if(wpt != 0)
    {
        CProjWpt dlg(*wpt, 0);
        dlg.exec();
    }

    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::moveWptByKey(const QString& key)
{
    IGisItem::mutexItems.lock();
    CGisItemWpt * wpt = dynamic_cast<CGisItemWpt*>(getItemByKey(key));
    if(wpt != 0)
    {
        CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
        if(canvas != 0)
        {
            canvas->setMouseMoveWpt(*wpt);
        }
    }
    IGisItem::mutexItems.unlock();
}

void CGisWidget::focusTrkByKey(bool yes, const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemTrk * trk = dynamic_cast<CGisItemTrk*>(getItemByKey(key));
    if(trk != 0)
    {
        trk->gainUserFocus(yes);
    }

    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::cutTrkByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemTrk * trk = dynamic_cast<CGisItemTrk*>(getItemByKey(key));
    if(trk != 0 && trk->cut())
    {
        delete trk;
    }
    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::reverseTrkByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemTrk * trk = dynamic_cast<CGisItemTrk*>(getItemByKey(key));
    if(trk)
    {
        trk->reverse();
    }
    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::combineTrkByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemTrk * trk = dynamic_cast<CGisItemTrk*>(getItemByKey(key));
    if(trk)
    {
        trk->combine();
    }
    IGisItem::mutexItems.unlock();
    emit sigChanged();
}

void CGisWidget::editTrkByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemTrk * trk = dynamic_cast<CGisItemTrk*>(getItemByKey(key));
    if(trk != 0)
    {
        CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
        if(canvas != 0)
        {
            canvas->setMouseEditTrk(*trk);
        }
    }

    IGisItem::mutexItems.unlock();
}

void CGisWidget::rangeTrkByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemTrk * trk = dynamic_cast<CGisItemTrk*>(getItemByKey(key));
    if(trk != 0)
    {
        CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
        if(canvas != 0)
        {
            canvas->setMouseRangeTrk(*trk);
        }
    }

    IGisItem::mutexItems.unlock();
}

void CGisWidget::editAreaByKey(const QString& key)
{
    IGisItem::mutexItems.lock();

    CGisItemOvlArea * area = dynamic_cast<CGisItemOvlArea*>(getItemByKey(key));
    if(area != 0)
    {
        CCanvas * canvas = CMainWindow::self().getVisibleCanvas();
        if(canvas != 0)
        {
            canvas->setMouseEditArea(*area);
        }
    }

    IGisItem::mutexItems.unlock();
}

void CGisWidget::draw(QPainter& p, const QRectF& viewport, CGisDraw * gis)
{
    QFontMetricsF fm(CMainWindow::self().getMapFont());
    QList<QRectF> blockedAreas;
    QSet<QString> seenKeys;

    IGisItem::mutexItems.lock();
    // draw mandatory stuff first
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        if(gis->needsRedraw())
        {
            break;
        }

        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }
        project->drawItem(p, viewport, blockedAreas, seenKeys, gis);
    }

    // reset seen keys as lables will build the list a second time
    seenKeys.clear();

    // draw optional labels second
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        if(gis->needsRedraw())
        {
            break;
        }

        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }
        project->drawLabel(p, viewport, blockedAreas, seenKeys, fm, gis);
    }
    IGisItem::mutexItems.unlock();
}

void CGisWidget::fastDraw(QPainter& p, const QRectF& viewport, CGisDraw *gis)
{
    /*
        Mutex locking will make map moving very slow if there are many GIS items
        visible. Remove it for now. But I am not sure if that is a good idea.
     */
    //IGisItem::mutexItems.lock();
    for(int i = 0; i < treeWks->topLevelItemCount(); i++)
    {
        IGisProject * project = dynamic_cast<IGisProject*>(treeWks->topLevelItem(i));
        if(project == 0)
        {
            continue;
        }

        project->drawItem(p, viewport, gis);
    }
    //IGisItem::mutexItems.unlock();
}
