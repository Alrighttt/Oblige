//------------------------------------------------------------------------
//  CSG : QUAKE I and II
//------------------------------------------------------------------------
//
//  Oblige Level Maker
//
//  Copyright (C) 2006-2010 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "headers.h"

#include <algorithm>

#include "hdr_fltk.h"
#include "hdr_lua.h"
#include "hdr_ui.h"

#include "lib_file.h"
#include "lib_util.h"

#include "main.h"
#include "m_lua.h"

#include "q_common.h"
#include "q_light.h"

#include "csg_main.h"
#include "csg_local.h"
#include "csg_quake.h"

#include "q1_main.h"
#include "q1_structs.h"


#define FACE_MAX_SIZE  240

#define LIGHT_INDOOR   64
#define LIGHT_OUTDOOR  100


extern void Q1_CreateSubModels(qLump_c *L, int first_face, int first_leaf);


class rSide_c;
class rNode_c;


static rNode_c *R_ROOT;

static rNode_c *SOLID_LEAF;

static dmodel_t model;

qLump_c *q1_nodes;
qLump_c *q1_leafs;
qLump_c *q1_faces;

static qLump_c *q_mark_surfs;
static qLump_c *q_surf_edges;
static qLump_c *q_clip_nodes;

int q1_total_nodes;
int q1_total_leafs;
int q1_total_faces;

int q1_total_mark_surfs;
int q1_total_surf_edges;
int q1_total_clusters;


static std::vector<rNode_c *> all_windings;

static std::vector<rNode_c *> z_leafs;


class rWindingVerts_c
{
public:
  int count;

  float x[256];
  float y[256];
};


class rFace_c
{
public:
  enum
  {
    WALL  = 0,
    FLOOR = 1,
    CEIL  = 2
  };

  int kind;

  int index;

  csg_property_set_c *face;

  int light;

  /* WALL STUFF */

  rSide_c *side;
  double z1, z2;

  /* FLOOR n CEIL STUFF */

  double z;

  rWindingVerts_c *UU;

public:
  rFace_c() : index(-1), face(NULL), light(0)
  { }
};


#if 0
class rFaceFactory_c
{
  static std::list<rFace_c> all_faces;

public:
  static rFace_c *NewFace(int kind)
  {
    all_faces.push_back(rFace_c(kind));

    rFace_c *F = &all_faces.back();

    return F;
  }

  static rFace_c *NewFace(int kind, int gap, double z1, double z2,
                          brush_vert_c *av)
  {
    rFace_c *F = NewFace(kind);

    F->gap = gap;
    F->z1  = z1;
    F->z2  = z2;
    F->area_v = av;

    return F;
  }

  static rFace_c *CopyFace(const rFace_c *other)
  {
    rFace_c *F = NewFace(other->kind);

    F->gap = other->gap;
    F->z1  = other->z1;
    F->z2  = other->z2;
    F->area_v = other->area_v;

    return F;
  }

  static void FreeAll()
  {
    all_faces.clear();
  }
};

#endif


class rSide_c
{
friend class rSideFactory_c;

public:
  snag_c *snag;

  merge_segment_c *seg;

//  int side;  // side on the seg: 0 = front/right, 1 = back/left

  rSide_c *partner;
  rNode_c *node;

  double x1, y1;
  double x2, y2;

  rNode_c *on_partition;

  std::vector<rFace_c *> faces;

public:
  rSide_c(merge_segment_c * _seg = NULL, int _side = 0) :
      seg(_seg),              partner(NULL), node(NULL),
      on_partition(NULL), faces()
  { }

  ~rSide_c()
  { }

public:
  bool Usable() const
  {
    return (seg && ! on_partition);
  }

  double Length() const
  {
    return ComputeDist(x1,y1, x2,y2);
  }

  merge_region_c *FrontRegion() const
  {
    if (! seg) return NULL;
///!!!!!    return side ? seg->back : seg->front;
  }

  merge_region_c *BackRegion() const
  {
    if (! seg) return NULL;
///!!!!!    return side ? seg->front : seg->back;
  }

  void AddFace(int kind, int gap, double z1, double z2, brush_vert_c *av)
  {
///    rFace_c *F = rFaceFactory_c::NewFace(kind, gap, z1, z2, av);
///
///    faces.push_back(F);
  }
};


class rSideFactory_c
{
  static std::list<rSide_c> all_sides;

  static rSide_c *RawNew(merge_segment_c *seg, int side)
  {
    all_sides.push_back(rSide_c(seg, side));
    return &all_sides.back();
  }

public:
  static rSide_c *NewSide(snag_c *_snag)
  {
merge_segment_c *seg;
int side = 0;

    rSide_c *S = RawNew(seg, side);

    if (side == 0)
    {
      S->x1 = seg->start->x;  S->x2 = seg->end->x;
      S->y1 = seg->start->y;  S->y2 = seg->end->y;
    }
    else
    {
      S->x1 = seg->end->x;  S->x2 = seg->start->x;
      S->y1 = seg->end->y;  S->y2 = seg->start->y;
    }

    return S;
  }

  static rSide_c *NewPortal(double px, double py, double pdx, double pdy,
                            double start, double end)
  {
    rSide_c *P = RawNew(NULL, 0);

    double p_len = ComputeDist(0, 0, pdx, pdy);
    P->x1 = px + pdx * start / p_len;
    P->y1 = py + pdy * start / p_len;

    P->x2 = px + pdx * end / p_len;
    P->y2 = py + pdy * end / p_len;

    return P;
  }

  static rSide_c *SplitAt(rSide_c *S, double new_x, double new_y);

  static rSide_c *FakePartition(double x, double y, double dx, double dy)
  {
    rSide_c * S = RawNew(NULL, 0);

    S->x1 = x;
    S->y1 = y;

    S->x2 = x+dx;
    S->y2 = y+dy;

    return S;
  }

  static void FreeAll()
  {
    all_sides.clear();
  }
};


std::list<rSide_c> rSideFactory_c::all_sides;
/// std::list<rFace_c> rFaceFactory_c::all_faces;


#define CONTENT__NODE  12345


class rNode_c
{
public:
  /* LEAF STUFF */

  int lf_contents;

  std::vector<rSide_c *> sides;

  // this node just contains a list of sides which will be used
  // to construct the floor and ceiling faces.
  rNode_c * winding;

  merge_region_c *region;

  int gap;

  int cluster;

  int light;


  /* NODE STUFF */

  // if this node splits the tree by Z
  int z_splitter;

  double z; int dz;

  // normal splitting planes are vertical, and here are the
  // coordinates on the map.
  double x,  y;
  double dx, dy;

  rNode_c *front;  // front space
  rNode_c *back;   // back space

  rWindingVerts_c *wi_verts;


  /* COMMON STUFF */

  std::vector<rFace_c *> faces;

  int index;

  bool bbox_valid;

  double mins[3];
  double maxs[3];

public:
  // LEAF
  rNode_c() : lf_contents(CONTENTS_EMPTY), sides(),
              winding(NULL), region(NULL), cluster(-1), light(LIGHT_INDOOR),
              front(NULL), back(NULL), wi_verts(NULL),
              faces(), index(-1)
  {
    EmptyBBox();
  }

  rNode_c(bool _Zsplit) : lf_contents(CONTENT__NODE), sides(),
                          winding(NULL), region(NULL), cluster(-1),
                          z_splitter(_Zsplit), z(0), dz(_Zsplit ? 1 : 0),
                          x(0), y(0), dx(0), dy(0),
                          front(NULL), back(NULL), wi_verts(NULL),
                          faces(), index(-1)
  {
    EmptyBBox();
  }

  ~rNode_c()
  {
    if (front && front != SOLID_LEAF) delete front;
    if (back  && back  != SOLID_LEAF) delete back;
  }

