//
// Coypright (c) 2022 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//
// Polygonal mesh with multiple surfaces sharing a vertex list.
//
// DESIGN
//
// Tris and quads only.
//
// Rendering with different vertex position arrays is supported, in which case
// the member vertex array can be a base or reference shape.
//
// There is a one-to-many relationship between the vertex array and surfaces,
// since several smoothly connected surfaces may be segemented for different
// texture images.
//
// Floats are used for vertex positions due to size. Note that in a 1 metre 
// square volume, float precision gives at least 1 micron resolution. Since the thinnest human
// hairs are 17 microns (the thickest 180) float is adequate for most applications.
//

#ifndef FG3DMESH_HPP
#define FG3DMESH_HPP

#include "FgStdLibs.hpp"
#include "Fg3dSurface.hpp"
#include "FgSimilarity.hpp"
#include "FgCmp.hpp"

namespace Fg {

// Per-vertex morph, typically represented as deltas from base shape (aka blendshape):
struct  DirectMorph
{
    String8             name;
    Vec3Fs              verts;      // 1-1 correspondence with base verts

    DirectMorph() {}
    explicit DirectMorph(String8 const & n) : name(n) {}
    DirectMorph(String8 const & n,Vec3Fs const & v) : name(n), verts(v) {}

    inline void         accAsDelta_(float val,Vec3Fs & accVerts) const {mapMulAcc_(verts,val,accVerts); }
    bool                operator==(String8 const & n) const {return (name==n); }    // easy lookup
};
typedef Svec<DirectMorph>     DirectMorphs;

struct  IdxVec3F
{
    uint                idx;            // index into vertex list
    Vec3F               vec;            // delta or absolute position depending on use
    IdxVec3F() : idx{0}, vec{0} {}
    IdxVec3F(uint i,Vec3F const & v) : idx{i}, vec{v} {}
    FG_EQ_M2(IdxVec3F,idx,vec);
};
typedef Svec<IdxVec3F>  IdxVec3Fs;

IdxVec3Fs           offsetIndices(IdxVec3Fs const & ivs,uint offset);
Vec3Fs              indexedToCorrMorph(IdxVec3Fs const & morph,size_t baseSize);

// Indexed vertices morph representation is advantageous when only a fraction of the vertices are affected:
struct      IndexedMorph
{
    String8             name;
    IdxVec3Fs           ivs;        // element for each vertex which moves with this morph
    IndexedMorph() {}
    IndexedMorph(String8 const & n,IdxVec3Fs const & i) : name{n}, ivs{i} {}

    void                accAsTarget_(Vec3Fs const & baseVerts,float coeff,Vec3Fs & accVerts) const;
    bool                operator==(IndexedMorph const & rhs) const {return (ivs == rhs.ivs); }  // duplicate check
    bool                operator==(String8 const & n) const {return (name==n); }    // easy lookup by name
};
typedef Svec<IndexedMorph>  IndexedMorphs;

size_t              cNumVerts(IndexedMorphs const & ims);       // Number of target vertices in given indexed morphs
IndexedMorph        deltaToIndexedMorph(DirectMorph const & morph,float diffMagThreshold=0);
IndexedMorph        deltaToTargetMorph(Vec3Fs const & base,IndexedMorph const & morph);

void                accDeltaMorphs(
    DirectMorphs const &        deltaMorphs,
    Floats const &              coord,
    Vec3Fs &                    accVerts);      // MODIFIED: morphing delta accumualted here

// This version of target morph application is more suited to SSM dataflow, where the
// target positions have been transformed as part of the 'allVerts' array:
void                accTargetMorphs(
    Vec3Fs const &              allVerts,       // Base verts plus all target morph verts
    IndexedMorphs const &       targMorphs,     // Only 'baseInds' is used.
    Floats const &              coord,          // morph coefficient for each target morph
    Vec3Fs &                    accVerts);      // MODIFIED: target morphing delta accumulated here

// Like IndexedMorph we use a scattered data approach for skin weight storage, which can be turned into
// a per-vertex list for performance at runtime. The base xform skin weight is implicitly one minus the
// sum of these, lower-bounded by 0:
struct      SkinWeight
{
    uint                vertIdx;
    float               weight;
};
typedef Svec<SkinWeight>    SkinWeights;

// A joint is associated with one quaternion at animation time, so separately moving parts
// such as upper & lower eyelids must have separate joints (even though they share a position).
struct      Joint
{
    // The base pose is implicitly the identity rotation.
    String              name;
    uint                parentIdx;          // Parent part index starting at 1 (0 for base xform)
    // Don't keep this as idx into verts since it doesn't morph like verts:
    Vec3F               pos;
    SkinWeights         skin;               // If empty, applies to entire mesh
};
typedef Svec<Joint>      Joints;

struct      JointDof
{
    String              name;
    uint                jointIdx;           // Which joint ?
    Vec3F               rotAxis;            // unit length rotation axis
    Vec2F               angleBounds;        // in radians. Base pose implicitly zero.
};
typedef Svec<JointDof>      JointDofs;      // Applied in order given

struct      PoseDef
{
    String8             name;
    Vec2F               bounds;
    float               neutral;

