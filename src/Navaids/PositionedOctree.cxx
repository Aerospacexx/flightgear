/**
 * PositionedOctree - define a spatial octree containing Positioned items
 * arranged by their global cartesian position.
 */
 
// Written by James Turner, started 2012.
//
// Copyright (C) 2012 James Turner
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "PositionedOctree.hxx"
#include "positioned.hxx"

#include <cassert>
#include <algorithm> // for sort
#include <cstring> // for memset
#include <iostream>

#include <boost/foreach.hpp>

#include <simgear/debug/logstream.hxx>

namespace flightgear
{

namespace Octree
{
  
Node* global_spatialOctree = NULL;

Leaf::Leaf(const SGBoxd& aBox, int64_t aIdent) :
  Node(aBox, aIdent),
  childrenLoaded(false)
{
}
  
void Leaf::visit(const SGVec3d& aPos, double aCutoff,
                   FGPositioned::Filter* aFilter,
                   FindNearestResults& aResults, FindNearestPQueue&)
{
  int previousResultsSize = aResults.size();
  int addedCount = 0;
  NavDataCache* cache = NavDataCache::instance();
  
  loadChildren();
  
  ChildMap::const_iterator it = children.lower_bound(aFilter->minType());
  ChildMap::const_iterator end = children.upper_bound(aFilter->maxType());
  
  for (; it != end; ++it) {
    FGPositioned* p = cache->loadById(it->second);
    assert(intersects(_box, p->cart()));
    double d = dist(aPos, p->cart());
    if (d > aCutoff) {
      continue;
    }
    
    if (aFilter && !aFilter->pass(p)) {
      continue;
    }
    
    ++addedCount;
    aResults.push_back(OrderedPositioned(p, d));
  }
  
  if (addedCount == 0) {
    return;
  }
  
  // keep aResults sorted
  // sort the new items, usually just one or two items
  std::sort(aResults.begin() + previousResultsSize, aResults.end());
  
  // merge the two sorted ranges together - in linear time
  std::inplace_merge(aResults.begin(),
                     aResults.begin() + previousResultsSize, aResults.end());
}

void Leaf::insertChild(FGPositioned::Type ty, PositionedID id)
{
  assert(childrenLoaded);
  children.insert(children.end(), TypedPositioned(ty, id));
}
  
void Leaf::loadChildren()
{
  if (childrenLoaded) {
    return;
  }
  
  NavDataCache* cache = NavDataCache::instance();
  BOOST_FOREACH(TypedPositioned tp, cache->getOctreeLeafChildren(guid())) {
    children.insert(children.end(), tp);
  } // of leaf members iteration
  
  childrenLoaded = true;
}
  
Branch::Branch(const SGBoxd& aBox, int64_t aIdent) :
  Node(aBox, aIdent),
  childrenLoaded(false)
{
  memset(children, 0, sizeof(Node*) * 8);
}

void Branch::visit(const SGVec3d& aPos, double aCutoff,
                   FGPositioned::Filter*,
                   FindNearestResults&, FindNearestPQueue& aQ)
{
  loadChildren();
  for (unsigned int i=0; i<8; ++i) {
    if (!children[i]) {
      continue;
    }
    
    double d = children[i]->distToNearest(aPos);
    if (d > aCutoff) {
      continue; // exceeded cutoff
    }
    
    aQ.push(Ordered<Node*>(children[i], d));
  } // of child iteration
}

Node* Branch::childForPos(const SGVec3d& aCart) const
{
  assert(contains(aCart));
  int childIndex = 0;
  
  SGVec3d center(_box.getCenter());
// tests must match indices in SGbox::getCorner
  if (aCart.x() < center.x()) {
    childIndex += 1;
  }
  
  if (aCart.y() < center.y()) {
    childIndex += 2;
  }
  
  if (aCart.z() < center.z()) {
    childIndex += 4;
  }
  
  return childAtIndex(childIndex);
}

Node* Branch::childAtIndex(int childIndex) const
{
  Node* child = children[childIndex];
  if (!child) { // lazy building of children
    SGBoxd cb(boxForChild(childIndex));
    double d2 = dot(cb.getSize(), cb.getSize());
    
    assert(((_ident << 3) >> 3) == _ident);
    
    // child index is 0..7, so 3-bits is sufficient, and hence we can
    // pack 20 levels of octree into a int64, which is plenty
    int64_t childIdent = (_ident << 3) | childIndex;
    
    if (d2 < LEAF_SIZE_SQR) {
      child = new Leaf(cb, childIdent);
    } else {
      child = new Branch(cb, childIdent);
    }
    
    children[childIndex] = child;
    
    if (childrenLoaded) {
    // childrenLoad is done, so we're defining a new node - add it to the
    // cache too.
      NavDataCache::instance()->defineOctreeNode(const_cast<Branch*>(this), child);
    }
  }

  return children[childIndex];
}
  
void Branch::loadChildren() const
{
  if (childrenLoaded) {
    return;
  }
  
  int childrenMask = NavDataCache::instance()->getOctreeBranchChildren(guid());
  for (int i=0; i<8; ++i) {
    if ((1 << i) & childrenMask) {
      childAtIndex(i); // accessing will create!
    }
  } // of child index iteration
  
// set this after creating the child nodes, so the cache update logic
// in childAtIndex knows any future created children need to be added.
  childrenLoaded = true;
}
  
int Branch::childMask() const
{
  int result = 0;
  for (int i=0; i<8; ++i) {
    if (children[i]) {
      result |= 1 << i;
    }
  }
  
  return result;
}

void findNearestN(const SGVec3d& aPos, unsigned int aN, double aCutoffM, FGPositioned::Filter* aFilter, FGPositioned::List& aResults)
{
  aResults.clear();
  FindNearestPQueue pq;
  FindNearestResults results;
  pq.push(Ordered<Node*>(global_spatialOctree, 0));
  double cut = aCutoffM;

  while (!pq.empty()) {
    if (!results.empty()) {
      // terminate the search if we have sufficent results, and we are
      // sure no node still on the queue contains a closer match
      double furthestResultOrder = results.back().order();
    //  std::cout << "furthest result:" << furthestResultOrder << ", top node order:" << pq.top().order() << std::endl;
      if ((results.size() >= aN) && (furthestResultOrder < pq.top().order())) {
        break;
      }
    }
  
    Node* nd = pq.top().get();
    pq.pop();
  
//    std::cout << "visiting:" << std::oct << nd->guid() << "(" << std::dec << nd->guid() << ")" << std::endl;
    nd->visit(aPos, cut, aFilter, results, pq);
  } // of queue iteration

  // depending on leaf population, we may have (slighty) more results
  // than requested
  unsigned int numResults = std::min((unsigned int) results.size(), aN);
  // copy results out
  aResults.resize(numResults);
  for (unsigned int r=0; r<numResults; ++r) {
    aResults[r] = results[r].get();
  }
}

void findAllWithinRange(const SGVec3d& aPos, double aRangeM, FGPositioned::Filter* aFilter, FGPositioned::List& aResults)
{
  aResults.clear();
  FindNearestPQueue pq;
  FindNearestResults results;
  pq.push(Ordered<Node*>(global_spatialOctree, 0));
  double rng = aRangeM;

  while (!pq.empty()) {
    Node* nd = pq.top().get();
    pq.pop();
  
    nd->visit(aPos, rng, aFilter, results, pq);
  } // of queue iteration

  unsigned int numResults = results.size();
  // copy results out
  aResults.resize(numResults);
  for (unsigned int r=0; r<numResults; ++r) {
    aResults[r] = results[r].get();
  }
}
      
} // of namespace Octree

} // of namespace flightgear