  inline bool IsNode()  const { return lf_contents == CONTENT__NODE; }
  inline bool IsLeaf()  const { return lf_contents != CONTENT__NODE; }
  inline bool IsSolid() const { return lf_contents == CONTENTS_SOLID; }

  void AddSide(rSide_c *S)
  {
    S->node = this;

    sides.push_back(S);

    if (! bbox_valid)
      ComputeLeafBBox();
    else
    {
      AddToBBox(S->x1, S->y1);
      AddToBBox(S->x2, S->y2);
    }
  }

  void AddFace(rFace_c *F)
  {
    faces.push_back(F);
  }

  int UsableSides() const
  {
    int count = 0;

    for (unsigned int i = 0; i < sides.size(); i++)
      if (sides[i]->Usable())
        count++;

    return count;
  }

  void CheckValid() const
  {
    SYS_ASSERT(index >= 0);
  }

  void BecomeNode(double x1, double y1, double x2, double y2)
  {
    lf_contents = CONTENT__NODE;

    z_splitter = false;

    x = x1; dx = x2 - x1;
    y = y1; dy = y2 - y1;
  }

  void EmptyBBox()
  {
    for (int b = 0; b < 3; b++)
      mins[b] = maxs[b] = 0;

    bbox_valid = false;
  }

  void AddToBBox(double x, double y)
  {
    if (x < mins[0]) mins[0] = x;
    if (y < mins[1]) mins[1] = y;

    if (x > maxs[0]) maxs[0] = x;
    if (y > maxs[1]) maxs[1] = y;
  }

  void ComputeLeafBBox()
  {
    SYS_ASSERT(lf_contents != CONTENT__NODE);

    mins[0] = +99999; maxs[0] = -99999;
    mins[1] = +99999; maxs[1] = -99999;
    mins[2] = -2000;  maxs[2] = +4000;

    for (unsigned int i = 0; i < sides.size(); i++)
    {
      rSide_c *S = sides[i];

      AddToBBox(S->x1, S->y1);
      AddToBBox(S->x2, S->y2);
    }

    bbox_valid = true;
  }

  void ComputeNodeBBox()
  {
    bool front_ok = (front != SOLID_LEAF && front->bbox_valid);
    bool  back_ok = ( back != SOLID_LEAF &&  back->bbox_valid);

    if (! front_ok && ! back_ok)
    {
      EmptyBBox();
      return;
    }

    for (int b = 0; b < 3; b++)
    {
      if (! front_ok)
      {
        mins[b] = back->mins[b];
        maxs[b] = back->maxs[b];
      }
      else if (! back_ok)
      {
        mins[b] = front->mins[b];
        maxs[b] = front->maxs[b];
      }
      else
      {
        mins[b] = MIN(front->mins[b], back->mins[b]);
        maxs[b] = MAX(front->maxs[b], back->maxs[b]);
      }
    }

    bbox_valid = true;
  }

  void CopyBBox(const rNode_c *src)
  {
    for (int b = 0; b < 3; b++)
    {
      mins[b] = src->mins[b];
      maxs[b] = src->maxs[b];
    }

    bbox_valid = src->bbox_valid;
  }

  double BBoxSizeX() const { return maxs[0] - mins[0]; }
  double BBoxSizeY() const { return maxs[1] - mins[1]; }
  double BBoxSizeZ() const { return maxs[2] - mins[2]; }

  double BBoxMidX() const { return (mins[0] + maxs[0]) * 0.5; }
  double BBoxMidY() const { return (mins[1] + maxs[1]) * 0.5; }
  double BBoxMidZ() const { return (mins[2] + maxs[2]) * 0.5; }
};


rSide_c *rSideFactory_c::SplitAt(rSide_c *S, double new_x, double new_y)
{
//!!!!!! FIXME
#if 0 
  rSide_c *T = RawNew(S->seg, S->side);

  T->x2 = S->x2; T->y2 = S->y2;
  T->x1 = new_x; T->y1 = new_y;
  S->x2 = new_x; S->y2 = new_y;

  T->node = S->node;
  T->on_partition = S->on_partition;

  // howdy partners!  handle them
  if (S->partner)
  {
    rSide_c *SP = S->partner;
    rSide_c *TP = RawNew(SP->seg, SP->side);

    if (S->seg)
    {
      SYS_ASSERT(SP->side != S->side);
    }

    TP->x1 = SP->x1; TP->y1 = SP->y1;
    TP->x2 = new_x;  TP->y2 = new_y;
    SP->x1 = new_x;  SP->y1 = new_y;

    TP->node = SP->node;
    TP->on_partition = SP->on_partition;

    // establish partner relationship (S <-> SP remains OK)
     T->partner = TP;
    TP->partner = T;

    // insert new partner into its containing node
    SYS_ASSERT(TP->node);

    TP->node->AddSide(TP);
  }
  return T;
#else
  return NULL;
#endif
}


static rSide_c * CreateSide(rNode_c *LEAF, snag_c *snag, int side)
{
  rSide_c *S = rSideFactory_c::NewSide(snag);

  LEAF->AddSide(S);

// fprintf(stderr, "Create side: seg:%p  (%1.0f %1.0f) -> (%1.0f %1.0f)\n",
// S->seg, S->x1, S->y1, S->x2, S->y2);

  return S;
}

static rNode_c * NewLeaf(int contents)
{
  rNode_c *leaf = new rNode_c();

  leaf->lf_contents = contents;

  return leaf;
}


#if 0
static void CreatePortals(rNode_c *part, rNode_c *FRONT, rNode_c *BACK,
                          std::vector<intersect_t> & cut_list)
{
  for (unsigned int i = 0; i < cut_list.size(); i++)
  {
    double along1 = cut_list[i].along;
    double along2 = cut_list[i].next_along;

    SYS_ASSERT(along1 < along2);

    rSide_c *F = rSideFactory_c::NewPortal(part->x, part->y, part->dx, part->dy, along1, along2);
    rSide_c *B = rSideFactory_c::NewPortal(part->x, part->y, part->dx, part->dy, along2, along1);

    F->partner = B;
    B->partner = F;

    FRONT->AddSide(F);
     BACK->AddSide(B);
  }
}
#endif


static double EvaluatePartition(rNode_c * LEAF,
                                double px1, double py1, double px2, double py2)
{
  double pdx = px2 - px1;
  double pdy = py2 - py1;

  int back   = 0;
  int front  = 0;
  int splits = 0;

  std::vector<rSide_c *>::iterator SI;

  for (SI = LEAF->sides.begin(); SI != LEAF->sides.end(); SI++)
  {
    rSide_c *S = *SI;

    // get state of lines' relation to each other
    double a = PerpDist(S->x1, S->y1, px1, py1, px2, py2);
    double b = PerpDist(S->x2, S->y2, px1, py1, px2, py2);

    int a_side = (a < -Q_EPSILON) ? -1 : (a > Q_EPSILON) ? +1 : 0;
    int b_side = (b < -Q_EPSILON) ? -1 : (b > Q_EPSILON) ? +1 : 0;

    if (a_side == 0 && b_side == 0)
    {
      // lines are colinear

      if (VectorSameDir(pdx, pdy, S->x2 - S->x1, S->y2 - S->y1))
        front++;
      else
        back++;

      continue;
    }

    if (a_side >= 0 && b_side >= 0)
    {
      front++;
      continue;
    }

    if (a_side <= 0 && b_side <= 0)
    {
      back++;
      continue;
    }

    // the partition line will split it

    splits++;

    back++;
    front++;
  }


  // always prefer axis-aligned planes
  // (this helps prevent the "sticky doorways" problem)
  bool aligned = (fabs(pdx) < 0.0001 || fabs(pdy) < 0.0001);

  if (front == 0 && back == 0)
    return aligned ? 1e24 : 1e26;

  if (splits > 1000)
    splits = 1000;

  double cost = splits * (splits+1) * 3.65;

  cost += ABS(front - back);
  cost /=    (front + back);

  if (! aligned)
    cost += 1e12;

  return cost;
}