    PoseDef() {}
    PoseDef(String8 const & n,Vec2F const & b,float u) : name(n), bounds(b), neutral(u) {}

    bool                operator==(String8 const & rhsName) const {return (name == rhsName); }
};
typedef Svec<PoseDef>          PoseDefs;

struct  MarkedVert
{
    uint                idx;
    String              label;

    MarkedVert() : idx {0} {}
    explicit MarkedVert(uint i) : idx(i) {}
    MarkedVert(uint i,String const & l) : idx(i), label(l) {}

    bool                operator==(uint rhs) const {return (idx == rhs); }
    bool                operator==(String const & rhs) const {return (label == rhs); }
};

typedef Svec<MarkedVert>    MarkedVerts;

struct  Mesh
{
    String8                 name;           // Optional. Not loaded/saved to file. Useful when passing mesh as arg
    Vec3Fs                  verts;          // Base shape
    Vec2Fs                  uvs;            // OTCS. Values outside [0,1) could mean wraparound but should be avoided
    Surfs                   surfaces;
    DirectMorphs            deltaMorphs;
    IndexedMorphs           targetMorphs;
    MarkedVerts             markedVerts;
    Joints                  joints;
    JointDofs               jointDofs;

    Mesh() {}
    explicit Mesh(Vec3Fs const & vts) : verts(vts) {}
    explicit Mesh(TriSurf const & ts) : verts{ts.verts}, surfaces{{Surf{ts.tris}}} {}
    explicit Mesh(QuadSurf const & qs) : verts{qs.verts}, surfaces{{Surf{qs.quads}}} {}
    Mesh(Vec3Fs const & vts,Vec3UIs const & ts) : verts(vts), surfaces(svec(Surf(ts))) {}
    Mesh(Vec3Fs const & vts,Vec4UIs const & quads) : verts(vts), surfaces{{Surf{quads}}} {}
    Mesh(Vec3Fs const & vts,Surfs const & surfs) : verts(vts), surfaces(surfs) {}
    Mesh(Vec3Fs const & v,Surfs const & s,DirectMorphs const & m) : verts(v), surfaces(s), deltaMorphs(m) {}
    explicit Mesh(TriSurfLms const & tsf);

