/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkPath.h"
#include "include/core/SkPathTypes.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTypes.h"
#include "modules/pathops/src/SkPathOpsBounds.h"
#include "modules/pathops/src/SkPathOpsConic.h"
#include "modules/pathops/src/SkPathOpsCubic.h"
#include "modules/pathops/src/SkPathOpsLine.h"
#include "modules/pathops/src/SkPathOpsQuad.h"
#include "modules/pathops/src/SkPathOpsRect.h"
#include "modules/pathops/src/SkPathOpsTSect.h"
#include "modules/pathops/src/SkPathOpsTypes.h"
#include "modules/pathops/src/SkReduceOrder.h"
#include "src/base/SkTSort.h"
#include "src/core/SkPathPriv.h"
#include "tests/PathOpsTestCommon.h"

#include <cmath>
#include <cstring>
#include <utility>

using namespace skia_private;

static double calc_t_div(const SkDCubic& cubic, double precision, double start) {
    const double adjust = sqrt(3.) / 36;
    SkDCubic sub;
    const SkDCubic* cPtr;
    if (start == 0) {
        cPtr = &cubic;
    } else {
        // OPTIMIZE: special-case half-split ?
        sub = cubic.subDivide(start, 1);
        cPtr = &sub;
    }
    const SkDCubic& c = *cPtr;
    double dx = c[3].fX - 3 * (c[2].fX - c[1].fX) - c[0].fX;
    double dy = c[3].fY - 3 * (c[2].fY - c[1].fY) - c[0].fY;
    double dist = sqrt(dx * dx + dy * dy);
    double tDiv3 = precision / (adjust * dist);
    double t = std::cbrt(tDiv3);
    if (start > 0) {
        t = start + (1 - start) * t;
    }
    return t;
}

static bool add_simple_ts(const SkDCubic& cubic, double precision, TArray<double, true>* ts) {
    double tDiv = calc_t_div(cubic, precision, 0);
    if (tDiv >= 1) {
        return true;
    }
    if (tDiv >= 0.5) {
        ts->push_back(0.5);
        return true;
    }
    return false;
}

static void addTs(const SkDCubic& cubic, double precision, double start, double end,
        TArray<double, true>* ts) {
    double tDiv = calc_t_div(cubic, precision, 0);
    double parts = ceil(1.0 / tDiv);
    for (double index = 0; index < parts; ++index) {
        double newT = start + (index / parts) * (end - start);
        if (newT > 0 && newT < 1) {
            ts->push_back(newT);
        }
    }
}

static void toQuadraticTs(const SkDCubic* cubic, double precision, TArray<double, true>* ts) {
    SkReduceOrder reducer;
    int order = reducer.reduce(*cubic, SkReduceOrder::kAllow_Quadratics);
    if (order < 3) {
        return;
    }
    double inflectT[5];
    int inflections = cubic->findInflections(inflectT);
    SkASSERT(inflections <= 2);
    if (!cubic->endsAreExtremaInXOrY()) {
        inflections += cubic->findMaxCurvature(&inflectT[inflections]);
        SkASSERT(inflections <= 5);
    }
    SkTQSort<double>(inflectT, inflectT + inflections);
    // OPTIMIZATION: is this filtering common enough that it needs to be pulled out into its
    // own subroutine?
    while (inflections && approximately_less_than_zero(inflectT[0])) {
        memmove(inflectT, &inflectT[1], sizeof(inflectT[0]) * --inflections);
    }
    int start = 0;
    int next = 1;
    while (next < inflections) {
        if (!approximately_equal(inflectT[start], inflectT[next])) {
            ++start;
        ++next;
            continue;
        }
        memmove(&inflectT[start], &inflectT[next], sizeof(inflectT[0]) * (--inflections - start));
    }

    while (inflections && approximately_greater_than_one(inflectT[inflections - 1])) {
        --inflections;
    }
    SkDCubicPair pair;
    if (inflections == 1) {
        pair = cubic->chopAt(inflectT[0]);
        int orderP1 = reducer.reduce(pair.first(), SkReduceOrder::kNo_Quadratics);
        if (orderP1 < 2) {
            --inflections;
        } else {
            int orderP2 = reducer.reduce(pair.second(), SkReduceOrder::kNo_Quadratics);
            if (orderP2 < 2) {
                --inflections;
            }
        }
    }
    if (inflections == 0 && add_simple_ts(*cubic, precision, ts)) {
        return;
    }
    if (inflections == 1) {
        pair = cubic->chopAt(inflectT[0]);
        addTs(pair.first(), precision, 0, inflectT[0], ts);
        addTs(pair.second(), precision, inflectT[0], 1, ts);
        return;
    }
    if (inflections > 1) {
        SkDCubic part = cubic->subDivide(0, inflectT[0]);
        addTs(part, precision, 0, inflectT[0], ts);
        int last = inflections - 1;
        for (int idx = 0; idx < last; ++idx) {
            part = cubic->subDivide(inflectT[idx], inflectT[idx + 1]);
            addTs(part, precision, inflectT[idx], inflectT[idx + 1], ts);
        }
        part = cubic->subDivide(inflectT[last], 1);
        addTs(part, precision, inflectT[last], 1, ts);
        return;
    }
    addTs(*cubic, precision, 0, 1, ts);
}