#if 0
static void OLD_DivideOneSide(rSide_c *S, rNode_c *part, rNode_c *FRONT, rNode_c *BACK,
                          std::vector<intersect_t> & cut_list)
{
  SYS_ASSERT(S && part && FRONT && BACK);

  S->node = NULL;

  double sdx = S->x2 - S->x1;
  double sdy = S->y2 - S->y1;

  // get state of lines' relation to each other
  double a = PerpDist(S->x1, S->y1,
                      part->x, part->y,
                      part->x + part->dx, part->y + part->dy);

  double b = PerpDist(S->x2, S->y2,
                      part->x, part->y,
                      part->x + part->dx, part->y + part->dy);

  int a_side = (a < -Q_EPSILON) ? -1 : (a > Q_EPSILON) ? +1 : 0;
  int b_side = (b < -Q_EPSILON) ? -1 : (b > Q_EPSILON) ? +1 : 0;

/// fprintf(stderr, "dividing side %p valid:%d (%1.1f,%1.1f -> %1.1f,%1.1f)\n",
///         S, (S->seg && ! S->on_partition) ? 1 : 0,
///         S->x1, S->y1, S->x2, S->y2);

  if (a_side == 0 && b_side == 0)
  {
    // side sits on the partition, it will go either left or right
    S->on_partition = part;

    if (VectorSameDir(part->dx, part->dy, sdx, sdy))
    {
// fprintf(stderr, "  --> front\n");
      FRONT->AddSide(S);

      // +2 and -2 mean "remove"
      BSP_AddIntersection(cut_list, P_Along(part, S, 0), +2);
      BSP_AddIntersection(cut_list, P_Along(part, S, 1), -2);
    }
    else
    {
// fprintf(stderr, "  --> back\n");
      BACK->AddSide(S);

      BSP_AddIntersection(cut_list, P_Along(part, S, 0), -2);
      BSP_AddIntersection(cut_list, P_Along(part, S, 1), +2);
    }

    return;
  }

  /* check for right side */
  if (a_side >= 0 && b_side >= 0)
  {
    if (a_side == 0)
      BSP_AddIntersection(cut_list, P_Along(part, S, 0), -1);
    else if (b_side == 0)
      BSP_AddIntersection(cut_list, P_Along(part, S, 1), +1);

// fprintf(stderr, "  --> front\n");
    FRONT->AddSide(S);
    return;
  }

  /* check for left side */
  if (a_side <= 0 && b_side <= 0)
  {
    if (a_side == 0)
      BSP_AddIntersection(cut_list, P_Along(part, S, 0), +1);
    else if (b_side == 0)
      BSP_AddIntersection(cut_list, P_Along(part, S, 1), -1);

// fprintf(stderr, "  --> back\n");
    BACK->AddSide(S);
    return;
  }

  /* need to split it */

  // determine the intersection point
  double along = a / (a - b);

  double ix = S->x1 + along * sdx;
  double iy = S->y1 + along * sdy;

// fprintf(stderr, "split at (%1.5f %1.5f)\n", ix, iy);
  rSide_c *T = rSideFactory_c::SplitAt(S, ix, iy);

  if (a < 0)
  {
     BACK->AddSide(S);
    FRONT->AddSide(T);
  }
  else
  {
    SYS_ASSERT(b < 0);

    FRONT->AddSide(S);
     BACK->AddSide(T);
  }

  BSP_AddIntersection(cut_list, P_Along(part, T, 0), a_side);
}
#endif


static rSide_c * OLD_FindPartition(rNode_c * LEAF)
{
  double w = LEAF->BBoxSizeX();
  double h = LEAF->BBoxSizeY();

  // speed up large maps
  if (MAX(w, h) > FACE_MAX_SIZE)
  {
    double lx = LEAF->mins[0];
    double ly = LEAF->mins[1];

    if (w >= h)
      return rSideFactory_c::FakePartition(BSP_NiceMidwayPoint(lx, w), 0, 0, 1);
    else
      return rSideFactory_c::FakePartition(0, BSP_NiceMidwayPoint(ly, h), 1, 0);
  }

  double   best_c = 1e30;
  rSide_c *best_p = NULL;

  std::vector<rSide_c *>::iterator SI;

  for (SI = LEAF->sides.begin(); SI != LEAF->sides.end(); SI++)
  {
    rSide_c *part = *SI;

    if (! part->Usable())
      continue;

    double cost = EvaluatePartition(LEAF, part->x1, part->y1, part->x2, part->y2);

    if (! best_p || cost < best_c)
    {
      best_c = cost;
      best_p = part;
    }
  }
// fprintf(stderr, "FIND DONE : best_c=%1.0f best_p=%p\n",
//         best_p ? best_c : -9999, best_p);

  SYS_ASSERT(best_p);

  return best_p;
}


static void OLD_Split_XY(rNode_c *part, rNode_c *FRONT, rNode_c *BACK)
{
#if 0
  std::vector<intersect_t> cut_list;

  // need to loop multiple times, since splits can cause partner
  // segs to be added back into the 'part' node/leaf.
  for (int loop = 0; loop < 100; loop++)
  {
    if (part->sides.size() == 0)
      break;

    std::vector<rSide_c *> all_sides;

    all_sides.swap(part->sides);

    for (unsigned int k = 0; k < all_sides.size(); k++)
    {
      DivideOneSide(all_sides[k], part, FRONT, BACK, cut_list);
    }
  }

  BSP_MergeIntersections(cut_list);

  CreatePortals(part, FRONT, BACK, cut_list);
#endif
}


static rNode_c * OLD_Partition_XY(rNode_c * LN, merge_region_c *part_reg = NULL)
{
  if (LN->UsableSides() == 0)
  {
    // workaround for areas that become completely surrounded by portals
    if (! part_reg && LN->sides.size() > 1)
    {
      double mx = LN->BBoxMidX();
      double my = LN->BBoxMidY();

//!!!!!! FIXME      part_reg = CSG2_FindRegionForPoint(mx, my);
    }

    if (! part_reg || part_reg->gaps.size() == 0)
    {
      if (LN->sides.size() > 0)
        LogPrintf("WARNING: Lost %u sides behind solid wall\n", LN->sides.size());

      /// CANNOT DELETE LN HERE (it may have sides who may have partners)

      return SOLID_LEAF;
    }

    all_windings.push_back(LN);

    // Z partitioning occurs later (the second pass)
    LN->region = part_reg;

    return LN;
  }


  rSide_c *part = OLD_FindPartition(LN);
  SYS_ASSERT(part);

// fprintf(stderr, "PARTITION_XY = (%1.2f,%1.2f) -> (%1.2f,%1.2f)\n",
//                part->x1, part->y1, part->x2, part->y2);

  // turn the pseudo leaf into a real node
  LN->BecomeNode(part->x1, part->y1, part->x2, part->y2);

  rNode_c * FRONT = new rNode_c();
  rNode_c * BACK  = new rNode_c();

/// int count = (int)LN->sides.size(); // LN->UsableSides();

  OLD_Split_XY(LN, FRONT, BACK);

/// int c_front = (int)FRONT->sides.size(); //  FRONT->UsableSides();
/// int c_back  = (int) BACK->sides.size(); //  BACK->UsableSides();

///  fprintf(stderr, "  SplitXY DONE: %d --> %d / %d\n", count, c_front, c_back);

  LN->front = OLD_Partition_XY(FRONT, part->FrontRegion());
  LN->back  = OLD_Partition_XY(BACK,  part->BackRegion());

  return LN;
}


