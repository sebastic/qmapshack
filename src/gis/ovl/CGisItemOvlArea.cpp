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

#include "gis/ovl/CGisItemOvlArea.h"
#include "gis/ovl/CScrOptOvlArea.h"
#include "gis/ovl/CDetailsOvlArea.h"
#include "gis/CGisProject.h"
#include "gis/CGisDraw.h"

#include <QtWidgets>
#include <proj_api.h>

#define DEFAULT_COLOR       4
#define MIN_DIST_CLOSE_TO   10


const QColor CGisItemOvlArea::lineColors[OVL_N_COLORS] =
{
     Qt::black                    // 0
    ,Qt::darkRed                 // 1
    ,Qt::darkGreen               // 2
    ,Qt::darkYellow              // 3
    ,Qt::darkBlue                // 4
    ,Qt::darkMagenta             // 5
    ,Qt::darkCyan                // 6
    ,Qt::gray                    // 7
    ,Qt::darkGray                // 8
    ,Qt::red                     // 9
    ,Qt::green                   // 10
    ,Qt::yellow                  // 11
    ,Qt::blue                    // 12
    ,Qt::magenta                 // 13
    ,Qt::cyan                    // 14
    ,Qt::white                   // 15
    ,Qt::transparent             // 16
};

const QString CGisItemOvlArea::bulletColors[OVL_N_COLORS] =
{

                                 // 0
    QString("://icons/8x8/bullet_black.png")
                                 // 1
    ,QString("://icons/8x8/bullet_dark_red.png")
                                 // 2
    ,QString("://icons/8x8/bullet_dark_green.png")
                                 // 3
    ,QString("://icons/8x8/bullet_dark_yellow.png")
                                 // 4
    ,QString("://icons/8x8/bullet_dark_blue.png")
                                 // 5
    ,QString("://icons/8x8/bullet_dark_magenta.png")
                                 // 6
    ,QString("://icons/8x8/bullet_dark_cyan.png")
                                 // 7
    ,QString("://icons/8x8/bullet_gray.png")
                                 // 8
    ,QString("://icons/8x8/bullet_dark_gray.png")
                                 // 9
    ,QString("://icons/8x8/bullet_red.png")
                                 // 10
    ,QString("://icons/8x8/bullet_green.png")
                                 // 11
    ,QString("://icons/8x8/bullet_yellow.png")
                                 // 12
    ,QString("://icons/8x8/bullet_blue.png")
                                 // 13
    ,QString("://icons/8x8/bullet_magenta.png")
                                 // 14
    ,QString("://icons/8x8/bullet_cyan.png")
                                 // 15
    ,QString("://icons/8x8/bullet_white.png")
    ,QString("")                 // 16
};


