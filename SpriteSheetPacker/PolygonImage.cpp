
#include "PolygonImage.h"
#include "clipper.hpp"
#include "poly2tri.h"

static unsigned short quadIndices[]={0,1,2, 3,2,1};
const static float PRECISION = 10.0f;

/** Clamp a value between from and to.
 */

inline float clampf(float value, float min_inclusive, float max_inclusive)
{
    if (min_inclusive > max_inclusive) {
        std::swap(min_inclusive, max_inclusive);
    }
    return value < min_inclusive ? min_inclusive : value < max_inclusive? value : max_inclusive;
}

PolygonImage::PolygonImage(const QImage& image, const float epsilon, const float threshold)
    : _image(image)
    , _width(image.width())
    , _height(image.height())
    , _threshold(threshold)
    , _filename("unknow")
{

}

std::vector<QPointF> PolygonImage::traceAll(const QRectF& rect, const float& threshold) {
    std::vector<QPointF> _points;
    QPointF i;
    for(i.ry() = rect.top(); i.y() <= rect.bottom(); i.ry()++) {
        for(i.rx() = rect.left(); i.x() <= rect.right(); i.rx()++) {
            auto alpha = getAlphaByPos(i);
            if(alpha > 2) {
                _points.push_back(QPointF(i.x() - rect.left(), rect.size().height() - i.y() + rect.top()));
            }
        }
    }
    return _points;
}

std::vector<QPointF> PolygonImage::trace(const QRectF& rect, const float& threshold) {
    QPointF first = findFirstNoneTransparentPixel(rect, threshold);
    return marchSquare(rect, first, threshold);
}

QPointF PolygonImage::findFirstNoneTransparentPixel(const QRectF& rect, const float& threshold) {
    bool found = false;
    QPointF i;
    for(i.ry() = rect.top(); i.y() <= rect.bottom(); i.ry()++) {
        if (found) break;
        for(i.rx() = rect.left(); i.x() <= rect.right(); i.rx()++) {
            auto alpha = getAlphaByPos(i);
            if(alpha > threshold) {
                found = true;
                break;
            }
        }
    }
    qWarning() << "Image is all transparent!";
    return i;
}

unsigned char PolygonImage::getAlphaByIndex(const unsigned int& i) {
    return getAlphaByPos(getPosFromIndex(i));
}

unsigned char PolygonImage::getAlphaByPos(const QPointF& pos) {
    return qAlpha(_image.pixel(QPoint(pos.x(), pos.y())));
}

unsigned int PolygonImage::getSquareValue(const unsigned int& x, const unsigned int& y, const QRectF& rect, const float& threshold)
{
    /*
     checking the 2x2 pixel grid, assigning these values to each pixel, if not transparent
     +---+---+
     | 1 | 2 |
     +---+---+
     | 4 | 8 | <- current pixel (curx,cury)
     +---+---+
     */
    unsigned int sv = 0;
    //NOTE: due to the way we pick points from texture, rect needs to be smaller, otherwise it goes outside 1 pixel
    auto fixedRect = QRectF(rect.topLeft(), rect.size()-QSize(2,2));

    QPointF tl = QPointF(x-1, y-1);
    sv += (fixedRect.contains(tl) && getAlphaByPos(tl) > threshold)? 1 : 0;
    QPointF tr = QPointF(x, y-1);
    sv += (fixedRect.contains(tr) && getAlphaByPos(tr) > threshold)? 2 : 0;
    QPointF bl = QPointF(x-1, y);
    sv += (fixedRect.contains(bl) && getAlphaByPos(bl) > threshold)? 4 : 0;
    QPointF br = QPointF(x, y);
    sv += (fixedRect.contains(br) && getAlphaByPos(br) > threshold)? 8 : 0;
    Q_ASSERT_X(sv != 0 && sv != 15, "square value should not be 0, or 15", "");
    return sv;
}