static void OLD_Q1_BuildBSP()
{
  LogPrintf("\nQuake1_BuildBSP BEGUN\n");

  SOLID_LEAF = new rNode_c();
  SOLID_LEAF->lf_contents = CONTENTS_SOLID;
  SOLID_LEAF->index = -1;

  rNode_c *R_LEAF = new rNode_c();

  for (unsigned int i = 0 ; i < all_regions.size() ; i++)
  {
    region_c *R = all_regions[i];

    if (R->gaps.empty())
      continue;

    for (unsigned int k = 0 ; k < R->snags.size() ; k++)
    {
      snag_c *S = R->snags[k];

      rSide_c *F = NULL;
      rSide_c *B = NULL;

      // only handle partnered pairs ONCE
      if (S->partner && S > S->partner)
        continue;

      region_c *N = S->partner ? S->partner->region : NULL;

      if (N && N->equiv_id == R->equiv_id)
        continue;

      F = CreateSide(R_LEAF, S, 0);

      if (S->partner)
        B = CreateSide(R_LEAF, S->partner, 1);

      if (F && B)
      {
        F->partner = B;
        B->partner = F;
      }
    }
  }


  R_ROOT = OLD_Partition_XY(R_LEAF);

  SYS_ASSERT(R_ROOT->IsNode());
}



//------------------------------------------------------------------------
//  FACE STUFF
//------------------------------------------------------------------------


void Q1_AddEdge(double x1, double y1, double z1,
                double x2, double y2, double z2,
                dface_t *raw_face, dleaf_t *raw_lf = NULL)
{
  u16_t v1 = BSP_AddVertex(x1, y1, z1);
  u16_t v2 = BSP_AddVertex(x2, y2, z2);

  if (v1 == v2)
  {
    Main_FatalError("INTERNAL ERROR (Q1 AddEdge): zero length!\n"
                    "coordinate (%1.2f %1.2f %1.2f)\n", x1, y1, z1);
  }

  s32_t edge_idx = BSP_AddEdge(v1, v2);


  edge_idx = LE_S32(edge_idx);

  q_surf_edges->Append(&edge_idx, 4);

  q1_total_surf_edges += 1;

  raw_face->numedges += 1;


  // update bounding boxes
  double lows[3], highs[3];

  lows[0] = MIN(x1, x2);  highs[0] = MAX(x1, x2);
  lows[1] = MIN(y1, y2);  highs[1] = MAX(y1, y2);
  lows[2] = MIN(z1, z2);  highs[2] = MAX(z1, z2);

  for (int b = 0; b < 3; b++)
  {
    s16_t low  =  (I_ROUND( lows[b]) - 2) & ~3;
    s16_t high = ((I_ROUND(highs[b]) + 2) |  3) + 1;

    if (raw_lf)
    {
      raw_lf->mins[b] = MIN(raw_lf->mins[b], low);
      raw_lf->maxs[b] = MAX(raw_lf->maxs[b], high);
    }
  }
}


void Q1_AddSurf(u16_t index, dleaf_t *raw_lf )
{
  index = LE_U16(index);

  q_mark_surfs->Append(&index, 2);

  q1_total_mark_surfs += 1;

  raw_lf->num_marksurf += 1;
}


struct Compare_FloorAngle_pred
{
  double *angles;

   Compare_FloorAngle_pred(double *p) : angles(p) { }
  ~Compare_FloorAngle_pred() { }

  inline bool operator() (int A, int B) const
  {
    return angles[A] < angles[B];
  }
};


static rWindingVerts_c * CollectClockwiseVerts(rNode_c *winding)
{
  rWindingVerts_c * UU = new rWindingVerts_c;

  int v_num = 0;

  std::vector<rSide_c *>::iterator SI;

  double mid_x = 0;
  double mid_y = 0;
  

  for (SI = winding->sides.begin(); SI != winding->sides.end(); SI++)
  {
    rSide_c *S = *SI;

    UU->x[v_num] = S->x1;
    UU->y[v_num] = S->y1;

    mid_x += UU->x[v_num];
    mid_y += UU->y[v_num];

    v_num++;
  }

  UU->count = v_num;

  if (UU->count == 0)
    return UU;

  mid_x /= v_num;
  mid_y /= v_num;

  
  // determine angles, then sort into clockwise order

  double angles[256];

  std::vector<int> mapping(v_num);

  for (int a = 0; a < v_num; a++)
  {
    angles[a]  = 0 - CalcAngle(mid_x, mid_y, UU->x[a], UU->y[a]);
    mapping[a] = a;
  }


  std::sort(mapping.begin(), mapping.end(),
            Compare_FloorAngle_pred(angles));


  // apply mapping to vertices
  float old_x[256];
  float old_y[256];

  for (int k = 0; k < v_num; k++)
  {
    old_x[k] = UU->x[k];
    old_y[k] = UU->y[k];
  }

///fprintf(stderr, "\nMIDDLE @ (%1.2f %1.2f) COUNT:%d\n", mid_x, mid_y, v_num);
  for (int m = 0; m < v_num; m++)
  {
    UU->x[m] = old_x[mapping[m]];
    UU->y[m] = old_y[mapping[m]];

///fprintf(stderr, "___ (%+5.0f %+5.0f)\n", vert_x[m], vert_y[m]);
  }

  return UU;
}


static int CalcTextureFlag(const char *tex_name)
{
  if (tex_name[0] == '*')
    return TEX_SPECIAL;

  if (strncmp(tex_name, "sky", 3) == 0)
    return TEX_SPECIAL;

  return 0;
}

static double DotProduct3(const double *A, const double *B)
{
  return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
}

static void GetExtents(double min_s, double min_t, double max_s, double max_t,
                       int *ext_W, int *ext_H)
{
  // -AJA- this matches the logic in the Quake1 engine.

  int bmin_s = (int)floor(min_s / 16.0);
  int bmin_t = (int)floor(min_t / 16.0);

  int bmax_s = (int)ceil(max_s / 16.0);
  int bmax_t = (int)ceil(max_t / 16.0);

  *ext_W = bmax_s - bmin_s + 1;
  *ext_H = bmax_t - bmin_t + 1;
}


static void BuildFloorFace(dface_t& raw_face, rFace_c *F, rNode_c *N)
{
  bool is_ceil = (F->kind == rFace_c::CEIL) ? true : false;
  bool flipped;

  double z = F->z;

  raw_face.planenum = BSP_AddPlane(0, 0, z,
                                   0, 0, is_ceil ? -1 : +1, &flipped);
  raw_face.side = flipped ? 1 : 0;


  const char *texture = F->face->getStr("tex", "missing");

  int flags = CalcTextureFlag(texture);

  double s[4] = { 1.0, 0.0, 0.0, 0.0 };
  double t[4] = { 0.0, 1.0, 0.0, 0.0 };

  raw_face.texinfo = Q1_AddTexInfo(texture, flags, s, t);

  raw_face.styles[0] = 0xFF;  // no lightmap
  raw_face.styles[1] = 0xFF;
  raw_face.styles[2] = 0xFF;
  raw_face.styles[3] = 0xFF;

  raw_face.lightofs = -1;  // no lightmap

  
  rWindingVerts_c *UU = F->UU;

  // FIXME: need at least 3 verts
  int v_num = UU->count;

  double min_x = +9e9; double max_x = -9e9;
  double min_y = +9e9; double max_y = -9e9;


  // add the edges [anti-clockwise if flipped]

  for (int pos = 0; pos < v_num; pos++)
  {
    int p1;
    int p2;

    if (flipped)
    {
      p1 = (v_num*2 - pos)     % v_num;
      p2 = (v_num*2 - pos - 1) % v_num;
    }
    else
    {
      p1 = pos;
      p2 = (pos + 1) % v_num;
    }

    Q1_AddEdge(UU->x[p1], UU->y[p1], z,
               UU->x[p2], UU->y[p2], z, &raw_face);

    min_x = MIN(min_x, UU->x[pos]);
    min_y = MIN(min_y, UU->y[pos]);
    max_x = MAX(max_x, UU->x[pos]);
    max_y = MAX(max_y, UU->y[pos]);
  }


  if (! (flags & TEX_SPECIAL))
  {
    int ext_W, ext_H;

    GetExtents(min_x, min_y, max_x, max_y, &ext_W, &ext_H);

    static int foo; foo++;
    raw_face.styles[0] = 0; // (foo & 3); //!!!!!

    raw_face.lightofs = 60*17*17;  //!!!! FIXME
  }
}