QString CGisItemOvlArea::keyUserFocus;
const QPen CGisItemOvlArea::penBackground(Qt::white, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

CGisItemOvlArea::CGisItemOvlArea(const QPolygonF& line, const QString &name, CGisProject * project, int idx)
    : IGisItem(project, eTypeOvl, idx)
    , penForeground(Qt::blue, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
{
    area.name = name;
    readLine(line);

    flags |=  eFlagCreatedInQms|eFlagWriteAllowed;

    setColor(str2color(""));
    setText(1, "*");
    setText(0, area.name);
    setToolTip(0, getInfo());
    genKey();
    project->setText(1,"*");
}

CGisItemOvlArea::CGisItemOvlArea(const QDomNode &xml, CGisProject *project)
    : IGisItem(project, eTypeOvl, project->childCount())
    , penForeground(Qt::blue, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
{
    // --- start read and process data ----
    setColor(penForeground.color());
    readArea(xml, area);
    // --- stop read and process data ----
    setText(0, area.name);
    setToolTip(0, getInfo());
    genKey();

}

CGisItemOvlArea::~CGisItemOvlArea()
{
    // reset user focus if focused on this track
    if(key == keyUserFocus)
    {
        keyUserFocus.clear();
    }

}

void CGisItemOvlArea::genKey()
{
    if(key.isEmpty())
    {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData((const char*)&area, sizeof(area));
        key = md5.result().toHex();
    }
}

bool CGisItemOvlArea::isCloseTo(const QPointF& pos)
{
    foreach(const QPointF& pt, line)
    {
        if((pt - pos).manhattanLength() < MIN_DIST_CLOSE_TO)
        {
            return true;
        }
    }
    return false;
}

QPointF CGisItemOvlArea::getPointCloseBy(const QPoint& screenPos)
{
    qint32 i    = 0;
    qint32 idx  = -1;
    qint32  d   = NOINT;
    foreach(const QPointF& point, line)
    {
        int tmp = (screenPos - point).manhattanLength();
        if(tmp < d)
        {
            idx = i;
            d   = tmp;
        }
        i++;
    }

    if(idx < 0)
    {
        return NOPOINTF;
    }

    return line[idx];
}

void CGisItemOvlArea::readLine(const QPolygonF &line)
{
    area.pts.clear();
    area.pts.resize(line.size());

    for(int i = 0; i < line.size(); i++)
    {
        pt_t& areapt        = area.pts[i];
        const QPointF& pt   = line[i];

        areapt.lon = pt.x() * RAD_TO_DEG;
        areapt.lat = pt.y() * RAD_TO_DEG;
    }

    deriveSecondaryData();
}

void CGisItemOvlArea::readArea(const QDomNode& xml, area_t& area)
{
    readXml(xml, "ql:name", area.name);
    readXml(xml, "ql:cmt", area.cmt);
    readXml(xml, "ql:desc", area.desc);
    readXml(xml, "ql:src", area.src);
    readXml(xml, "ql:link", area.links);
    readXml(xml, "ql:number", area.number);
    readXml(xml, "ql:type", area.type);
    readXml(xml, "ql:color", area.color);
    readXml(xml, "ql:key", key);
    readXml(xml, "ql:flags", flags);
    readXml(xml, history);

    const QDomNode& seg = xml.namedItem("ql:seg");

    const QDomNodeList& xmlPts = seg.toElement().elementsByTagName("ql:pt");
    int M = xmlPts.count();
    area.pts.resize(M);
    for(int m = 0; m < M; ++m)
    {
        pt_t& pt = area.pts[m];
        const QDomNode& xmlPt = xmlPts.item(m);
        readWpt(xmlPt, pt);
    }


    setColor(str2color(area.color));

    deriveSecondaryData();
}

void CGisItemOvlArea::save(QDomNode& gpx)
{
    QDomDocument doc = gpx.ownerDocument();

    QDomElement xmlArea = doc.createElement("ql:area");
    gpx.appendChild(xmlArea);

    writeXml(xmlArea, "ql:name", area.name);
    writeXml(xmlArea, "ql:cmt", area.cmt);
    writeXml(xmlArea, "ql:desc", area.desc);
    writeXml(xmlArea, "ql:src", area.src);
    writeXml(xmlArea, "ql:link", area.links);
    writeXml(xmlArea, "ql:number", area.number);
    writeXml(xmlArea, "ql:type", area.type);
    writeXml(xmlArea, "ql:color", area.color);
    writeXml(xmlArea, "ql:key", key);
    writeXml(xmlArea, "ql:flags", flags);
    writeXml(xmlArea, history);

    QDomElement xmlSeg = doc.createElement("ql:seg");
    xmlArea.appendChild(xmlSeg);

    foreach(const pt_t& pt, area.pts)
    {
        QDomElement xmlPt = doc.createElement("ql:pt");
        xmlSeg.appendChild(xmlPt);
        writeWpt(xmlPt, pt);
    }

    setText(1, "");

}

void CGisItemOvlArea::edit()
{
    CDetailsOvlArea dlg(*this, 0);
    dlg.exec();

    deriveSecondaryData();
}

void CGisItemOvlArea::deriveSecondaryData()
{
    qreal north = -90;
    qreal east  = -180;
    qreal south =  90;
    qreal west  =  180;

    foreach(const pt_t& pt, area.pts)
    {
        if(pt.lon < west)  west    = pt.lon;
        if(pt.lon > east)  east    = pt.lon;
        if(pt.lat < south) south   = pt.lat;
        if(pt.lat > north) north   = pt.lat;
    }

    boundingRect = QRectF(QPointF(west * DEG_TO_RAD, north * DEG_TO_RAD), QPointF(east * DEG_TO_RAD,south * DEG_TO_RAD));
}

void CGisItemOvlArea::drawItem(QPainter& p, const QRectF& viewport, QList<QRectF>& blockedAreas, CGisDraw * gis)
{
    line.clear();
    if(!viewport.intersects(boundingRect))
    {
        return;
    }

    QPointF pt1;

    foreach(const pt_t& pt, area.pts)
    {
        pt1.setX(pt.lon);
        pt1.setY(pt.lat);
        pt1 *= DEG_TO_RAD;
        line << pt1;
    }

    gis->convertRad2Px(line);

    p.setBrush(QBrush(color, Qt::BDiagPattern));
    p.setPen(penBackground);
    p.drawPolygon(line);

    penForeground.setColor(color);
    p.setPen(penForeground);
    p.drawPolygon(line);

}

void CGisItemOvlArea::drawLabel(QPainter& p, const QRectF& viewport,QList<QRectF>& blockedAreas, const QFontMetricsF& fm, CGisDraw * gis)
{
    if(line.isEmpty())
    {
        return;
    }
    QPointF pt  = getPolygonCentroid(line);
    QRectF rect = fm.boundingRect(area.name);
    rect.adjust(-2,-2,2,2);
    rect.moveCenter(pt);

    CCanvas::drawText(getName(), p, pt.toPoint(), Qt::darkBlue);
    blockedAreas << rect;
}

void CGisItemOvlArea::drawHighlight(QPainter& p)
{
    if(line.isEmpty() || key == keyUserFocus)
    {
        return;
    }
    p.setPen(QPen(QColor(255,0,0,100),11,Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolygon(line);

}

void CGisItemOvlArea::gainUserFocus(bool yes)
{
    keyUserFocus = yes ? key : "";
}

QPointF CGisItemOvlArea::getPolygonCentroid(const QPolygonF& polygon)
{
    int i, len;
    qreal x = 0, y = 0;

    len = polygon.size();

    for(i = 0; i < len; i++)
    {
        x = x + polygon[i].x();
        y = y + polygon[i].y();
    }
    x = x / len;
    y = y / len;

    return QPointF(x,y);
}

IScrOpt * CGisItemOvlArea::getScreenOptions(const QPoint& origin, IMouse * mouse)
{
    if(scrOpt.isNull())
    {
        scrOpt = new CScrOptOvlArea(this, origin, mouse);
    }
    return scrOpt;
}

const QString& CGisItemOvlArea::getName()
{
    return area.name;
}

QString CGisItemOvlArea::getInfo()
{
    QString str = getName();

    return str;
}

void CGisItemOvlArea::getData(QPolygonF& line)
{    
    line.clear();
    foreach(const pt_t& pt, area.pts)
    {
        line << QPointF(pt.lon * DEG_TO_RAD, pt.lat * DEG_TO_RAD);
    }
}

void CGisItemOvlArea::setData(const QPolygonF& line)
{
    readLine(line);

    flags |= eFlagTainted;
    setText(1,"*");
    setToolTip(0, getInfo());
    parent()->setText(1,"*");
    changed(QObject::tr("Changed area shape."));
}


void CGisItemOvlArea::setColor(int idx)
{
    int N = sizeof(lineColors)/sizeof(QColor);
    if(idx >= N)
    {
        return;
    }
    setColor(lineColors[idx]);
    changed(QObject::tr("Changed color"));
}

void CGisItemOvlArea::setColor(const QColor& c)
{
    int n;
    int N = sizeof(lineColors)/sizeof(QColor);

    for(n = 0; n < N; n++)
    {
        if(lineColors[n] == c)
        {
            colorIdx    = n;
            color       = lineColors[n];
            bullet      = QPixmap(bulletColors[n]);
            break;
        }
    }

    if(n == N)
    {
        colorIdx    = DEFAULT_COLOR;
        color       = lineColors[DEFAULT_COLOR];
        bullet      = QPixmap(bulletColors[DEFAULT_COLOR]);
    }

    setIcon(color.name());
}

void CGisItemOvlArea::setIcon(const QString& c)
{
    area.color   = c;
    icon        = QPixmap("://icons/48x48/Area.png");

    QPixmap mask( icon.size() );
    mask.fill( str2color(c) );
    mask.setMask( icon.createMaskFromColor( Qt::transparent ) );
    icon = mask.scaled(22,22, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QTreeWidgetItem::setIcon(0,icon);
}