//
// Coypright (c) 2022 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//

#include "stdafx.h"

#include "FgImage.hpp"
#include "FgTime.hpp"
#include "FgImgDisplay.hpp"
#include "FgImage.hpp"
#include "FgTestUtils.hpp"
#include "FgApproxEqual.hpp"
#include "FgCommand.hpp"
#include "FgSyntax.hpp"

using namespace std;

namespace Fg {

namespace {

void                testmCompare(CLArgs const &)
{
    String8             dd = dataDir();
    Img3F               img0 = toUnit3F(loadImage(dd+"base/Mandrill512.png")),
                        img1 = toUnit3F(loadImage(dd+"base/test/imgops/composite.png"));
    compareImages(img0,img1);
}

void                testmDisplay(CLArgs const &)
{
    String8     dd = dataDir();
    string      testorig_jpg("base/test/testorig.jpg");
    ImgRgba8     img = loadImage(dd+testorig_jpg);
    viewImage(img);
    string      testorig_bmp("base/test/testorig.bmp");
    loadImage_(dd+testorig_bmp,img);
    viewImage(img);
}

void                testmResize(CLArgs const &)
{
    string          fname("base/test/testorig.jpg");
    ImgRgba8         img = loadImage(dataDir()+fname);
    ImgRgba8         out(img.width()/2+1,img.height()+1);
    imgResize(img,out);
    viewImage(out);
}

void                resamp(CLArgs const &)
{
    Img3F                   in = toUnit3F(loadImage(dataDir()+"base/Mandrill512.png"));
    Vec2D                   lo {94.3,37.8};
    for (uint lnSz = 5; lnSz < 10; ++lnSz) {
        uint                    sz = 2 << lnSz;
        Img3F                   out = resampleAdaptive(in,lo,309.7f,sz);
        viewImageFixed(toRgba8(out));
    }
}

void                testmSfs(CLArgs const &)
{
    Img3F               img = toUnit3F(loadImage(dataDir()+"base/Mandrill512.png"));
    {
        PushTimer           timer {"floating point image smooth"};
        for (uint ii=0; ii<64; ++ii)
            smoothFloat_(img,img,1);
    }
    viewImage(toRgba8(img));
}

void                testBlerp(CLArgs const &)
{
    {
        ImgF            empty;
        Blerp           blerp {{0,0},empty.dims()};
        FGASSERT(!blerp.valid());
    }
    {
        ImgF            two {2,2,{
            1.0f,   2.0f,
            4.0f,   8.0f
        }};
        auto            fn = [&two](Vec2F ircs,float resultZero,float resultClamp)
        {
            {
                Blerp           blerp {ircs,two.dims()};
                float           val = blerp.sample(two).wval;
                FGASSERT(val == resultZero);
            }
            {
                BlerpClamp      blerp {ircs,two.dims()};
                float           val = blerp.sample(two);
                FGASSERT(val == resultClamp);
            }
        };
        fn({-1,-1},     0,1);
        fn({-1,0},      0,1);
        fn({0,-1},      0,1);
        fn({-0.5,0},    0.5,1);
        fn({0,-0.5},    0.5,1);
        fn({0,0},       1,1);
        fn({0.5,0},     1.5f,1.5f);
        fn({0.25,0},    1.25f,1.25f);
        fn({0.75,0.25}, 3.0625f,3.0625f);
        fn({2,0},       0,2);
        fn({0,2},       0,4);
        fn({2,2},       0,8);
    }
}

void                testComposite(CLArgs const &)
{
    String8             dd = dataDir();
    ImgRgba8            overlay = loadImage(dd+"base/Teeth512.png"),
                        base = loadImage(dd+"base/Mandrill512.png");
    regressTest(composite(overlay,base),dd+"base/test/imgops/composite.png");
}

void                testConvolve(CLArgs const &)
{
    randSeedRepeatable();
    ImgF          tst(16,16);
    for (size_t ii=0; ii<tst.numPixels(); ++ii)
        tst[ii] = float(randUniform());
    ImgF          i0,i1;
    smoothFloat_(tst,i0,1);
    fgConvolveFloat(tst,Mat33F(1,2,1,2,4,2,1,2,1)/16.0f,i1,1);
    //fgout << fgnl << i0.m_data << fgnl << i1.m_data;
    FGASSERT(isApproxEqualRelMag(i0.m_data,i1.m_data));
}

void                testDecodeJpeg(CLArgs const &)
{
    String8             imgFile = dataDir() + "base/trees.jpg";
    String              blob = loadRaw(imgFile);
    Uchars              ub; ub.reserve(blob.size());
    for (char ch : blob)
        ub.push_back(scast<uchar>(ch));
    ImgRgba8            tst = decodeJpeg(ub),
                        ref = loadImage(imgFile);
    FGASSERT(isApproxEqual(tst,ref,3U));
}

void                testTransform(CLArgs const &)
{
    AffineEw2F          identity;
    Vec2UIs             dimss {{16,32},{19,47}};
    float               maxDiff = scast<float>(epsBits(20));
    for (Vec2UI dims : dimss) {
        AffineEw2F          xf0 = cIucsToIrcsXf(dims),
                            xf0i = xf0.inverse(),
                            xf1 = cIrcsToIucsXf(dims),
                            xf1i = xf1.inverse();
        FGASSERT(isApproxEqual(xf0*xf0i,identity,maxDiff));
        FGASSERT(isApproxEqual(xf0*xf1,identity,maxDiff));
        FGASSERT(isApproxEqual(xf1*xf1i,identity,maxDiff));
    }
}

}

void                testmImage(CLArgs const & args)
{
    Cmds                cmds {
        {testmCompare,"comp","view to compare images"},
        {testmDisplay,"disp","display a single image"},
        {resamp,"resamp","resample adaptive"},
        {testmResize,"resize","change image pixel size by resampling"},
        {testmSfs,"sfs","smoothFloat speed"},
    };
    doMenu(args,cmds);
}

void                testDecodeJfif(CLArgs const &);

void                testImage(CLArgs const & args)
{
    Cmds                cmds {
        {testBlerp,"blerp","bilinear interpolation"},
        {testComposite,"composite"},
        {testConvolve,"conv"},
        {testDecodeJfif,"jfif"},
        {testDecodeJpeg,"jpg"},
        {testTransform,"xf"},
    };
    doMenu(args,cmds,true,false,true);
}

}