static void BuildWallFace(dface_t& raw_face, rFace_c *F, rNode_c *N)
{
  rSide_c *S = F->side;

///  merge_gap_c *gap = R->gaps.at(F->gap);

  double z1 = F->z1;
  double z2 = F->z2;


  bool flipped;

  raw_face.planenum = BSP_AddPlane(S->x1, S->y1, 0,
                                   (S->y2 - S->y1), (S->x1 - S->x2), 0,
                                   &flipped);

  raw_face.side = flipped ? 1 : 0;


  const char *texture = F->face->getStr("tex", "missing");

  int flags = CalcTextureFlag(texture);

  double s[4] = { 0.0, 0.0, 0.0, 0.0 };
  double t[4] = { 0.0, 0.0, 1.0, 0.0 };

  if (fabs(S->x1 - S->x2) > fabs(S->y1 - S->y2))
  {
    s[0] = 1.0;
  }
  else
  {
    s[1] = 1.0;
  }

  raw_face.texinfo = Q1_AddTexInfo(texture, flags, s, t);

  raw_face.styles[0] = 0xFF;  // no lightmap
  raw_face.styles[1] = 0xFF;
  raw_face.styles[2] = 0xFF;
  raw_face.styles[3] = 0xFF;

  raw_face.lightofs = -1;  // no lightmap

  // add the edges

  Q1_AddEdge(S->x1, S->y1, z1,  S->x1, S->y1, z2, &raw_face);
  Q1_AddEdge(S->x1, S->y1, z2,  S->x2, S->y2, z2, &raw_face);
  Q1_AddEdge(S->x2, S->y2, z2,  S->x2, S->y2, z1, &raw_face);
  Q1_AddEdge(S->x2, S->y2, z1,  S->x1, S->y1, z1, &raw_face);

  if (! (flags & TEX_SPECIAL))
  {
    double coord[4][3];

    coord[0][0] = S->x1; coord[0][1] = S->y1; coord[0][2] = z1;
    coord[1][0] = S->x1; coord[1][1] = S->y1; coord[1][2] = z2;
    coord[2][0] = S->x2; coord[2][1] = S->y2; coord[2][2] = z1;
    coord[3][0] = S->x2; coord[3][1] = S->y2; coord[3][2] = z2;

    double min_s = +9e9; double max_s = -9e9;
    double min_t = +9e9; double max_t = -9e9;

    for (int k = 0; k < 4; k++)
    {
      double ss = DotProduct3(s, coord[k]);
      double tt = DotProduct3(t, coord[k]);

      min_s = MIN(min_s, ss); max_s = MAX(max_s, ss);
      min_t = MIN(min_t, tt); max_t = MAX(max_t, tt);
    }

    int ext_W, ext_H;

    GetExtents(min_s, min_t, max_s, max_t, &ext_W, &ext_H);

    static int foo = 0; foo++;
    raw_face.styles[0] = 0; // (foo & 3); //!!!!!

    raw_face.lightofs = 80*17*17; //!!!! FIXME
  }
}


static rFace_c * NewFace(rSide_c *S, double z1, double z2,
                         brush_vert_c *av)
{
  rFace_c * F = new rFace_c;

  F->kind = rFace_c::WALL;
  F->index = -1;

  F->side = S;

  F->z1 = z1;
  F->z2 = z2;

  SYS_ASSERT(av);

  F->face = &av->face;
///---  if (av)
///---  {
///---    F->w_face = av->face;
///---
///---    if (! F->w_face)
///---      F->w_face = av->parent->verts[0]->face;
///---  }

  return F;
}


static rFace_c * NewFace(int kind, double z, rWindingVerts_c *UU,
                         csg_property_set_c *face)
{
  rFace_c * F = new rFace_c;

  F->kind = kind;
  F->index = -1;

  SYS_ASSERT(face);

  F->face = face;

  F->z = z;
  F->UU = UU;

////  SYS_ASSERT(F->w_face);

  return F;
}


static void DoAddFace(rNode_c *LEAF, rSide_c *S, double z1, double z2,
                      brush_vert_c *av)
{
  if (! av) return;  // FIXME !!!!!! should not happen

  SYS_ASSERT(z2 > z1);

  // make sure face height does not exceed the limit
  if (true)
  {
    int num = 1 + (int)floor((z2 - z1) / (double)FACE_MAX_SIZE);

///fprintf(stderr, "Splitting tall face (%1.0f .. %1.0f) into %d pieces\n", z1, z2, num);

    SYS_ASSERT(num >= 1);

    for (int i = 0; i < num; i++)
    {
      double nz1 = z1 + (z2 - z1) *  i    / (double)num;
      double nz2 = z1 + (z2 - z1) * (i+1) / (double)num;

      rFace_c * F = NewFace(S, nz1, nz2, av);
      F->light = LEAF->light;

      LEAF->AddFace(F);
      S->on_partition->AddFace(F);
    }
  }
}


static void Side_BuildFaces(rNode_c *LEAF, rSide_c *S, merge_gap_c *G)
{
  if (! S->seg)
    return;

  if (! S->on_partition)
  {
    LogPrintf("WARNING: Side found which is not on any node.\n");
    return;
  }

  merge_segment_c * seg = S->seg;
  SYS_ASSERT(seg);

// fprintf(stderr, "SIDE (%1.0f %1.0f) --> (%1.0f %1.0f)\n", S->x1,S->y1, S->x2,S->y2);
// fprintf(stderr, "SEG  (%1.0f %1.0f) --> (%1.0f %1.0f)\n", seg->start->x,seg->start->y, seg->end->x,seg->end->y);
// fprintf(stderr, "SIDE %p  SEG %p  S->side:%d  f_sides:%u b:%u\n",
// S, seg, S->side, seg->f_sides.size(), seg->b_sides.size());

  // create the faces
  merge_region_c *RX = S->BackRegion();

  double z1 = G->GetZ1();
  double z2 = G->GetZ2();

  // FIXME: this is totally wrong, need to fix CSG2_FindSideVertex et al
  bool on_front = true; //!!!!!  (S->side == 1);

  // emergency fallback
  if (RX == NULL)
  {
    DoAddFace(LEAF, S, z1, z2, CSG2_FindSideVertex(seg, (z1+z2)/2.0, on_front));
    return;
  }

  // complex case: compare with solids on other side

  // FIXME: use brushes, not gaps

  for (int m = 0; m <= (int)RX->gaps.size(); m++)
  {
    double sz1 = (m == 0) ? -9e6 : RX->gaps[m-1]->GetZ2();
    double sz2 = (m == (int)RX->gaps.size()) ? +9e6 : RX->gaps[m]->GetZ1();

    if (sz1 < z1) sz1 = z1;
    if (sz2 > z2) sz2 = z2;

    if (sz2 < sz1 + 0.99)  // don't create tiny faces
      continue;

    brush_vert_c *V = CSG2_FindSideVertex(seg, (sz1+sz2)/2.0, on_front);
    if (! V) V = RX->brushes[0]->verts[0];

    DoAddFace(LEAF, S,  sz1, sz2, V);
  }
}



static rNode_c * Z_Leaf(rNode_c *winding, merge_region_c *R, int gap)
{
  rNode_c *LEAF = NewLeaf(CONTENTS_EMPTY);

  LEAF->gap = gap;
  LEAF->CopyBBox(winding);

  z_leafs.push_back(LEAF);

  SYS_ASSERT(R);
  SYS_ASSERT(0 <= gap && gap < (int)R->gaps.size());
  
  merge_gap_c *G = R->gaps[gap];

  if (gap == (int)R->gaps.size()-1 && G->t_brush->bkind == BKIND_Sky)
    LEAF->light = LIGHT_OUTDOOR;
  else if (G->GetZ2() > G->GetZ1()+150)
    LEAF->light = LIGHT_INDOOR;
  else
    LEAF->light = LIGHT_INDOOR * 2 / 3;

  for (unsigned int i = 0; i < winding->sides.size(); i++)
  {
    Side_BuildFaces(LEAF, winding->sides[i], G);
  }

  return LEAF;
}