    size_t              allVertsSize() const;               // size of below
    // Return base verts plus all target morph verts plus all animPart bones:
    Vec3Fs              allVerts() const;
    void                updateAllVerts(Vec3Fs const &);     // Update verts / targetMorphs / joints
    uint                numPolys() const;                  // tris plus quads over all surfaces
    uint                numTriEquivs() const;               // tris plus 2*quads over all surfaces
    Vec3UI              getTriEquivVertInds(uint idx) const;
    NPolys<3>           getTriEquivs() const;               // Over all surfaces
    size_t              numTris() const;                    // Just the number of tris over all surfaces
    size_t              numQuads() const;                   // Just the number of quads over all surfaces
    Surf const &        surface(String8 const & surfName) const {return findFirst(surfaces,surfName); }
    size_t              surfPointNum() const;              // Over all surfaces
    Vec3F               surfPointPos(Vec3Fs const & verts,size_t num) const;
    Vec3F               surfPointPos(size_t num) const {return surfPointPos(verts,num); }
    Opt<Vec3F>          surfPointPos(String const & label) const;
    NameVec3Fs          surfPointsAsNameVecs() const {return surfPointsToNameVecs(surfaces,verts); }
    Vec3Fs              surfPointPositions(Strings const & labels) const;
    Vec3Fs              surfPointPositions() const;
    Vec3F               markedVertPos(String const & name_) const {return verts[findFirst(markedVerts,name_).idx]; }
    Vec3Fs              markedVertPositions() const;        // Return positions of all marked verts
    // Returns the marked verts with the given labels (including duplicates) in the given order:
    Vec3Fs              markedVertPositions(Strings const & labels) const;
    NameVec3Fs          markedVertsAsNameVecs() const;
    void                addMarkedVert(Vec3F pos,String const & label)
    {
        markedVerts.push_back(MarkedVert(uint(verts.size()),label));
        verts.push_back(pos);
    }
    Svec<Sptr<ImgRgba8>> albedoMaps() const
    {
        Svec<Sptr<ImgRgba8>>         ret;
        ret.reserve(surfaces.size());
        for (size_t ss=0; ss<surfaces.size(); ++ss)
            ret.push_back(surfaces[ss].material.albedoMap);
        return ret;
    }
    uint                numValidAlbedoMaps() const
    {
        uint        ret = 0;
        for (size_t ss=0; ss<surfaces.size(); ++ss)
            if (surfaces[ss].material.albedoMap)
                ++ret;
        return ret;
    }
    Svec<ImgRgba8>      albedoMapsOld() const
    {
        Svec<ImgRgba8>     ret;
        ret.reserve(surfaces.size());
        for (size_t ss=0; ss<surfaces.size(); ++ss) {
            if (surfaces[ss].material.albedoMap)
                ret.push_back(*surfaces[ss].material.albedoMap);
            else
                ret.push_back(ImgRgba8());
        }
        return ret;
    }
    TriSurf             asTriSurf() const;

    // MORPHS:
    size_t              numMorphs() const {return deltaMorphs.size() + targetMorphs.size(); }
    String8             morphName(size_t idx) const;
    String8s            morphNames() const;
    Valid<size_t>       findDeltaMorph(String8 const & name) const;
    Valid<size_t>       findTargMorph(String8 const & name) const;
    Valid<size_t>       findMorph(String8 const & name) const;          // Return the combined morph index
    // morph using member base and target vertices:
    void                morph(Floats const & coord,Vec3Fs & outVerts) const;
    // morph using given base and target vertices:
    void                morph(
        Vec3Fs const &      allVerts,       // Must have same number of verts as base plus targets
        Floats const &      coord,          // Combined morph coordinate over delta then targer morphs
        Vec3Fs &            outVerts)       // RETURNED. Same size as base verts
        const;
    // morph using member base and target vertices:
    Vec3Fs              morph(
        Floats const &      deltaMorphCoord,
        Floats const &      targMorphCoord)
        const;
    // Apply just a single morph by its universal index (ie over deltas & targets):
    Vec3Fs              morphSingle(size_t idx,float val = 1.0f) const;
    IndexedMorph        getMorphAsIndexedDelta(size_t idx) const;
    // Overwrites any existing morph of the same name:
    void                addDeltaMorph(DirectMorph const & deltaMorph);
    // Overwrites any existing morph of the same name:
    void                addDeltaMorphFromTarget(String8 const & name,Vec3Fs const & targetShape);
    // Overwrites any existing morph of the same name:
    void                addTargMorph(const IndexedMorph & morph);
    // Overwrites any existing morph of the same name:
    void                addTargMorph(String8 const & name,Vec3Fs const & targetShape);
    Vec3Fs              poseShape(Vec3Fs const & allVerts,std::map<String8,float> const & poseVals) const;