void CubicToQuads(const SkDCubic& cubic, double precision, TArray<SkDQuad, true>& quads) {
    TArray<double, true> ts;
    toQuadraticTs(&cubic, precision, &ts);
    if (ts.empty()) {
        SkDQuad quad = cubic.toQuad();
        quads.push_back(quad);
        return;
    }
    double tStart = 0;
    for (int i1 = 0; i1 <= ts.size(); ++i1) {
        const double tEnd = i1 < ts.size() ? ts[i1] : 1;
        SkDRect bounds;
        bounds.setBounds(cubic);
        SkDCubic part = cubic.subDivide(tStart, tEnd);
        SkDQuad quad = part.toQuad();
        if (quad[1].fX < bounds.fLeft) {
            quad[1].fX = bounds.fLeft;
        } else if (quad[1].fX > bounds.fRight) {
            quad[1].fX = bounds.fRight;
        }
        if (quad[1].fY < bounds.fTop) {
            quad[1].fY = bounds.fTop;
        } else if (quad[1].fY > bounds.fBottom) {
            quad[1].fY = bounds.fBottom;
        }
        quads.push_back(quad);
        tStart = tEnd;
    }
}

void CubicPathToQuads(const SkPath& cubicPath, SkPath* quadPath) {
    quadPath->reset();
    SkDCubic cubic;
    TArray<SkDQuad, true> quads;
    for (auto [verb, pts, w] : SkPathPriv::Iterate(cubicPath)) {
        switch (verb) {
            case SkPathVerb::kMove:
                quadPath->moveTo(pts[0].fX, pts[0].fY);
                continue;
            case SkPathVerb::kLine:
                quadPath->lineTo(pts[1].fX, pts[1].fY);
                break;
            case SkPathVerb::kQuad:
                quadPath->quadTo(pts[1].fX, pts[1].fY, pts[2].fX, pts[2].fY);
                break;
            case SkPathVerb::kCubic:
                quads.clear();
                cubic.set(pts);
                CubicToQuads(cubic, cubic.calcPrecision(), quads);
                for (int index = 0; index < quads.size(); ++index) {
                    SkPoint qPts[2] = {
                        quads[index][1].asSkPoint(),
                        quads[index][2].asSkPoint()
                    };
                    quadPath->quadTo(qPts[0].fX, qPts[0].fY, qPts[1].fX, qPts[1].fY);
                }
                break;
            case SkPathVerb::kClose:
                 quadPath->close();
                break;
            default:
                SkDEBUGFAIL("bad verb");
                return;
        }
    }
}

void CubicPathToSimple(const SkPath& cubicPath, SkPath* simplePath) {
    simplePath->reset();
    SkDCubic cubic;
    for (auto [verb, pts, w] : SkPathPriv::Iterate(cubicPath)) {
        switch (verb) {
            case SkPathVerb::kMove:
                simplePath->moveTo(pts[0].fX, pts[0].fY);
                continue;
            case SkPathVerb::kLine:
                simplePath->lineTo(pts[1].fX, pts[1].fY);
                break;
            case SkPathVerb::kQuad:
                simplePath->quadTo(pts[1].fX, pts[1].fY, pts[2].fX, pts[2].fY);
                break;
            case SkPathVerb::kCubic: {
                cubic.set(pts);
                double tInflects[2];
                int inflections = cubic.findInflections(tInflects);
                if (inflections > 1 && tInflects[0] > tInflects[1]) {
                    using std::swap;
                    swap(tInflects[0], tInflects[1]);
                }
                double lo = 0;
                for (int index = 0; index <= inflections; ++index) {
                    double hi = index < inflections ? tInflects[index] : 1;
                    SkDCubic part = cubic.subDivide(lo, hi);
                    SkPoint cPts[3];
                    cPts[0] = part[1].asSkPoint();
                    cPts[1] = part[2].asSkPoint();
                    cPts[2] = part[3].asSkPoint();
                    simplePath->cubicTo(cPts[0].fX, cPts[0].fY, cPts[1].fX, cPts[1].fY,
                            cPts[2].fX, cPts[2].fY);
                    lo = hi;
                }
                break;
            }
            case SkPathVerb::kClose:
                 simplePath->close();
                break;
            default:
                SkDEBUGFAIL("bad verb");
                return;
        }
    }
}

bool ValidBounds(const SkPathOpsBounds& bounds) {
    if (SkIsNaN(bounds.fLeft)) {
        return false;
    }
    if (SkIsNaN(bounds.fTop)) {
        return false;
    }
    if (SkIsNaN(bounds.fRight)) {
        return false;
    }
    return !SkIsNaN(bounds.fBottom);
}

bool ValidConic(const SkDConic& conic) {
    for (int index = 0; index < SkDConic::kPointCount; ++index) {
        if (!ValidPoint(conic[index])) {
            return false;
        }
    }
    if (SkIsNaN(conic.fWeight)) {
        return false;
    }
    return true;
}

bool ValidCubic(const SkDCubic& cubic) {
    for (int index = 0; index < 4; ++index) {
        if (!ValidPoint(cubic[index])) {
            return false;
        }
    }
    return true;
}

bool ValidLine(const SkDLine& line) {
    for (int index = 0; index < 2; ++index) {
        if (!ValidPoint(line[index])) {
            return false;
        }
    }
    return true;
}

bool ValidPoint(const SkDPoint& pt) {
    if (SkIsNaN(pt.fX)) {
        return false;
    }
    return !SkIsNaN(pt.fY);
}

bool ValidPoints(const SkPoint* pts, int count) {
    for (int index = 0; index < count; ++index) {
        if (SkIsNaN(pts[index].fX)) {
            return false;
        }
        if (SkIsNaN(pts[index].fY)) {
            return false;
        }
    }
    return true;
}

bool ValidQuad(const SkDQuad& quad) {
    for (int index = 0; index < 3; ++index) {
        if (!ValidPoint(quad[index])) {
            return false;
        }
    }
    return true;
}

bool ValidVector(const SkDVector& v) {
    if (SkIsNaN(v.fX)) {
        return false;
    }
    return !SkIsNaN(v.fY);
}
