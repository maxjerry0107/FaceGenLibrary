//
// Coypright (c) 2022 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//

#include "stdafx.h"

#include "FgSimilarity.hpp"
#include "FgMatrixSolver.hpp"
#include "FgBounds.hpp"
#include "FgApproxEqual.hpp"
#include "FgMain.hpp"
#include "FgCommand.hpp"

using namespace std;

namespace Fg {

ostream &           operator<<(ostream & os,Rigid3D const & r)
{
    return os << "rot: " << r.rot << " trans: " << r.trans;
}

// SimilarityRD: v' = sR(v + t) = sRv + sRt
SimilarityD::SimilarityD(SimilarityRD const & s) : scale{s.scale}, rot{s.rot}, trans{s.rot*s.trans*s.scale}
{}

SimilarityD         SimilarityD::operator*(SimilarityD const & rhs) const
{
    // Transform:   sRv+t
    // Composition: s'R'(sRv+t)+t'
    //            = s'R'sR(v) + (s'R't+t')
    SimilarityD    ret;
    ret.scale = scale * rhs.scale;
    ret.rot = rot * rhs.rot;
    ret.trans = scale * (rot * rhs.trans) + trans;
    return ret;
}

SimilarityD         SimilarityD::inverse() const
{
    // v' = sRv+t
    // sRv = v' - t
    // v = s'R'v' - s'R't
    double              s = 1.0 / scale;
    QuaternionD         r = rot.inverse();
    Vec3D               t = r * trans * s;
    return SimilarityD {s,r,-t};
}

SimilarityD         similarityRand()
{
    return
        SimilarityD(
            std::exp(randNormal()),
            QuaternionD::rand(),
            Vec3D::randNormal());
}

SimilarityD         solveSimilarity(Vec3Ds const & domain,Vec3Ds const & range)
{
    size_t              V = domain.size();
    FGASSERT(V > 2);                                // not solvable with only 2 points.
    FGASSERT(range.size() == V);                    // must be 1-1
    Vec3D               domMean = cMean(domain),
                        ranMean = cMean(range);
    double              domRayMag {0},
                        ranRayMag {0};
    Mat33D              S {0};
    for (size_t ii=0; ii<V; ii++) {
        Vec3D               domRay = domain[ii] - domMean,
                            ranRay = range[ii] - ranMean;
        domRayMag += domRay.mag();
        ranRayMag += ranRay.mag();
        S += domRay * ranRay.transpose();
    }
    double              scale = sqrt(ranRayMag / domRayMag );
    Mat44D              N {0};
    double              Sxx = S.cr(0,0),   Sxy = S.cr(1,0),   Sxz = S.cr(2,0),
                        Syx = S.cr(0,1),   Syy = S.cr(1,1),   Syz = S.cr(2,1),
                        Szx = S.cr(0,2),   Szy = S.cr(1,2),   Szz = S.cr(2,2);
    // Set the upper triangular elements of N not including the diagonal:
    N.cr(1,0)=Syz-Szy;          N.cr(2,0)=Szx-Sxz;      N.cr(3,0)=Sxy-Syx;
                                N.cr(2,1)=Sxy+Syx;      N.cr(3,1)=Szx+Sxz;
                                                        N.cr(3,2)=Syz+Szy;
    // Since it's symmetric, set the lower triangular (not including diagonal) by:
    N += N.transpose();
    // And set the diagonal elements:
    N.cr(0,0) = Sxx+Syy+Szz;
    N.cr(1,1) = Sxx-Syy-Szz;
    N.cr(2,2) = Syy-Sxx-Szz;
    N.cr(3,3) = Szz-Sxx-Syy;
    // Calculate rotation from N per [Jain '95]. 'cEigsRsm' Leaves largest eigVal in last index:
    QuaternionD         pose {cEigsRsm(N).vecs.colVec(3)};
    // Calculate the 'trans' term: The transform is given by:
    // X = SR(d-dm)+rm = SR(d)-SR(dm)+rm
    Vec3D               trans = -scale * (pose.asMatrix() * domMean) + ranMean;
    return {scale,pose,trans};
}
SimilarityD         solveSimilarity(Vec3Fs const & d,Vec3Fs const & r)
{
    return solveSimilarity(deepCast<double>(d),deepCast<double>(r));
}

// as above but for count-weighted samples:
SimilarityD         solveSimilarity(Vec3Ds const & domain,Vec3Ds const & range,Doubles weights)
{
    size_t              V = domain.size();
    FGASSERT(V > 2);                                // not solvable with only 2 points.
    FGASSERT(range.size() == V);                    // must be 1-1
    FGASSERT(weights.size() == V);                  // must be 1-1
    double              wgtTot = cSum(weights);
    Vec3D               domAcc {0},
                        ranAcc {0};
    for (size_t ii=0; ii<V; ++ii) {
        domAcc += domain[ii] * weights[ii];
        ranAcc += range[ii] * weights[ii];
    }
    Vec3D               domMean = domAcc / wgtTot,
                        ranMean = ranAcc / wgtTot;
    double              domRayMag {0},
                        ranRayMag {0};
    Mat33D              S {0};
    for (size_t ii=0; ii<V; ii++) {
        Vec3D               domRay = domain[ii] - domMean,
                            ranRay = range[ii] - ranMean;
        double              wgt = weights[ii];
        domRayMag += domRay.mag() * wgt;
        ranRayMag += ranRay.mag() * wgt;
        S += domRay * ranRay.transpose() * wgt;
    }
    double              scale = sqrt(ranRayMag / domRayMag );
    Mat44D              N {0};
    double              Sxx = S.cr(0,0),   Sxy = S.cr(1,0),   Sxz = S.cr(2,0),
                        Syx = S.cr(0,1),   Syy = S.cr(1,1),   Syz = S.cr(2,1),
                        Szx = S.cr(0,2),   Szy = S.cr(1,2),   Szz = S.cr(2,2);
    N.cr(1,0)=Syz-Szy;          N.cr(2,0)=Szx-Sxz;      N.cr(3,0)=Sxy-Syx;
                                N.cr(2,1)=Sxy+Syx;      N.cr(3,1)=Szx+Sxz;
                                                        N.cr(3,2)=Syz+Szy;
    N += N.transpose();
    N.cr(0,0) = Sxx+Syy+Szz;
    N.cr(1,1) = Sxx-Syy-Szz;
    N.cr(2,2) = Syy-Sxx-Szz;
    N.cr(3,3) = Szz-Sxx-Syy;
    QuaternionD         pose {cEigsRsm(N).vecs.colVec(3)};
    Vec3D               trans = -scale * (pose.asMatrix() * domMean) + ranMean;
    return {scale,pose,trans};
}

SimilarityD         interpolateAsModelview(SimilarityD s0,SimilarityD s1,double val)
{
    // Scale is also affected by depth so we must interpolate that in log space, and it turns out
    // we need to use that same profile for the other translations to keep linear looking motion:
    double          z0 = s0.trans[2],
                    z1 = s1.trans[2],
                    sign = z0 / abs(z0);
    FGASSERT(z0*z1>0.0);        // Ensure same sign and no zero - can't interpolate behind camera
    double          zl0 = log(abs(s0.trans[2])),
                    zl1 = log(abs(s1.trans[2])),
                    zval = exp(interpolate(zl0,zl1,val)) * sign,
                    lval = (zval-z0) / (z1-z0);
    return SimilarityD {
        exp(interpolate(log(s0.scale),log(s1.scale),val)),
        interpolate(s0.rot,s1.rot,val),
        interpolate(s0.trans,s1.trans,lval)
    };
}

Affine3D            SimilarityRD::asAffine() const
{
    // Since 'Affine' applies translation second: v' = sR(v+t) = sRv + sRt
    Affine3D      ret;
    ret.linear = rot.asMatrix() * scale;
    ret.translation = ret.linear * trans;
    return ret;
}

SimilarityRD        SimilarityRD::inverse() const
{
    // v' = sR(v+t)
    // v' = sRv + sRt
    // sRv = v' - sRt
    // v = s'R'(v' - sRt)
    Vec3D           t = rot * trans * scale;
    return SimilarityRD {-t,rot.inverse(),1.0/scale};
}

std::ostream &      operator<<(std::ostream & os,SimilarityRD const & v)
{
    return os
        << "Translation: " << v.trans
        << " Scale: " << v.scale
        << " Rotation: " << v.rot;
}

bool                isApproxEqual(SimilarityD const & l,SimilarityD const & r,double prec)
{
    return (
        isApproxEqual(l.scale,r.scale,prec) &&
        isApproxEqual(l.rot,r.rot,prec) &&
        isApproxEqual(l.trans,r.trans,prec)
    );
}

namespace {

SimilarityD         rand()
{
    double              scale = exp(randNormal());
    QuaternionD         rot = QuaternionD::rand();
    Vec3D               trans = Vec3D::randNormal();
    return {scale,rot,trans};
}

void                testStruct(CLArgs const &)
{
    randSeedRepeatable();
    double constexpr    prec = lims<double>::epsilon() * 8;
    for (size_t ii=0; ii<10; ++ii) {
        SimilarityD         sim = rand(),
                            inv = sim.inverse(),
                            id = sim * inv;
        FGASSERT(isApproxEqual(id.scale,1.0,prec));
        FGASSERT(isApproxEqual(id.rot,QuaternionD{},prec));
        FGASSERT(isApproxEqual(id.trans,Vec3D{0},prec));
    }
}

void                testSolve(CLArgs const &)
{
    randSeedRepeatable();
    // exact test:
    for (size_t ii=0; ii<5; ++ii) {
        double const        prec = epsBits(40);
        size_t              V = 3;
        for (size_t jj=0; jj<10; ++jj) {
            Vec3Ds              domain = randVecNormals<double,3>(V,1.0);
            SimilarityD         simRef = rand();
            Vec3Ds              range = mapMul(simRef.asAffine(),domain);
            {   // unweighted:
                SimilarityD         simTst = solveSimilarity(domain,range);
                FGASSERT(isApproxEqual(simTst,simRef,prec));
            }
            if (V > 3) {       // weighted can have low precision with only 3 points:
                Doubles             weights = generateSvec<double>(V,[](size_t){return sqr(randNormal()); });
                SimilarityD         simTst = solveSimilarity(domain,range,weights);
                FGASSERT(isApproxEqual(simTst,simRef,prec));
            }
        }
        V *= 2;
    }
    // test w/ noise:
    for (size_t ii=0; ii<5; ++ii) {
        double const        prec = epsBits(20);
        size_t              V = 2ULL << 16;
        SimilarityD         simRef = rand();
        Vec3Ds              domain = randVecNormals<double,3>(V,1.0),
                            noise = randVecNormals<double,3>(V,0.01),
                            range = mapMul(simRef,domain);
        {   // unweighted:
            SimilarityD         simTst = solveSimilarity(domain,range);
            FGASSERT(isApproxEqual(simTst,simRef,prec));
        }
        {   // weighted:
            Doubles             weights = generateSvec<double>(V,[](size_t){return sqr(randNormal()); });
            SimilarityD         simTst = solveSimilarity(domain,range,weights);
            FGASSERT(isApproxEqual(simTst,simRef,prec));
        }
    }
}

}

void                testSimilarity(CLArgs const & args)
{
    Cmds            cmds {
        {testStruct,"sim","similarity transform structure"},
        {testSolve,"solve","similarity transform structure"},
    };
    doMenu(args,cmds,true);
}

}

// */