    // EDITING:
    void                addSurfaces(Surfs const & s);
    void                transform_(Affine3F const &);     // Transform each data type appropriately
    void                transform_(SimilarityD const & sim) {transform_(Affine3F{sim.asAffine()}); }
    void                convertToTris();
    void                removeUVs();
    void                checkValidity() const;          // Throws if the mesh is not valid
};
typedef Svec<Mesh>      Meshes;
std::ostream &          operator<<(std::ostream &,Mesh const &);
std::ostream &          operator<<(std::ostream &,Meshes const &);

Mat32F              cBounds(Meshes const & meshes);
size_t              cNumTriEquivs(Meshes const & meshes);
std::set<String8>   getMorphNames(Meshes const & meshes);
PoseDefs            cPoseDefs(Mesh const & mesh);
PoseDefs            cPoseDefs(Meshes const & meshes);
inline MeshNormals  cNormals(Mesh const & mesh) {return cNormals(mesh.surfaces,mesh.verts); }
Mesh                transform(Mesh const &,SimilarityD const &);

TriSurf             subdivide(TriSurf const & surf,bool loop);  // Loop subdivision of true, flat subdivision otherwise
Mesh                subdivide(Mesh const &,bool loop = true);   // If 'loop' not selected then just do flat subdivision
// Remove all tris that lie entirely outside the given bounds then remove all unused vertices:
TriSurf             cullVolume(TriSurf surf,Mat32F const & bounds);

// Creates 2.5D surface from depth image:
// * Vertex created for each pixel (RCS X to X, Y to Y and pixel value to Z)
// * Vertices are re-scaled to fit bounding box [0,1]^3
// * Quads created by defined pixel grid.
// * UVs created to fit bounds of image (so for perfect correspondence the depth image pixel
//   centres should fill the domain bounds wheras the texture image bounds should map the domain
//   bounds).
Mesh            meshFromImage(const ImgD & img);

// Create a grid of SxS squares filling XY in [-1,1] at Z=0:
QuadSurf        cGrid(size_t squaresPerSide);
TriSurf         cCubeTris(bool open=false);         // size 2 cube centred at origin
TriSurf         cPyramid(bool open=false);
// Regular tetrahedron, centre at origin, edges length 2*sqrt(2), CC winding.
TriSurf         cTetrahedron(bool open=false);
TriSurf         cOctahedron();                      // Size (max width) 2 centred around origin. CC winding.
// Regular icosahedron, centre at origin, all vertices distance 1 from origin, CC winding:
TriSurf         cIcosahedron();
TriSurf         cIcosahedron(float scale,Vec3F const & centre); // as above but scaled then translated
TriSurf         cNTent(uint nn);
// Create unit radius sphere centred at origin by subdividing a tetrahedron and renormalizing the 
// vertex positions 'subdivision' times. Poor isotropy.
TriSurf         cSphere4(size_t subdivisions);
// Create unit radius sphere centred at origin by subdividing an icosahedron and renormalizing the
// vertex positions 'subdivision' times (0 - 20 tris, 1 - 80 tris, 2 - 320 tris, 3 - 1280 tris):
TriSurf         cSphere(size_t subdivisions);

Mesh            removeDuplicateFacets(Mesh const &);
// Removes vertices & uvs that are not referenced by a surface or marked vertex.
// Retains only those morphs which affect the remaining vertices:
Mesh            removeUnusedVerts(Mesh const &);
// Removes the given vertices (by index, can be specified in any order), along with any marked verts,
// polys, surface points that depend on them. Morphs updated. Joint information discarded.
Mesh            removeVerts(Mesh const & orig,Uints const & vertInds);
Mesh            mergeSameNameSurfaces(Mesh const &);
Mesh            fuseIdenticalVerts(Mesh const &);       // morphs and marked verts are discarded
Mesh            fuseIdenticalUvs(Mesh const &);
Mesh            splitSurfsByUvContiguous(Mesh const &);
// Merge surfaces in meshes with identically sized vertex lists,
// keeping the vertex list of the first mesh:
Mesh            mergeMeshSurfaces(Mesh const & m0,Mesh const & m1);
// Unify vertex lists, preserves all separate surfaces, unifies morphs with same name.
// Does not handle Joints:
Mesh            mergeMeshes(Ptrs<Mesh> meshPtrs);
inline Mesh     mergeMeshes(Meshes const & ms) {return mergeMeshes(mapAddr(ms)); }
// Doesn't preserve uvs, surface points or marked verts:
Mesh            fg3dMaskFromUvs(Mesh const & mesh,const Img<FatBool> & mask);
// Binary image of which texels (centre point) are in the mesh UV layout (0 - no map, 255 - map):
ImgUC           getUvCover(Mesh const & mesh,Vec2UI dims);
// Wireframe image of UV layout for each surface:
ImgRgba8s       cUvWireframeImages(Mesh const & mesh,Rgba8 wireColor);
// Emboss the given pattern onto a mesh with UVs, with max magnitude given by image value 255,
// corresponding to a displacement (in the direction of surface normal) by 'ratio' times the
// max bounding box dimensions of all vertices whose UV coordinate in 'pattern' sample to non-zero:
Vec3Fs          embossMesh(Mesh const & mesh,const ImgUC & pattern,double ratio=0.05);

struct  MorphVal
{
    String8         name;
    float           val;            // 0 - no change, 1 - full application