std::vector<QPointF> PolygonImage::marchSquare(const QRectF& rect, const QPointF& start, const float& threshold)
{
    int stepx = 0;
    int stepy = 0;
    int prevx = 0;
    int prevy = 0;
    int startx = start.x();
    int starty = start.y();
    int curx = startx;
    int cury = starty;
    unsigned int count = 0;
    unsigned int totalPixel = _width*_height;
    bool problem = false;
    std::vector<int> case9s;
    std::vector<int> case6s;
    int i;
    std::vector<int>::iterator it;
    std::vector<QPointF> _points;
    do{
        int sv = getSquareValue(curx, cury, rect, threshold);
        switch(sv){

            case 1:
            case 5:
            case 13:
                /* going UP with these cases:
                 1          5           13
                 +---+---+  +---+---+  +---+---+
                 | 1 |   |  | 1 |   |  | 1 |   |
                 +---+---+  +---+---+  +---+---+
                 |   |   |  | 4 |   |  | 4 | 8 |
                 +---+---+  +---+---+  +---+---+
                 */
                stepx = 0;
                stepy = -1;
                break;


            case 8:
            case 10:
            case 11:
                /* going DOWN with these cases:
                 8          10          11
                 +---+---+  +---+---+   +---+---+
                 |   |   |  |   | 2 |   | 1 | 2 |
                 +---+---+  +---+---+   +---+---+
                 |   | 8 |  |   | 8 |   |   | 8 |
                 +---+---+  +---+---+  	+---+---+
                 */
                stepx = 0;
                stepy = 1;
                break;


            case 4:
            case 12:
            case 14:
                /* going LEFT with these cases:
                 4          12          14
                 +---+---+  +---+---+   +---+---+
                 |   |   |  |   |   |   |   | 2 |
                 +---+---+  +---+---+   +---+---+
                 | 4 |   |  | 4 | 8 |   | 4 | 8 |
                 +---+---+  +---+---+  	+---+---+
                 */
                stepx = -1;
                stepy = 0;
                break;


            case 2 :
            case 3 :
            case 7 :
                /* going RIGHT with these cases:
                 2          3           7
                 +---+---+  +---+---+   +---+---+
                 |   | 2 |  | 1 | 2 |   | 1 | 2 |
                 +---+---+  +---+---+   +---+---+
                 |   |   |  |   |   |   | 4 |   |
                 +---+---+  +---+---+  	+---+---+
                 */
                stepx=1;
                stepy=0;
                break;
            case 9 :
                /*
                 +---+---+
                 | 1 |   |
                 +---+---+
                 |   | 8 |
                 +---+---+
                 this should normally go UP, but if we already been here, we go down
                */
                //find index from xy;
                i = getIndexFromPos(curx, cury);
                it = find (case9s.begin(), case9s.end(), i);
                if (it != case9s.end())
                {
                    //found, so we go down, and delete from case9s;
                    stepx = 0;
                    stepy = 1;
                    case9s.erase(it);
                    problem = true;
                }
                else
                {
                    //not found, we go up, and add to case9s;
                    stepx = 0;
                    stepy = -1;
                    case9s.push_back(i);
                }
                break;
            case 6 :
                /*
                 6
                 +---+---+
                 |   | 2 |
                 +---+---+
                 | 4 |   |
                 +---+---+
                 this normally go RIGHT, but if its coming from UP, it should go LEFT
                 */
                i = getIndexFromPos(curx, cury);
                it = find (case6s.begin(), case6s.end(), i);
                if (it != case6s.end())
                {
                    //found, so we go down, and delete from case9s;
                    stepx = -1;
                    stepy = 0;
                    case6s.erase(it);
                    problem = true;
                }
                else{
                    //not found, we go up, and add to case9s;
                    stepx = 1;
                    stepy = 0;
                    case6s.push_back(i);
                }
                break;
            default:
                qDebug() << "this shouldn't happen.";
        }
        //little optimization
        // if previous direction is same as current direction,
        // then we should modify the last vec to current
        curx += stepx;
        cury += stepy;
        if(stepx == prevx && stepy == prevy) {
            _points.back().setX(curx-rect.left());
            _points.back().setY(rect.size().height() - cury + rect.top());
        } else if(problem) {
            //TODO: we triangulation cannot work collinear points, so we need to modify same point a little
            //TODO: maybe we can detect if we go into a hole and coming back the hole, we should extract those points and remove them
            _points.push_back(QPointF(curx - rect.left(), rect.size().height() - cury + rect.top()));
        } else {
            _points.push_back(QPointF(curx - rect.left(), rect.size().height() - cury + rect.top()));
        }

        count++;
        prevx = stepx;
        prevy = stepy;
        problem = false;
        Q_ASSERT_X(count <= totalPixel, "oh no, marching square cannot find starting position", "");
    } while(curx != startx || cury != starty);
    return _points;
}