static void AddFlatFace(rNode_c * N, int gap, int kind, csg_property_set_c *face,
                        rWindingVerts_c *UU)
{
if (UU->count < 3) return; //!!!!!!!!!!

  for (unsigned int k = 0; k < z_leafs.size(); k++)
  {
    rNode_c * LEAF = z_leafs[k];

    if (LEAF->gap == gap)
    {
      rFace_c *F = NewFace(kind, N->z, UU, face);
      F->light = LEAF->light;

      LEAF->AddFace(F);
      N->AddFace(F);

      return;
    }
  }

  LogPrintf("WARNING: missing leaf for flat surface\n");
}


// area number is:
//    0 for below the lowest floor,
//    1 for the first gap
//    2 for above the first gap
//    3 for the second gap
//    etc etc...

static rNode_c * Partition_Z(rNode_c *winding, merge_region_c *R,
                              int min_area, int max_area)
{
  SYS_ASSERT(min_area <= max_area);

  if (min_area == max_area)
  {
    if ((min_area & 1) == 0)
      return SOLID_LEAF;
    else
      return Z_Leaf(winding, R, min_area / 2);
  }


  {
    rNode_c *node = new rNode_c(true /* z_splitter */);

    node->dz = 1;
    node->winding = winding;

    int a1 = (min_area + max_area) / 2;
    int a2 = a1 + 1;

    int g = a1 / 2;
    SYS_ASSERT(g < (int)R->gaps.size());

    merge_gap_c *G = R->gaps[g];

    if (a1 & 1)
      node->z = G->GetZ2();
    else
      node->z = G->GetZ1();

    node->back  = Partition_Z(winding, R, min_area, a1);
    node->front = Partition_Z(winding, R, a2, max_area);

    if (! winding->wi_verts)
      winding->wi_verts = CollectClockwiseVerts(winding);

    AddFlatFace(node, g, (a1 & 1) ? rFace_c::CEIL : rFace_c::FLOOR,
                (a1 & 1) ? &G->t_brush->b.face : &G->b_brush->t.face,
                winding->wi_verts);

    return node;
  }
}


static rNode_c * SecondPass(rNode_c *LN)
{
  // Faces are built in a second pass after building the 2D BSP tree.
  // This is because Sides (and their partners) can get split by future
  // partition lines, and trying to manage faces in the face of that
  // becomes a nightmare.

  if (LN == SOLID_LEAF)
    return LN;

  if (LN->IsNode())
  {
    LN->front = SecondPass(LN->front);
    LN->back  = SecondPass(LN->back);

    return LN;
  }

  SYS_ASSERT(LN->region);
  SYS_ASSERT(LN->region->gaps.size() > 0);

  z_leafs.clear();

  return Partition_Z(LN, LN->region, 0, (int)LN->region->gaps.size() * 2);
}


//------------------------------------------------------------------------

// an "intersection" remembers the vertex that touches a BSP divider
// line (especially a new vertex that is created at a seg split).

// Note: two points can exist in the intersection list with
//       the same along value but different dirs.
typedef struct
{
  // how far along the partition line the vertex is.
  // bigger value are further along the partition line.
  double along;

  // quantized along value
  int q_dist;

  // direction that miniseg will touch, +1 for further along
  // the partition, and -1 for backwards on the partition.
  // The values +2 and -2 indicate REMOVED points.
  int dir;

  // this is only set after MergeIntersections().
  double next_along;
}
intersect_t;


struct intersect_qdist_Compare
{
  inline bool operator() (const intersect_t& A, const intersect_t& B) const
  {
    if (A.q_dist != B.q_dist)
      return A.q_dist < B.q_dist;

    return A.dir < B.dir;
  }
};


static void AddIntersection(std::vector<intersect_t> & cut_list,
                            double along, int dir)
{
  intersect_t new_cut;

  new_cut.along = along;
  new_cut.q_dist = I_ROUND(along * 21.6f);
  new_cut.dir = dir;
  new_cut.next_along = -1e30;

  cut_list.push_back(new_cut);
}


static void MergeIntersections(std::vector<intersect_t> & cut_list)
{
  if (cut_list.empty())
    return;

  // move input vector contents into a temporary vector, which we
  // sort and iterate over.  Valid intersections then get pushed
  // back into the input vector.

  std::vector<intersect_t> local_cuts;

  local_cuts.swap(cut_list);

  std::sort(local_cuts.begin(), local_cuts.end(),
            intersect_qdist_Compare());

  std::vector<intersect_t>::iterator A, B;

  A = local_cuts.begin();

  while (A != local_cuts.end())
  {
    if (A->dir != +1)
    {
      A++; continue;
    }

    B = A; B++;

    if (B == local_cuts.end())
      break;

    // this handles multiple +1 entries and also ensures
    // that the +2 "remove" entry kills a +1 entry.
    if (A->q_dist == B->q_dist)
    {
      A++; continue;
    }

    if (B->dir != -1)
    {
      DebugPrintf("WARNING: bad pair in intersection list\n");

      A = B; continue;
    }

    // found a viable intersection!
    A->next_along = B->along;

    cut_list.push_back(*A);

    B++; A = B; continue;
  }
}


//------------------------------------------------------------------------
//  NEW LOGIC
//------------------------------------------------------------------------


quake_node_c * qk_bsp_root;
quake_leaf_c * qk_solid_leaf;


class quake_side_c
{
public:
  snag_c *snag;

  bool on_node;

  double x1, y1;
  double x2, y2;

public:
  // this is only for partitions
  quake_side_c() : snag(NULL), on_node(false)
  { }

  quake_side_c(snag_c *S) :
      snag(S), on_node(false),
      x1(S->x1), y1(S->y1), x2(S->x2), y2(S->y2)
  { }

  quake_side_c(const quake_side_c *other) :
      snag(other->snag), on_node(other->on_node),
      x1(other->x1), y1(other->y1), x2(other->x2), y2(other->y2)
  { }

  // make a "mini side"
  quake_side_c(const quake_side_c *part, double along1, double along2) :
      snag(NULL), on_node(true)
  {
    double sx, sy;
    double ex, ey;

    AlongCoord(along1, part->x1,part->y1, part->x2,part->y2, &sx, &sy);
    AlongCoord(along2, part->x1,part->y1, part->x2,part->y2, &ex, &ey);

    x1 = sx; y1 = sy;
    x2 = ex; y2 = ey;
  }

  ~quake_side_c()
  { }

public:
  double Length() const
  {
    return ComputeDist(x1,y1, x2,y2);
  }

  bool TwoSided() const
  {
    if (! snag->partner)
      return false;

    if (! snag->partner->region)
      return false;

    if (snag->partner->region->gaps.empty())
      return false;

    return true;
  }

  void ToPlane(quake_plane_c *plane)
  {
    plane->x = x1;
    plane->y = y1;
    plane->z = 0;

    plane->nx = (y2 - y1);
    plane->ny = (x1 - x2);
    plane->nz = 0;

    plane->Normalize();
  }
};


class quake_group_c
{
public:
  std::vector<quake_side_c *> sides;

public:
  quake_group_c() : sides()
  { }

  ~quake_group_c()
  { }

  void AddSide(quake_side_c *S)
  {
    sides.push_back(S);
  }

  bool IsEmpty() const
  {
    return sides.empty();
  }
};


void quake_plane_c::Normalize()
{
  double len = sqrt(nx*nx + ny*ny + nz*nz);

  if (len > 0.000001)
  {
    nx /= len;
    ny /= len;
    nz /= len;
  }
}