    MorphVal() {}
    MorphVal(String8 const & name_,float val_) : name(name_), val(val_) {}
};
typedef Svec<MorphVal>  MorphVals;

// Only applies those morphs which mesh supports, ignores the rest:
Vec3Fs          poseMesh(Mesh const & mesh,MorphVals const &  morphVals);
// Cannot be functional since marked verts are stored as indices into the vertex array, which must
// be updated in sync:
void            surfPointsToMarkedVerts_(Mesh const & in,Mesh & out);
Vec3Fs          cMirrorX(Vec3Fs const & verts);             // reflect verts in X=0 plane
inline TriSurf  cMirrorX(TriSurf const & ts) {return {cMirrorX(ts.verts),reverseWinding(ts.tris)}; }
Surf            cMirrorX(Surf const & surf);        // same as reverseWinding but also modifies surface point names
// Mirror vertex positions around X=0 plane, reverse poly winding and UV winding
// swap surface point and marked vertex labels ending in 'L' and 'R'
// UVs, maps, points left unchanged, morphs removed:
Mesh            cMirrorX(Mesh const &);
// for all points whose name ends in 'L'/'R', add the mirror point (around X=0) ending in 'R'/'L'.
// points not ending in 'L' or 'R' have X value set to 0:
NameVec3Fs      mirrorXFuse(NameVec3Fs const & nvs);
// Mirrors geometry around X=0 plane, fused through vertices on that plane (X value exactly 0.0).
// All input verts must have X>=0. Surface points not mirrored.
// Delta morphs are mirrored but they must be X=0 symmetry friendly or ugliness will result.
// UVs, target morphs, etc. discarded.
Mesh            mirrorXFuse(Mesh const & in);
// Copy the surface assignment (tris only) between aligned meshes of different topology:
Mesh            copySurfaceStructure(Mesh const & from,Mesh const & to);
// Merge all surface facets converted to tris:
Svec<Vec3UI>    meshSurfacesAsTris(Mesh const &);

}

#endif
