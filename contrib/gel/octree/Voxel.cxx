// This is gel/octree/Voxel.cxx
#ifdef VCL_NEEDS_PRAGMA_INTERFACE
#pragma implementation
#endif
//:
// \file
// \author Geoffrey Cross, Oxford RRG
// \date 20 May 99
// \verbatim
//  Modifications:
//   990520 Geoff Initial version.
// \endverbatim
//-----------------------------------------------------------------------------

#include "Voxel.h"
#include <vcl_iostream.h>
#include <vcl_cassert.h>

#if 0
bool operator==( const Voxel &a, const Voxel &b)
{
  return a.depth==b.depth && a.x==b.x && a.y==b.y && a.z==b.z;
}
#endif

vcl_ostream &operator<<( vcl_ostream &os, const Voxel &a)
{
  return os << a.depth << ' ' << a.x << ' ' << a.y << ' ' << a.z;
}

bool operator<( Voxel & /* v1 */, Voxel & /* v2 */)
{
  assert(0);
  return false;
}