quake_node_c::quake_node_c(const quake_plane_c& P) :
    plane(P),
    front_N(NULL), front_L(NULL),
     back_N(NULL),  back_L(NULL),
    faces(), index(-1)
{ }


static void CreateSides(quake_group_c & group)
{
fprintf(stderr, "CREATE SIDES\n");
  for (unsigned int i = 0 ; i < all_regions.size() ; i++)
  {
    region_c *R = all_regions[i];

    if (R->gaps.empty())
      continue;

    for (unsigned int k = 0 ; k < R->snags.size() ; k++)
    {
      snag_c *snag = R->snags[k];

      region_c *N = snag->partner ? snag->partner->region : NULL;

      if (N && R->HasSameBrushes(N))
        continue;

      quake_side_c *S = new quake_side_c(S);
fprintf(stderr, "New Side: %p %s (%1.0f %1.0f) .. (%1.0f %1.0f)\n",
        S, S->TwoSided() ? "2S" : "1S",
        S->x1, S->y1, S->x2, S->y2);

      group.AddSide(S);
    }
  }

fprintf(stderr, "\n");
}


static void CreateMiniSides(std::vector<intersect_t> & cut_list,
                            const quake_side_c *part,
                            quake_group_c & front, quake_group_c & back)
{
  for (unsigned int i = 0 ; i < cut_list.size() ; i++)
  {
    double along1 = cut_list[i].along;
    double along2 = cut_list[i].next_along;

    SYS_ASSERT(along1 < along2);

    quake_side_c *F = new quake_side_c(part, along1, along2);
    quake_side_c *B = new quake_side_c(part, along2, along1);

    front.AddSide(F);
     back.AddSide(B);
  }
}


static quake_side_c * SplitSideAt(quake_side_c *S, float new_x, float new_y)
{
  quake_side_c *T = new quake_side_c(S);

  S->x2 = T->x1 = new_x;
  S->y2 = T->y1 = new_y;

  return T;
}


// what: 0 for start, 1 for end
static inline double P_Along(const quake_side_c *p, const quake_side_c *S, int what)
{
  if (what == 0)
    return AlongDist(S->x1, S->y1, p->x1, p->y1, p->x2, p->y2);
  else
    return AlongDist(S->x2, S->y2, p->x1, p->y1, p->x2, p->y2);
}


static void Split_XY(quake_group_c & group, const quake_side_c *part,
                     quake_group_c & front, quake_group_c & back)
{
  std::vector<intersect_t> cut_list;

  std::vector<quake_side_c *> local_sides;

  local_sides.swap(group.sides);

  for (unsigned int k = 0 ; k < local_sides.size() ; k++)
  {
    quake_side_c *S = local_sides[k];

    // get relationship of this side to the partition line
    double a = PerpDist(S->x1, S->y1,
                        part->x1,part->y1, part->x2,part->y2);

    double b = PerpDist(S->x2, S->y2,
                        part->x1,part->y1, part->x2,part->y2);

    int a_side = (a < -Q_EPSILON) ? -1 : (a > Q_EPSILON) ? +1 : 0;
    int b_side = (b < -Q_EPSILON) ? -1 : (b > Q_EPSILON) ? +1 : 0;

    if (a_side == 0 && b_side == 0)
    {
      // side sits on the partition
      S->on_node = true;

      if (VectorSameDir(part->x2 - part->x1, part->y2 - part->y1,
                        S->x2 - S->x1, S->y2 - S->y1))
      {
        front.AddSide(S);

        // +2 and -2 mean "remove"
        AddIntersection(cut_list, P_Along(part, S, 0), +2);
        AddIntersection(cut_list, P_Along(part, S, 1), -2);
      }
      else
      {
        back.AddSide(S);

        AddIntersection(cut_list, P_Along(part, S, 0), -2);
        AddIntersection(cut_list, P_Along(part, S, 1), +2);
      }
      continue;
    }

    if (a_side >= 0 && b_side >= 0)
    {
      front.AddSide(S);

      if (a_side == 0)
        AddIntersection(cut_list, P_Along(part, S, 0), -1);
      else if (b_side == 0)
        AddIntersection(cut_list, P_Along(part, S, 1), +1);

      continue;
    }

    if (a_side <= 0 && b_side <= 0)
    {
      back.AddSide(S);

      if (a_side == 0)
        AddIntersection(cut_list, P_Along(part, S, 0), +1);
      else if (b_side == 0)
        AddIntersection(cut_list, P_Along(part, S, 1), -1);

      continue;
    }

    /* need to split it */

    // determine the intersection point
    double along = a / (a - b);

    double ix = S->x1 + along * (S->x2 - S->x1);
    double iy = S->y1 + along * (S->y2 - S->y1);

    quake_side_c *T = SplitSideAt(S, ix, iy);

    front.AddSide((a > 0) ? S : T);
     back.AddSide((a > 0) ? T : S);

    AddIntersection(cut_list, P_Along(part, T, 0), a_side);
  }

  MergeIntersections(cut_list);

  CreateMiniSides(cut_list, part, front, back);
}


static bool FindPartition_XY(quake_group_c & group, quake_side_c *part)
{
  quake_side_c *poss = NULL;
  quake_side_c *best = NULL;

  for (unsigned int i = 0 ; i < group.sides.size() ; i++)
  {
    quake_side_c *S = group.sides[i];

    if (S->on_node)
      continue;

    poss = S;

    // MUST choose 2-sided snag BEFORE any 1-sided snag

    if (S->TwoSided())
    {
      best = S; break;  // !!!!! FIXME: decide properly
    }
  }

  if (! poss)
    return false;

  if (! best)
    best = poss;

  part->x1 = best->x1;  part->y1 = best->y1;
  part->x2 = best->x2;  part->y2 = best->y2;

  return true;
}


static void FlatToPlane(quake_plane_c *plane, const gap_c *G, bool is_ceil)
{
  // FIXME: support slopes !!

  plane->x  = plane->y  = 0;
  plane->nx = plane->ny = 0;

  plane->z  = is_ceil ? G->top->b.z : G->bottom->t.z;
  plane->nz = is_ceil ? -1 : 1;

  /// NOT NEEDED (YET) : plane->Normalize();
}


static void Partition_Z(region_c *R,
                        quake_node_c ** node,
                        quake_leaf_c ** leaf)
{
  SYS_ASSERT(R->gaps.size() > 0);

  quake_node_c *cur_node = NULL;
  quake_leaf_c *cur_leaf = new quake_leaf_c(CONTENTS_EMPTY);
  
  for (int surf = (int)R->gaps.size() * 2 - 1 ; surf >= 0 ; surf--)
  {
    gap_c *G = R->gaps[surf / 2];

    bool is_ceil = (surf & 1) ? true : false;

    quake_node_c *last_node = cur_node;
    quake_leaf_c *last_leaf = cur_leaf;

    cur_node = new quake_node_c();
    cur_leaf = NULL;

    FlatToPlane(&cur_node->plane, G, is_ceil);

    cur_node->front_N = last_node;
    cur_node->front_L = last_leaf;

    cur_node->back_L  = qk_solid_leaf;

///---  if (is_ceil)
///---    std::swap(node->front, node->back);
  }

  *node = cur_node;
  *leaf = cur_leaf;
}


static void Partition_Group(quake_group_c & group,
                            quake_node_c ** node,
                            quake_leaf_c ** leaf)
{
  // this function "returns" either a node OR a leaf via the
  // parameters with the same name.

  SYS_ASSERT(! group.sides.empty());

  quake_side_c part;
  quake_plane_c p_plane;

  if (FindPartition_XY(group, &part))
  {
    // divide the group
    quake_group_c front;
    quake_group_c back;

    Split_XY(group, &part, front, back);

    part.ToPlane(&p_plane);

    quake_node_c * new_node = new quake_node_c(p_plane);

    // the front should never be empty
    Partition_Group(front, &new_node->front_N, &new_node->front_L);

    if (back.sides.empty())
      new_node->back_L = qk_solid_leaf;
    else
      Partition_Group(back, &new_node->back_N, &new_node->back_L);

    // input group has been consumed now 

    (*node) = new_node;
  }
  else
  {
    region_c *region = group.sides[0]->snag->region;

    SYS_ASSERT(region);

    Partition_Z(region, node, leaf);
  }
}


