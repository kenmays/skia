/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkPathEffect_DEFINED
#define SkPathEffect_DEFINED

#include "include/core/SkFlattenable.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTypes.h"

// TODO(kjlubick) update clients and remove this unnecessary #include
#include "include/core/SkPath.h"  // IWYU pragma: keep

#include <cstddef>

class SkMatrix;
class SkPathBuilder;
class SkStrokeRec;
struct SkDeserialProcs;
struct SkRect;

/** \class SkPathEffect

    SkPathEffect is the base class for objects in the SkPaint that affect
    the geometry of a drawing primitive before it is transformed by the
    canvas' matrix and drawn.

    Dashing is implemented as a subclass of SkPathEffect.
*/
class SK_API SkPathEffect : public SkFlattenable {
public:
    /**
     *  Returns a patheffect that apples each effect (first and second) to the original path,
     *  and returns a path with the sum of these.
     *
     *  result = first(path) + second(path)
     *
     */
    static sk_sp<SkPathEffect> MakeSum(sk_sp<SkPathEffect> first, sk_sp<SkPathEffect> second);

    /**
     *  Returns a patheffect that applies the inner effect to the path, and then applies the
     *  outer effect to the result of the inner's.
     *
     *  result = outer(inner(path))
     */
    static sk_sp<SkPathEffect> MakeCompose(sk_sp<SkPathEffect> outer, sk_sp<SkPathEffect> inner);

    static SkFlattenable::Type GetFlattenableType() {
        return kSkPathEffect_Type;
    }

    /**
     *  Given a src path (input) and a stroke-rec (input and output), apply
     *  this effect to the src path, returning the new path in dst, and return
     *  true. If this effect cannot be applied, return false and ignore dst
     *  and stroke-rec.
     *
     *  The stroke-rec specifies the initial request for stroking (if any).
     *  The effect can treat this as input only, or it can choose to change
     *  the rec as well. For example, the effect can decide to change the
     *  stroke's width or join, or the effect can change the rec from stroke
     *  to fill (or fill to stroke) in addition to returning a new (dst) path.
     *
     *  If this method returns true, the caller will apply (as needed) the
     *  resulting stroke-rec to dst and then draw.
     */
    bool filterPath(SkPathBuilder* dst, const SkPath& src, SkStrokeRec*, const SkRect* cullR,
                    const SkMatrix& ctm) const;
    bool filterPath(SkPathBuilder* dst, const SkPath& src, SkStrokeRec*) const;

    /** True if this path effect requires a valid CTM */
    bool needsCTM() const;

    static sk_sp<SkPathEffect> Deserialize(const void* data, size_t size,
                                           const SkDeserialProcs* procs = nullptr);

#ifdef SK_SUPPORT_MUTABLE_PATHEFFECT
    bool filterPath(SkPath* dst, const SkPath& src, SkStrokeRec*, const SkRect* cullR) const;

    /** Version of filterPath that can be called when the CTM is known. */
    bool filterPath(SkPath* dst, const SkPath& src, SkStrokeRec*, const SkRect* cullR,
                    const SkMatrix& ctm) const;
#endif

private:
    SkPathEffect() = default;
    friend class SkPathEffectBase;

    using INHERITED = SkFlattenable;
};

#endif
