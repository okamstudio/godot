// Copyright 2024 The Manifold Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Derived from the public domain work of Antti Kuukka at
// https://github.com/akuukka/quickhull

#include "quickhull.h"

#include <algorithm>
#include <limits>

#include "./impl.h"

namespace manifold {

double defaultEps() { return 0.0000001; }

inline double getSquaredDistanceBetweenPointAndRay(const vec3& p,
                                                   const Ray& r) {
  const vec3 s = p - r.S;
  double t = la::dot(s, r.V);
  return la::dot(s, s) - t * t * r.VInvLengthSquared;
}

inline double getSquaredDistance(const vec3& p1, const vec3& p2) {
  return la::dot(p1 - p2, p1 - p2);
}
// Note that the unit of distance returned is relative to plane's normal's
// length (divide by N.getNormalized() if needed to get the "real" distance).
inline double getSignedDistanceToPlane(const vec3& v, const Plane& p) {
  return la::dot(p.N, v) + p.D;
}

inline vec3 getTriangleNormal(const vec3& a, const vec3& b, const vec3& c) {
  // We want to get (a-c).crossProduct(b-c) without constructing temp vectors
  double x = a.x - c.x;
  double y = a.y - c.y;
  double z = a.z - c.z;
  double rhsx = b.x - c.x;
  double rhsy = b.y - c.y;
  double rhsz = b.z - c.z;
  double px = y * rhsz - z * rhsy;
  double py = z * rhsx - x * rhsz;
  double pz = x * rhsy - y * rhsx;
  return la::normalize(vec3(px, py, pz));
}

size_t MeshBuilder::addFace() {
  if (disabledFaces.size()) {
    size_t index = disabledFaces.back();
    auto& f = faces[index];
    DEBUG_ASSERT(f.isDisabled(), logicErr, "f should be disabled");
    DEBUG_ASSERT(!f.pointsOnPositiveSide, logicErr,
                 "f should not be on the positive side");
    f.mostDistantPointDist = 0;
    disabledFaces.pop_back();
    return index;
  }
  faces.emplace_back();
  return faces.size() - 1;
}

size_t MeshBuilder::addHalfedge() {
  if (disabledHalfedges.size()) {
    const size_t index = disabledHalfedges.back();
    disabledHalfedges.pop_back();
    return index;
  }
  halfedges.push_back({});
  halfedgeToFace.push_back(0);
  halfedgeNext.push_back(0);
  return halfedges.size() - 1;
}

void MeshBuilder::setup(int a, int b, int c, int d) {
  faces.clear();
  halfedges.clear();
  halfedgeToFace.clear();
  halfedgeNext.clear();
  disabledFaces.clear();
  disabledHalfedges.clear();

  faces.reserve(4);
  halfedges.reserve(12);

  // Create halfedges
  // AB
  halfedges.push_back({0, b, 6});
  halfedgeToFace.push_back(0);
  halfedgeNext.push_back(1);
  // BC
  halfedges.push_back({0, c, 9});
  halfedgeToFace.push_back(0);
  halfedgeNext.push_back(2);
  // CA
  halfedges.push_back({0, a, 3});
  halfedgeToFace.push_back(0);
  halfedgeNext.push_back(0);
  // AC
  halfedges.push_back({0, c, 2});
  halfedgeToFace.push_back(1);
  halfedgeNext.push_back(4);
  // CD
  halfedges.push_back({0, d, 11});
  halfedgeToFace.push_back(1);
  halfedgeNext.push_back(5);
  // DA
  halfedges.push_back({0, a, 7});
  halfedgeToFace.push_back(1);
  halfedgeNext.push_back(3);
  // BA
  halfedges.push_back({0, a, 0});
  halfedgeToFace.push_back(2);
  halfedgeNext.push_back(7);
  // AD
  halfedges.push_back({0, d, 5});
  halfedgeToFace.push_back(2);
  halfedgeNext.push_back(8);
  // DB
  halfedges.push_back({0, b, 10});
  halfedgeToFace.push_back(2);
  halfedgeNext.push_back(6);
  // CB
  halfedges.push_back({0, b, 1});
  halfedgeToFace.push_back(3);
  halfedgeNext.push_back(10);
  // BD
  halfedges.push_back({0, d, 8});
  halfedgeToFace.push_back(3);
  halfedgeNext.push_back(11);
  // DC
  halfedges.push_back({0, c, 4});
  halfedgeToFace.push_back(3);
  halfedgeNext.push_back(9);

  // Create faces
  faces.emplace_back(0);
  faces.emplace_back(3);
  faces.emplace_back(6);
  faces.emplace_back(9);
}

std::array<int, 3> MeshBuilder::getVertexIndicesOfFace(const Face& f) const {
  std::array<int, 3> v;
  size_t index = f.he;
  auto* he = &halfedges[index];
  v[0] = he->endVert;

  index = halfedgeNext[index];
  he = &halfedges[index];
  v[1] = he->endVert;

  index = halfedgeNext[index];
  he = &halfedges[index];
  v[2] = he->endVert;
  return v;
}

HalfEdgeMesh::HalfEdgeMesh(const MeshBuilder& builderObject,
                           const VecView<vec3>& vertexData) {
  std::unordered_map<size_t, size_t> faceMapping;
  std::unordered_map<size_t, size_t> halfEdgeMapping;
  std::unordered_map<size_t, size_t> vertexMapping;

  size_t i = 0;
  for (const auto& face : builderObject.faces) {
    if (!face.isDisabled()) {
      halfEdgeIndexFaces.emplace_back(static_cast<size_t>(face.he));
      faceMapping[i] = halfEdgeIndexFaces.size() - 1;

      const auto heIndices = builderObject.getHalfEdgeIndicesOfFace(face);
      for (const auto heIndex : heIndices) {
        const auto vertexIndex = builderObject.halfedges[heIndex].endVert;
        if (vertexMapping.count(vertexIndex) == 0) {
          vertices.push_back(vertexData[vertexIndex]);
          vertexMapping[vertexIndex] = vertices.size() - 1;
        }
      }
    }
    i++;
  }

  i = 0;
  for (const auto& halfEdge : builderObject.halfedges) {
    if (halfEdge.pairedHalfedge != -1) {
      halfedges.push_back({halfEdge.endVert, halfEdge.pairedHalfedge,
                           builderObject.halfedgeToFace[i]});
      halfedgeToFace.push_back(builderObject.halfedgeToFace[i]);
      halfedgeNext.push_back(builderObject.halfedgeNext[i]);
      halfEdgeMapping[i] = halfedges.size() - 1;
    }
    i++;
  }

  for (auto& halfEdgeIndexFace : halfEdgeIndexFaces) {
    DEBUG_ASSERT(halfEdgeMapping.count(halfEdgeIndexFace) == 1, logicErr,
                 "invalid halfedge mapping");
    halfEdgeIndexFace = halfEdgeMapping[halfEdgeIndexFace];
  }

  for (size_t i = 0; i < halfedges.size(); i++) {
    auto& he = halfedges[i];
    halfedgeToFace[i] = faceMapping[halfedgeToFace[i]];
    he.pairedHalfedge = halfEdgeMapping[he.pairedHalfedge];
    halfedgeNext[i] = halfEdgeMapping[halfedgeNext[i]];
    he.endVert = vertexMapping[he.endVert];
  }
}

/*
 * Implementation of the algorithm
 */
std::pair<Vec<Halfedge>, Vec<vec3>> QuickHull::buildMesh(double epsilon) {
  if (originalVertexData.size() == 0) {
    return {Vec<Halfedge>(), Vec<vec3>()};
  }

  // Very first: find extreme values and use them to compute the scale of the
  // point cloud.
  extremeValues = getExtremeValues();
  scale = getScale(extremeValues);

  // Epsilon we use depends on the scale
  m_epsilon = epsilon * scale;
  epsilonSquared = m_epsilon * m_epsilon;

  // The planar case happens when all the points appear to lie on a two
  // dimensional subspace of R^3.
  planar = false;
  createConvexHalfedgeMesh();
  if (planar) {
    const int extraPointIndex = planarPointCloudTemp.size() - 1;
    for (auto& he : mesh.halfedges) {
      if (he.endVert == extraPointIndex) {
        he.endVert = 0;
      }
    }
    planarPointCloudTemp.clear();
  }

  // reorder halfedges
  Vec<Halfedge> halfedges(mesh.halfedges.size());
  Vec<int> halfedgeToFace(mesh.halfedges.size());
  Vec<int> counts(mesh.halfedges.size(), 0);
  Vec<int> mapping(mesh.halfedges.size());
  Vec<int> faceMap(mesh.faces.size());

  // Some faces are disabled and should not go into the halfedge vector, we can
  // update the face indices of the halfedges at the end using index/3
  int j = 0;
  for_each(
      autoPolicy(mesh.halfedges.size()), countAt(0_uz),
      countAt(mesh.halfedges.size()), [&](size_t i) {
        if (mesh.halfedges[i].pairedHalfedge < 0) return;
        if (mesh.faces[mesh.halfedgeToFace[i]].isDisabled()) return;
        if (AtomicAdd(counts[mesh.halfedgeToFace[i]], 1) > 0) return;
        int currIndex = AtomicAdd(j, 3);
        mapping[i] = currIndex;
        halfedges[currIndex + 0] = mesh.halfedges[i];
        halfedgeToFace[currIndex + 0] = mesh.halfedgeToFace[i];

        size_t k = mesh.halfedgeNext[i];
        mapping[k] = currIndex + 1;
        halfedges[currIndex + 1] = mesh.halfedges[k];
        halfedgeToFace[currIndex + 1] = mesh.halfedgeToFace[k];

        k = mesh.halfedgeNext[k];
        mapping[k] = currIndex + 2;
        halfedges[currIndex + 2] = mesh.halfedges[k];
        halfedgeToFace[currIndex + 2] = mesh.halfedgeToFace[k];
        halfedges[currIndex + 0].startVert = halfedges[currIndex + 2].endVert;
        halfedges[currIndex + 1].startVert = halfedges[currIndex + 0].endVert;
        halfedges[currIndex + 2].startVert = halfedges[currIndex + 1].endVert;
      });
  halfedges.resize(j);
  halfedgeToFace.resize(j);
  // fix pairedHalfedge id
  for_each(
      autoPolicy(halfedges.size()), halfedges.begin(), halfedges.end(),
      [&](Halfedge& he) { he.pairedHalfedge = mapping[he.pairedHalfedge]; });
  counts.resize(originalVertexData.size() + 1);
  fill(counts.begin(), counts.end(), 0);

  // remove unused vertices
  for_each(autoPolicy(halfedges.size() / 3), countAt(0_uz),
           countAt(halfedges.size() / 3), [&](size_t i) {
             AtomicAdd(counts[halfedges[3 * i].startVert], 1);
             AtomicAdd(counts[halfedges[3 * i + 1].startVert], 1);
             AtomicAdd(counts[halfedges[3 * i + 2].startVert], 1);
           });
  auto saturate = [](int c) { return c > 0 ? 1 : 0; };
  exclusive_scan(TransformIterator(counts.begin(), saturate),
                 TransformIterator(counts.end(), saturate), counts.begin(), 0);
  Vec<vec3> vertices(counts.back());
  for_each(autoPolicy(originalVertexData.size()), countAt(0_uz),
           countAt(originalVertexData.size()), [&](size_t i) {
             if (counts[i + 1] - counts[i] > 0) {
               vertices[counts[i]] = originalVertexData[i];
             }
           });
  for_each(autoPolicy(halfedges.size()), halfedges.begin(), halfedges.end(),
           [&](Halfedge& he) {
             he.startVert = counts[he.startVert];
             he.endVert = counts[he.endVert];
           });
  return {std::move(halfedges), std::move(vertices)};
}

void QuickHull::createConvexHalfedgeMesh() {
  visibleFaces.clear();
  horizonEdgesData.clear();
  possiblyVisibleFaces.clear();

  // Compute base tetrahedron
  setupInitialTetrahedron();
  DEBUG_ASSERT(mesh.faces.size() == 4, logicErr, "not a tetrahedron");

  // Init face stack with those faces that have points assigned to them
  faceList.clear();
  for (size_t i = 0; i < 4; i++) {
    auto& f = mesh.faces[i];
    if (f.pointsOnPositiveSide && f.pointsOnPositiveSide->size() > 0) {
      faceList.push_back(i);
      f.inFaceStack = 1;
    }
  }

  // Process faces until the face list is empty.
  size_t iter = 0;
  while (!faceList.empty()) {
    iter++;
    if (iter == std::numeric_limits<size_t>::max()) {
      // Visible face traversal marks visited faces with iteration counter (to
      // mark that the face has been visited on this iteration) and the max
      // value represents unvisited faces. At this point we have to reset
      // iteration counter. This shouldn't be an issue on 64 bit machines.
      iter = 0;
    }

    const auto topFaceIndex = faceList.front();
    faceList.pop_front();

    auto& tf = mesh.faces[topFaceIndex];
    tf.inFaceStack = 0;

    DEBUG_ASSERT(
        !tf.pointsOnPositiveSide || tf.pointsOnPositiveSide->size() > 0,
        logicErr, "there should be points on the positive side");
    if (!tf.pointsOnPositiveSide || tf.isDisabled()) {
      continue;
    }

    // Pick the most distant point to this triangle plane as the point to which
    // we extrude
    const vec3& activePoint = originalVertexData[tf.mostDistantPoint];
    const size_t activePointIndex = tf.mostDistantPoint;

    // Find out the faces that have our active point on their positive side
    // (these are the "visible faces"). The face on top of the stack of course
    // is one of them. At the same time, we create a list of horizon edges.
    horizonEdgesData.clear();
    possiblyVisibleFaces.clear();
    visibleFaces.clear();
    possiblyVisibleFaces.push_back({topFaceIndex, -1});
    while (possiblyVisibleFaces.size()) {
      const auto faceData = possiblyVisibleFaces.back();
      possiblyVisibleFaces.pop_back();
      auto& pvf = mesh.faces[faceData.faceIndex];
      DEBUG_ASSERT(!pvf.isDisabled(), logicErr, "pvf should not be disabled");

      if (pvf.visibilityCheckedOnIteration == iter) {
        if (pvf.isVisibleFaceOnCurrentIteration) {
          continue;
        }
      } else {
        const Plane& P = pvf.P;
        pvf.visibilityCheckedOnIteration = iter;
        const double d = la::dot(P.N, activePoint) + P.D;
        if (d > 0) {
          pvf.isVisibleFaceOnCurrentIteration = 1;
          pvf.horizonEdgesOnCurrentIteration = 0;
          visibleFaces.push_back(faceData.faceIndex);
          for (auto heIndex : mesh.getHalfEdgeIndicesOfFace(pvf)) {
            if (mesh.halfedges[heIndex].pairedHalfedge !=
                faceData.enteredFromHalfedge) {
              possiblyVisibleFaces.push_back(
                  {mesh.halfedgeToFace[mesh.halfedges[heIndex].pairedHalfedge],
                   heIndex});
            }
          }
          continue;
        }
        DEBUG_ASSERT(faceData.faceIndex != topFaceIndex, logicErr,
                     "face index invalid");
      }

      // The face is not visible. Therefore, the halfedge we came from is part
      // of the horizon edge.
      pvf.isVisibleFaceOnCurrentIteration = 0;
      horizonEdgesData.push_back(faceData.enteredFromHalfedge);
      // Store which half edge is the horizon edge. The other half edges of the
      // face will not be part of the final mesh so their data slots can by
      // recycled.
      const auto halfEdgesMesh = mesh.getHalfEdgeIndicesOfFace(
          mesh.faces[mesh.halfedgeToFace[faceData.enteredFromHalfedge]]);
      const std::int8_t ind =
          (halfEdgesMesh[0] == faceData.enteredFromHalfedge)
              ? 0
              : (halfEdgesMesh[1] == faceData.enteredFromHalfedge ? 1 : 2);
      mesh.faces[mesh.halfedgeToFace[faceData.enteredFromHalfedge]]
          .horizonEdgesOnCurrentIteration |= (1 << ind);
    }
    const size_t horizonEdgeCount = horizonEdgesData.size();

    // Order horizon edges so that they form a loop. This may fail due to
    // numerical instability in which case we give up trying to solve horizon
    // edge for this point and accept a minor degeneration in the convex hull.
    if (!reorderHorizonEdges(horizonEdgesData)) {
      failedHorizonEdges++;
      int change_flag = 0;
      for (size_t index = 0; index < tf.pointsOnPositiveSide->size(); index++) {
        if ((*tf.pointsOnPositiveSide)[index] == activePointIndex) {
          change_flag = 1;
        } else if (change_flag == 1) {
          change_flag = 2;
          (*tf.pointsOnPositiveSide)[index - 1] =
              (*tf.pointsOnPositiveSide)[index];
        }
      }
      if (change_flag == 1)
        tf.pointsOnPositiveSide->resize(tf.pointsOnPositiveSide->size() - 1);

      if (tf.pointsOnPositiveSide->size() == 0) {
        reclaimToIndexVectorPool(tf.pointsOnPositiveSide);
      }
      continue;
    }

    // Except for the horizon edges, all half edges of the visible faces can be
    // marked as disabled. Their data slots will be reused. The faces will be
    // disabled as well, but we need to remember the points that were on the
    // positive side of them - therefore we save pointers to them.
    newFaceIndices.clear();
    newHalfedgeIndices.clear();
    disabledFacePointVectors.clear();
    size_t disableCounter = 0;
    for (auto faceIndex : visibleFaces) {
      auto& disabledFace = mesh.faces[faceIndex];
      auto halfEdgesMesh = mesh.getHalfEdgeIndicesOfFace(disabledFace);
      for (size_t j = 0; j < 3; j++) {
        if ((disabledFace.horizonEdgesOnCurrentIteration & (1 << j)) == 0) {
          if (disableCounter < horizonEdgeCount * 2) {
            // Use on this iteration
            newHalfedgeIndices.push_back(halfEdgesMesh[j]);
            disableCounter++;
          } else {
            // Mark for reusal on later iteration step
            mesh.disableHalfedge(halfEdgesMesh[j]);
          }
        }
      }
      // Disable the face, but retain pointer to the points that were on the
      // positive side of it. We need to assign those points to the new faces we
      // create shortly.
      auto t = mesh.disableFace(faceIndex);
      if (t) {
        // Because we should not assign point vectors to faces unless needed...
        DEBUG_ASSERT(t->size(), logicErr, "t should not be empty");
        disabledFacePointVectors.push_back(std::move(t));
      }
    }
    if (disableCounter < horizonEdgeCount * 2) {
      const size_t newHalfEdgesNeeded = horizonEdgeCount * 2 - disableCounter;
      for (size_t i = 0; i < newHalfEdgesNeeded; i++) {
        newHalfedgeIndices.push_back(mesh.addHalfedge());
      }
    }

    // Create new faces using the edgeloop
    for (size_t i = 0; i < horizonEdgeCount; i++) {
      const size_t AB = horizonEdgesData[i];

      auto horizonEdgeVertexIndices =
          mesh.getVertexIndicesOfHalfEdge(mesh.halfedges[AB]);
      size_t A, B, C;
      A = horizonEdgeVertexIndices[0];
      B = horizonEdgeVertexIndices[1];
      C = activePointIndex;

      const size_t newFaceIndex = mesh.addFace();
      newFaceIndices.push_back(newFaceIndex);

      const size_t CA = newHalfedgeIndices[2 * i + 0];
      const size_t BC = newHalfedgeIndices[2 * i + 1];

      mesh.halfedgeNext[AB] = BC;
      mesh.halfedgeNext[BC] = CA;
      mesh.halfedgeNext[CA] = AB;

      mesh.halfedgeToFace[BC] = newFaceIndex;
      mesh.halfedgeToFace[CA] = newFaceIndex;
      mesh.halfedgeToFace[AB] = newFaceIndex;

      mesh.halfedges[CA].endVert = A;
      mesh.halfedges[BC].endVert = C;

      auto& newFace = mesh.faces[newFaceIndex];

      const vec3 planeNormal = getTriangleNormal(
          originalVertexData[A], originalVertexData[B], activePoint);
      newFace.P = Plane(planeNormal, activePoint);
      newFace.he = AB;

      mesh.halfedges[CA].pairedHalfedge =
          newHalfedgeIndices[i > 0 ? i * 2 - 1 : 2 * horizonEdgeCount - 1];
      mesh.halfedges[BC].pairedHalfedge =
          newHalfedgeIndices[((i + 1) * 2) % (horizonEdgeCount * 2)];
    }

    // Assign points that were on the positive side of the disabled faces to the
    // new faces.
    for (auto& disabledPoints : disabledFacePointVectors) {
      DEBUG_ASSERT(disabledPoints != nullptr, logicErr,
                   "disabledPoints should not be null");
      for (const auto& point : *(disabledPoints)) {
        if (point == activePointIndex) {
          continue;
        }
        for (size_t j = 0; j < horizonEdgeCount; j++) {
          if (addPointToFace(mesh.faces[newFaceIndices[j]], point)) {
            break;
          }
        }
      }
      // The points are no longer needed: we can move them to the vector pool
      // for reuse.
      reclaimToIndexVectorPool(disabledPoints);
    }

    // Increase face stack size if needed
    for (const auto newFaceIndex : newFaceIndices) {
      auto& newFace = mesh.faces[newFaceIndex];
      if (newFace.pointsOnPositiveSide) {
        DEBUG_ASSERT(newFace.pointsOnPositiveSide->size() > 0, logicErr,
                     "there should be points on the positive side");
        if (!newFace.inFaceStack) {
          faceList.push_back(newFaceIndex);
          newFace.inFaceStack = 1;
        }
      }
    }
  }

  // Cleanup
  indexVectorPool.clear();
}

/*
 * Private helper functions
 */

std::array<size_t, 6> QuickHull::getExtremeValues() {
  std::array<size_t, 6> outIndices{0, 0, 0, 0, 0, 0};
  double extremeVals[6] = {originalVertexData[0].x, originalVertexData[0].x,
                           originalVertexData[0].y, originalVertexData[0].y,
                           originalVertexData[0].z, originalVertexData[0].z};
  const size_t vCount = originalVertexData.size();
  for (size_t i = 1; i < vCount; i++) {
    const vec3& pos = originalVertexData[i];
    if (pos.x > extremeVals[0]) {
      extremeVals[0] = pos.x;
      outIndices[0] = i;
    } else if (pos.x < extremeVals[1]) {
      extremeVals[1] = pos.x;
      outIndices[1] = i;
    }
    if (pos.y > extremeVals[2]) {
      extremeVals[2] = pos.y;
      outIndices[2] = i;
    } else if (pos.y < extremeVals[3]) {
      extremeVals[3] = pos.y;
      outIndices[3] = i;
    }
    if (pos.z > extremeVals[4]) {
      extremeVals[4] = pos.z;
      outIndices[4] = i;
    } else if (pos.z < extremeVals[5]) {
      extremeVals[5] = pos.z;
      outIndices[5] = i;
    }
  }
  return outIndices;
}

bool QuickHull::reorderHorizonEdges(VecView<size_t>& horizonEdges) {
  const size_t horizonEdgeCount = horizonEdges.size();
  for (size_t i = 0; i + 1 < horizonEdgeCount; i++) {
    const size_t endVertexCheck = mesh.halfedges[horizonEdges[i]].endVert;
    bool foundNext = false;
    for (size_t j = i + 1; j < horizonEdgeCount; j++) {
      const size_t beginVertex =
          mesh.halfedges[mesh.halfedges[horizonEdges[j]].pairedHalfedge]
              .endVert;
      if (beginVertex == endVertexCheck) {
        std::swap(horizonEdges[i + 1], horizonEdges[j]);
        foundNext = true;
        break;
      }
    }
    if (!foundNext) {
      return false;
    }
  }
  DEBUG_ASSERT(
      mesh.halfedges[horizonEdges[horizonEdges.size() - 1]].endVert ==
          mesh.halfedges[mesh.halfedges[horizonEdges[0]].pairedHalfedge]
              .endVert,
      logicErr, "invalid halfedge");
  return true;
}

double QuickHull::getScale(const std::array<size_t, 6>& extremeValuesInput) {
  double s = 0;
  for (size_t i = 0; i < 6; i++) {
    const double* v =
        (const double*)(&originalVertexData[extremeValuesInput[i]]);
    v += i / 2;
    auto a = std::abs(*v);
    if (a > s) {
      s = a;
    }
  }
  return s;
}

void QuickHull::setupInitialTetrahedron() {
  const size_t vertexCount = originalVertexData.size();

  // If we have at most 4 points, just return a degenerate tetrahedron:
  if (vertexCount <= 4) {
    size_t v[4] = {0, std::min((size_t)1, vertexCount - 1),
                   std::min((size_t)2, vertexCount - 1),
                   std::min((size_t)3, vertexCount - 1)};
    const vec3 N =
        getTriangleNormal(originalVertexData[v[0]], originalVertexData[v[1]],
                          originalVertexData[v[2]]);
    const Plane trianglePlane(N, originalVertexData[v[0]]);
    if (trianglePlane.isPointOnPositiveSide(originalVertexData[v[3]])) {
      std::swap(v[0], v[1]);
    }
    return mesh.setup(v[0], v[1], v[2], v[3]);
  }

  // Find two most distant extreme points.
  double maxD = epsilonSquared;
  std::pair<size_t, size_t> selectedPoints;
  for (size_t i = 0; i < 6; i++) {
    for (size_t j = i + 1; j < 6; j++) {
      // I found a function for squaredDistance but i can't seem to include it
      // like this for some reason
      const double d = getSquaredDistance(originalVertexData[extremeValues[i]],
                                          originalVertexData[extremeValues[j]]);
      if (d > maxD) {
        maxD = d;
        selectedPoints = {extremeValues[i], extremeValues[j]};
      }
    }
  }
  if (maxD == epsilonSquared) {
    // A degenerate case: the point cloud seems to consists of a single point
    return mesh.setup(0, std::min((size_t)1, vertexCount - 1),
                      std::min((size_t)2, vertexCount - 1),
                      std::min((size_t)3, vertexCount - 1));
  }
  DEBUG_ASSERT(selectedPoints.first != selectedPoints.second, logicErr,
               "degenerate selectedPoints");

  // Find the most distant point to the line between the two chosen extreme
  // points.
  const Ray r(originalVertexData[selectedPoints.first],
              (originalVertexData[selectedPoints.second] -
               originalVertexData[selectedPoints.first]));
  maxD = epsilonSquared;
  size_t maxI = std::numeric_limits<size_t>::max();
  const size_t vCount = originalVertexData.size();
  for (size_t i = 0; i < vCount; i++) {
    const double distToRay =
        getSquaredDistanceBetweenPointAndRay(originalVertexData[i], r);
    if (distToRay > maxD) {
      maxD = distToRay;
      maxI = i;
    }
  }
  if (maxD == epsilonSquared) {
    // It appears that the point cloud belongs to a 1 dimensional subspace of
    // R^3: convex hull has no volume => return a thin triangle Pick any point
    // other than selectedPoints.first and selectedPoints.second as the third
    // point of the triangle
    auto it =
        std::find_if(originalVertexData.begin(), originalVertexData.end(),
                     [&](const vec3& ve) {
                       return ve != originalVertexData[selectedPoints.first] &&
                              ve != originalVertexData[selectedPoints.second];
                     });
    const size_t thirdPoint =
        (it == originalVertexData.end())
            ? selectedPoints.first
            : std::distance(originalVertexData.begin(), it);
    it =
        std::find_if(originalVertexData.begin(), originalVertexData.end(),
                     [&](const vec3& ve) {
                       return ve != originalVertexData[selectedPoints.first] &&
                              ve != originalVertexData[selectedPoints.second] &&
                              ve != originalVertexData[thirdPoint];
                     });
    const size_t fourthPoint =
        (it == originalVertexData.end())
            ? selectedPoints.first
            : std::distance(originalVertexData.begin(), it);
    return mesh.setup(selectedPoints.first, selectedPoints.second, thirdPoint,
                      fourthPoint);
  }

  // These three points form the base triangle for our tetrahedron.
  DEBUG_ASSERT(selectedPoints.first != maxI && selectedPoints.second != maxI,
               logicErr, "degenerate selectedPoints");
  std::array<size_t, 3> baseTriangle{selectedPoints.first,
                                     selectedPoints.second, maxI};
  const vec3 baseTriangleVertices[] = {originalVertexData[baseTriangle[0]],
                                       originalVertexData[baseTriangle[1]],
                                       originalVertexData[baseTriangle[2]]};

  // Next step is to find the 4th vertex of the tetrahedron. We naturally choose
  // the point farthest away from the triangle plane.
  maxD = m_epsilon;
  maxI = 0;
  const vec3 N =
      getTriangleNormal(baseTriangleVertices[0], baseTriangleVertices[1],
                        baseTriangleVertices[2]);
  Plane trianglePlane(N, baseTriangleVertices[0]);
  for (size_t i = 0; i < vCount; i++) {
    const double d = std::abs(
        getSignedDistanceToPlane(originalVertexData[i], trianglePlane));
    if (d > maxD) {
      maxD = d;
      maxI = i;
    }
  }
  if (maxD == m_epsilon) {
    // All the points seem to lie on a 2D subspace of R^3. How to handle this?
    // Well, let's add one extra point to the point cloud so that the convex
    // hull will have volume.
    planar = true;
    const vec3 N1 =
        getTriangleNormal(baseTriangleVertices[1], baseTriangleVertices[2],
                          baseTriangleVertices[0]);
    planarPointCloudTemp = Vec<vec3>(originalVertexData);
    const vec3 extraPoint = N1 + originalVertexData[0];
    planarPointCloudTemp.push_back(extraPoint);
    maxI = planarPointCloudTemp.size() - 1;
    originalVertexData = planarPointCloudTemp;
  }

  // Enforce CCW orientation (if user prefers clockwise orientation, swap two
  // vertices in each triangle when final mesh is created)
  const Plane triPlane(N, baseTriangleVertices[0]);
  if (triPlane.isPointOnPositiveSide(originalVertexData[maxI])) {
    std::swap(baseTriangle[0], baseTriangle[1]);
  }

  // Create a tetrahedron half edge mesh and compute planes defined by each
  // triangle
  mesh.setup(baseTriangle[0], baseTriangle[1], baseTriangle[2], maxI);
  for (auto& f : mesh.faces) {
    auto v = mesh.getVertexIndicesOfFace(f);
    const vec3 N1 =
        getTriangleNormal(originalVertexData[v[0]], originalVertexData[v[1]],
                          originalVertexData[v[2]]);
    const Plane plane(N1, originalVertexData[v[0]]);
    f.P = plane;
  }

  // Finally we assign a face for each vertex outside the tetrahedron (vertices
  // inside the tetrahedron have no role anymore)
  for (size_t i = 0; i < vCount; i++) {
    for (auto& face : mesh.faces) {
      if (addPointToFace(face, i)) {
        break;
      }
    }
  }
}

std::unique_ptr<Vec<size_t>> QuickHull::getIndexVectorFromPool() {
  auto r = indexVectorPool.get();
  r->resize(0);
  return r;
}

void QuickHull::reclaimToIndexVectorPool(std::unique_ptr<Vec<size_t>>& ptr) {
  const size_t oldSize = ptr->size();
  if ((oldSize + 1) * 128 < ptr->capacity()) {
    // Reduce memory usage! Huge vectors are needed at the beginning of
    // iteration when faces have many points on their positive side. Later on,
    // smaller vectors will suffice.
    ptr.reset(nullptr);
    return;
  }
  indexVectorPool.reclaim(ptr);
}

bool QuickHull::addPointToFace(typename MeshBuilder::Face& f,
                               size_t pointIndex) {
  const double D =
      getSignedDistanceToPlane(originalVertexData[pointIndex], f.P);
  if (D > 0 && D * D > epsilonSquared * f.P.sqrNLength) {
    if (!f.pointsOnPositiveSide) {
      f.pointsOnPositiveSide = getIndexVectorFromPool();
    }
    f.pointsOnPositiveSide->push_back(pointIndex);
    if (D > f.mostDistantPointDist) {
      f.mostDistantPointDist = D;
      f.mostDistantPoint = pointIndex;
    }
    return true;
  }
  return false;
}

// Wrapper to call the QuickHull algorithm with the given vertex data to build
// the Impl
void Manifold::Impl::Hull(VecView<vec3> vertPos) {
  size_t numVert = vertPos.size();
  if (numVert < 4) {
    status_ = Error::InvalidConstruction;
    return;
  }

  QuickHull qh(vertPos);
  std::tie(halfedge_, vertPos_) = qh.buildMesh();
  CalculateBBox();
  SetEpsilon();
  CalculateNormals();
  InitializeOriginal();
  Finish();
  CreateFaces();
}

}  // namespace manifold