float PolygonImage::perpendicularDistance(const QPointF& i, const QPointF& start, const QPointF& end) {
    float res;
    float slope;
    float intercept;

    if(start.x() == end.x()) {
        res = abs(i.x() - end.x());
    } else if (start.y() == end.y()) {
        res = abs(i.y() - end.y());
    } else {
        slope = (end.y() - start.y()) / (end.x() - start.x());
        intercept = start.y() - (slope * start.x());
        res = fabsf(slope * i.x() - i.y() + intercept) / sqrtf(powf(slope, 2) + 1);
    }
    return res;
}

std::vector<QPointF> PolygonImage::rdp(std::vector<QPointF> v, const float& optimization) {
    if(v.size() < 3)
        return v;

    int index = -1;
    float dist = 0;
    //not looping first and last point
    for(size_t i = 1; i < v.size()-1; i++)
    {
        float cdist = perpendicularDistance(v[i], v.front(), v.back());
        if(cdist > dist)
        {
            dist = cdist;
            index = i;
        }
    }
    if (dist>optimization)
    {
        std::vector<QPointF>::const_iterator begin = v.begin();
        std::vector<QPointF>::const_iterator end   = v.end();
        std::vector<QPointF> l1(begin, begin+index+1);
        std::vector<QPointF> l2(begin+index, end);

        std::vector<QPointF> r1 = rdp(l1, optimization);
        std::vector<QPointF> r2 = rdp(l2, optimization);

        r1.insert(r1.end(), r2.begin()+1, r2.end());
        return r1;
    }
    else {
        std::vector<QPointF> ret;
        ret.push_back(v.front());
        ret.push_back(v.back());
        return ret;
    }
}

std::vector<QPointF> PolygonImage::reduce(const std::vector<QPointF>& points, const QRectF& rect , const float& epsilon) {
    auto size = points.size();
    // if there are less than 3 points, then we have nothing
    if (size<3) {
        qDebug("AUTOPOLYGON: cannot reduce points for %s that has less than 3 points in input, e: %f", _filename.c_str(), epsilon);
        return std::vector<QPointF>();
    }
    // if there are less than 9 points (but more than 3), then we don't need to reduce it
    else if (size < 9) {
        qDebug("AUTOPOLYGON: cannot reduce points for %s e: %f", _filename.c_str(), epsilon);
        return points;
    }

    float maxEp = qMin(rect.size().width(), rect.size().height());
    float ep = clampf(epsilon, 0.0f, maxEp / 2);
    std::vector<QPointF> result = rdp(points, ep);

    auto last = result.back();

    QPointF pointDistance = last - result.front();

    if (last.y() > result.front().y() && pointDistance.manhattanLength() < ep * 0.5f) {
        result.front().setY(last.y());
        result.pop_back();
    }
    return result;
}

std::vector<QPointF> PolygonImage::expand(const std::vector<QPointF>& points, const QRectF &rect, const float& epsilon) {
    auto size = points.size();
    // if there are less than 3 points, then we have nothing
    if(size < 3) {
        qDebug("AUTOPOLYGON: cannot expand points for %s with less than 3 points, e: %f", _filename.c_str(), epsilon);
        return std::vector<QPointF>();
    }
    ClipperLib::Path subj;
    ClipperLib::PolyTree solution;
    ClipperLib::PolyTree out;
    for(std::vector<QPointF>::const_iterator it = points.begin(); it<points.end(); it++) {
        subj << ClipperLib::IntPoint(it->x()* PRECISION, it->y() * PRECISION);
    }
    ClipperLib::ClipperOffset co;
    co.AddPath(subj, ClipperLib::jtMiter, ClipperLib::etClosedPolygon);
    co.Execute(solution, epsilon * PRECISION);

    ClipperLib::PolyNode* p = solution.GetFirst();
    if(!p) {
        qDebug("AUTOPOLYGON: Clipper failed to expand the points");
        return points;
    }
    while(p->IsHole()){
        p = p->GetNext();
    }

    //turn the result into simply polygon (AKA, fix overlap)

    //clamp into the specified rect
    ClipperLib::Clipper cl;
    cl.StrictlySimple(true);
    cl.AddPath(p->Contour, ClipperLib::ptSubject, true);
    //create the clipping rect
    ClipperLib::Path clamp;
    clamp.push_back(ClipperLib::IntPoint(0, 0));
    clamp.push_back(ClipperLib::IntPoint(rect.size().width() * PRECISION, 0));
    clamp.push_back(ClipperLib::IntPoint(rect.size().width() * PRECISION, rect.size().height() * PRECISION));
    clamp.push_back(ClipperLib::IntPoint(0, rect.size().height() * PRECISION));
    cl.AddPath(clamp, ClipperLib::ptClip, true);
    cl.Execute(ClipperLib::ctIntersection, out);

    std::vector<QPointF> outPoints;
    ClipperLib::PolyNode* p2 = out.GetFirst();
    while(p2->IsHole()){
        p2 = p2->GetNext();
    }
    auto end = p2->Contour.end();
    for(std::vector<ClipperLib::IntPoint>::const_iterator pt = p2->Contour.begin(); pt < end; pt++)
    {
        outPoints.push_back(QPointF(pt->X/PRECISION, pt->Y/PRECISION));
    }
    return outPoints;
}