//------------------------------------------------------------------------

void quake_bbox_c::Begin()
{
  for (int b = 0 ; b < 3 ; b++)
  {
    mins[b] = +9e9;
    maxs[b] = -9e9;
  }
}

void quake_bbox_c::End()
{
  for (int b = 0 ; b < 3 ; b++)
    if (mins[b] > maxs[b])
      mins[b] = maxs[b] = 0;
}


void quake_bbox_c::AddPoint(float x, float y, float z)
{
  if (x < mins[0]) mins[0] = x;
  if (y < mins[1]) mins[1] = y;
  if (z < mins[2]) mins[2] = z;

  if (x > maxs[0]) maxs[0] = x;
  if (y > maxs[1]) maxs[1] = y;
  if (z > maxs[2]) maxs[2] = z;
}

void quake_bbox_c::Merge(const quake_bbox_c& other)
{
  for (int b = 0 ; b < 3 ; b++)
  {
    mins[b] = MIN(mins[b], other.mins[b]);
    maxs[b] = MAX(maxs[b], other.maxs[b]);
  }
}


void quake_node_c::ComputeBBox()
{
  // NOTE: assumes bbox of all children (nodes/leafs) are valid

  bbox.Begin();

  if (front_N)
    bbox.Merge(front_N->bbox);
  else if (front_L != qk_solid_leaf)
    bbox.Merge(front_L->bbox);

  if (back_N)
    bbox.Merge(back_N->bbox);
  else if (back_L != qk_solid_leaf)
    bbox.Merge(back_L->bbox);

  bbox.End();
}


static void AssignLeafIndex(quake_leaf_c *leaf, int *cur_leaf)
{
  SYS_ASSERT(leaf);

  if (leaf == qk_solid_leaf)
    return;

  // must add 2 (instead of 1) because leaf #0 is the SOLID_LEAF
  leaf->index = -((*cur_leaf)+2);

  *cur_leaf += 1;
}


static void AssignIndexes(quake_node_c *node, int *cur_node, int *cur_leaf)
{
  node->index = *cur_node;

  *cur_node += 1;

  if (node->front_N)
    AssignIndexes(node->front_N, cur_node, cur_leaf);
  else
    AssignLeafIndex(node->front_L, cur_leaf);

  if (node->back_N)
    AssignIndexes(node->back_N, cur_node, cur_leaf);
  else
    AssignLeafIndex(node->back_L, cur_leaf);

  // determine node's bounding box now
  node->ComputeBBox();
}


#if 0
static void AssignClusterID(rNode_c *node, int cluster)
{
  if (node == SOLID_LEAF)
    return;

  node->cluster = cluster;

  if (node->IsNode())
  {
    AssignClusterID(node->front, cluster);
    AssignClusterID(node->back,  cluster);
  }
}

static void AssignClusters(rNode_c *node)
{
  if (node == SOLID_LEAF)
    return;

  if (node->IsLeaf() || MAX(node->BBoxSizeX(), node->BBoxSizeY()) <= FACE_MAX_SIZE)
  {
    AssignClusterID(node, q1_total_clusters);
    q1_total_clusters += 1;
    return;
  }

  AssignClusters(node->front);
  AssignClusters(node->back);
}
#endif


static void Quake_BSP()
{
  qk_solid_leaf = new quake_leaf_c(CONTENTS_SOLID);


  quake_group_c GROUP;

  CreateSides(GROUP);

  quake_leaf_c *root_L = NULL;

  Partition_Group(GROUP, &qk_bsp_root, &root_L);

  SYS_ASSERT(qk_bsp_root);


  int cur_node = 0;
  int cur_leaf = 0;

  AssignIndexes(qk_bsp_root, &cur_node, &cur_leaf);


  //??  MakeFaces(qk_bsp_root);
}



void Q1_CreateModel(void)
{
#if 0
  rSideFactory_c::FreeAll();
  all_windings.clear();

// fprintf(stderr, "Q1_BuildBSP...\n");
  OLD_Q1_BuildBSP();

  qLump_c *lump = BSP_NewLump(LUMP_MODELS);

  memset(&model, 0, sizeof(model));

  q1_total_nodes = 0;
  q1_total_leafs = 0;  // does not include the solid leaf
  q1_total_faces = 0;

  q1_total_mark_surfs = 0;
  q1_total_surf_edges = 0;
  q1_total_clusters   = 0;

  q1_nodes = BSP_NewLump(LUMP_NODES);
  q1_leafs = BSP_NewLump(LUMP_LEAFS);
  q1_faces = BSP_NewLump(LUMP_FACES);

  q_mark_surfs = BSP_NewLump(LUMP_MARKSURFACES);
  q_surf_edges = BSP_NewLump(LUMP_SURFEDGES);

// fprintf(stderr, "Second Pass...\n");
  SecondPass(R_ROOT);

  AssignIndexes(R_ROOT);
  AssignClusters(R_ROOT);  // needs bboxes from AssignIndexes

  LogPrintf("render hull: %d nodes, %d leafs, %d clusters\n",
          q1_total_nodes, q1_total_leafs, q1_total_clusters);

  if (q1_total_nodes >= MAX_MAP_NODES)
    Main_FatalError("Quake1 build failure: exceeded limit of %d NODES\n",
                    MAX_MAP_NODES);

  if (q1_total_leafs >= MAX_MAP_LEAFS)
    Main_FatalError("Quake1 build failure: exceeded limit of %d LEAFS\n",
                    MAX_MAP_LEAFS);

  WriteSolidLeaf();

// fprintf(stderr, "Write Nodes...\n");
  WriteNodes(R_ROOT);

  if (q1_total_faces >= MAX_MAP_FACES)
    Main_FatalError("Quake1 build failure: exceeded limit of %d FACES\n",
                    MAX_MAP_FACES);

  model.numfaces = q1_total_faces;


  // set model bounding box
  model.mins[0] = bounds_x1;  model.maxs[0] = bounds_x2;
  model.mins[1] = bounds_y1;  model.maxs[1] = bounds_y2;
  model.mins[2] = bounds_z1;  model.maxs[2] = bounds_z2;

  model.origin[0] = 0;
  model.origin[1] = 0;
  model.origin[2] = 0;


  // clipping hulls
  model.visleafs = q1_total_leafs;

  q_clip_nodes = BSP_NewLump(LUMP_CLIPNODES);

  model.headnode[0] = 0; // root of drawing BSP
  model.headnode[1] = Q1_CreateClipHull(1, q_clip_nodes);
  model.headnode[2] = Q1_CreateClipHull(2, q_clip_nodes);
  model.headnode[3] = Q1_CreateClipHull(3, q_clip_nodes);


  // TODO: fix endianness in model
  lump->Append(&model, sizeof(model));

  Q1_CreateSubModels(lump, model.numfaces, model.visleafs);


  delete R_ROOT;  R_ROOT = NULL;
  delete SOLID_LEAF;  SOLID_LEAF = NULL;

// fprintf(stderr, "DONE.\n");

  // there is no need to delete the lumps from BSP_NewLump()
  // since is handled by the q_common.c code.
#endif
}



void CSG_QUAKE_Build()
{
  if (main_win)
    main_win->build_box->Prog_Step("CSG");

  CSG_BSP(1.0);

  CSG_MakeMiniMap();

  if (main_win)
    main_win->build_box->Prog_Step("BSP");

  Quake_BSP();
}


//--- editor settings ---
// vi:ts=2:sw=2:expandtab