Triangles PolygonImage::triangulate(const std::vector<QPointF>& points) {
    // if there are less than 3 points, then we can't triangulate
    if(points.size()<3)
    {
        qDebug("AUTOPOLYGON: cannot triangulate %s with less than 3 points", _filename.c_str());
        return Triangles();
    }
    std::vector<p2t::Point*> p2points;
    for(std::vector<QPointF>::const_iterator it = points.begin(); it<points.end(); it++)
    {
        p2t::Point * p = new p2t::Point(it->x(), it->y());
        p2points.push_back(p);
    }
    qDebug() << "!!!!!!!" << points.size();

    p2t::CDT cdt(p2points);
    cdt.Triangulate();
    std::vector<p2t::Triangle*> tris = cdt.GetTriangles();

    Triangles triangles;

    triangles.verts.resize(points.size());
    triangles.indices.resize(tris.size()*3);
    unsigned short idx = 0;
    unsigned short vdx = 0;

    for(std::vector<p2t::Triangle*>::const_iterator ite = tris.begin(); ite < tris.end(); ite++) {
        for(int i = 0; i < 3; i++) {
            auto p = (*ite)->GetPoint(i);
            auto v2 = QPointF(p->x, p->y);
            bool found = false;
            size_t j;
            size_t length = vdx;
            for(j = 0; j < length; j++) {
                if(triangles.verts[j].v == v2) {
                    found = true;
                    break;
                }
            }
            if(found) {
                //if we found the same vertex, don't add to verts, but use the same vertex with indices
                triangles.indices[idx] = j;
                idx++;
            } else {
                //vert does not exist yet, so we need to create a new one,
                auto t2f = QPointF(0,0); // don't worry about tex coords now, we calculate that later
                V2F_T2F vert = {v2, t2f};
                triangles.verts[vdx] = vert;
                triangles.indices[idx] = vdx;
                idx++;
                vdx++;
            }
        }
    }
    for(auto j : p2points)
    {
        delete j;
    };
    return triangles;
}

void PolygonImage::calculateUV(const QRectF& rect, V2F_T2F* verts, const size_t& count) {
    /*
     whole texture UV coordination
     0,0                  1,0
     +---------------------+
     |                     |0.1
     |                     |0.2
     |     +--------+      |0.3
     |     |texRect |      |0.4
     |     |        |      |0.5
     |     |        |      |0.6
     |     +--------+      |0.7
     |                     |0.8
     |                     |0.9
     +---------------------+
     0,1                  1,1
     */

    Q_ASSERT_X(_width && _height, "please specify width and height for this AutoPolygon instance", "");
    float texWidth  = _width;
    float texHeight = _height;

    auto end = &verts[count];
    for(auto i = verts; i != end; i++) {
        // for every point, offset with the center point
        float u = (i->v.x() + rect.left()) / texWidth;
        float v = (rect.top()+rect.size().height() - i->v.y()) / texHeight;
        i->t.setX(u);
        i->t.setY(v);
    }
}

Triangles PolygonImage::generateTriangles(const QImage& image, const QRectF& rect, const float epsilon, const float threshold) {
    PolygonImage polygonImage(image, epsilon, threshold);
    QRectF realRect = rect;

    std::vector<QPointF> p;

    p = polygonImage.traceAll(realRect, threshold);
    //p = polygonImage.reduce(p, realRect, epsilon);
    p = polygonImage.expand(p, realRect, epsilon);

    auto tri = polygonImage.triangulate(p);

    return tri;
}