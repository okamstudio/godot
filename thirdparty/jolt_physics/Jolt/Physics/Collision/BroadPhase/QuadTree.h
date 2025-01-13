// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/Atomics.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhase.h>

//#define JPH_DUMP_BROADPHASE_TREE

JPH_NAMESPACE_BEGIN

/// Internal tree structure in broadphase, is essentially a quad AABB tree.
/// Tree is lockless (except for UpdatePrepare/Finalize() function), modifying objects in the tree will widen the aabbs of parent nodes to make the node fit.
/// During the UpdatePrepare/Finalize() call the tree is rebuilt to achieve a tight fit again.
class JPH_EXPORT QuadTree : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

private:
	// Forward declare
	class AtomicNodeID;

	/// Class that points to either a body or a node in the tree
	class NodeID
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Default constructor does not initialize
		inline					NodeID() = default;

		/// Construct a node ID
		static inline NodeID	sInvalid()							{ return NodeID(cInvalidNodeIndex); }
		static inline NodeID	sFromBodyID(BodyID inID)			{ NodeID node_id(inID.GetIndexAndSequenceNumber()); JPH_ASSERT(node_id.IsBody()); return node_id; }
		static inline NodeID	sFromNodeIndex(uint32 inIdx)		{ NodeID node_id(inIdx | cIsNode); JPH_ASSERT(node_id.IsNode()); return node_id; }

		/// Check what type of ID it is
		inline bool				IsValid() const						{ return mID != cInvalidNodeIndex; }
		inline bool				IsBody() const						{ return (mID & cIsNode) == 0; }
		inline bool				IsNode() const						{ return (mID & cIsNode) != 0; }

		/// Get body or node index
		inline BodyID			GetBodyID() const					{ JPH_ASSERT(IsBody()); return BodyID(mID); }
		inline uint32			GetNodeIndex() const				{ JPH_ASSERT(IsNode()); return mID & ~cIsNode; }

		/// Comparison
		inline bool				operator == (const BodyID &inRHS) const { return mID == inRHS.GetIndexAndSequenceNumber(); }
		inline bool				operator == (const NodeID &inRHS) const	{ return mID == inRHS.mID; }

	private:
		friend class AtomicNodeID;

		inline explicit			NodeID(uint32 inID)					: mID(inID) { }

		static const uint32		cIsNode = BodyID::cBroadPhaseBit;	///< If this bit is set it means that the ID refers to a node, otherwise it refers to a body

		uint32					mID;
	};

	static_assert(sizeof(NodeID) == sizeof(BodyID), "Body id's should have the same size as NodeIDs");

	/// A NodeID that uses atomics to store the value
	class AtomicNodeID
	{
	public:
		/// Constructor
								AtomicNodeID() = default;
		explicit				AtomicNodeID(const NodeID &inRHS)			: mID(inRHS.mID) { }

		/// Assignment
		inline void				operator = (const NodeID &inRHS)			{ mID = inRHS.mID; }

		/// Getting the value
		inline					operator NodeID () const					{ return NodeID(mID); }

		/// Check if the ID is valid
		inline bool				IsValid() const								{ return mID != cInvalidNodeIndex; }

		/// Comparison
		inline bool				operator == (const BodyID &inRHS) const		{ return mID == inRHS.GetIndexAndSequenceNumber(); }
		inline bool				operator == (const NodeID &inRHS) const		{ return mID == inRHS.mID; }

		/// Atomically compare and swap value. Expects inOld value, replaces with inNew value or returns false
		inline bool				CompareExchange(NodeID inOld, NodeID inNew)	{ return mID.compare_exchange_strong(inOld.mID, inNew.mID); }

	private:
		atomic<uint32>			mID;
	};

	/// Class that represents a node in the tree
	class Node
	{
	public:
		/// Construct node
		explicit				Node(bool inIsChanged);

		/// Get bounding box encapsulating all children
		void					GetNodeBounds(AABox &outBounds) const;

		/// Get bounding box in a consistent way with the functions below (check outBounds.IsValid() before using the box)
		void					GetChildBounds(int inChildIndex, AABox &outBounds) const;

		/// Set the bounds in such a way that other threads will either see a fully correct bounding box or a bounding box with no volume
		void					SetChildBounds(int inChildIndex, const AABox &inBounds);

		/// Invalidate bounding box in such a way that other threads will not temporarily see a very large bounding box
		void					InvalidateChildBounds(int inChildIndex);

		/// Encapsulate inBounds in node bounds, returns true if there were changes
		bool					EncapsulateChildBounds(int inChildIndex, const AABox &inBounds);

		/// Bounding box for child nodes or bodies (all initially set to invalid so no collision test will ever traverse to the leaf)
		atomic<float>			mBoundsMinX[4];
		atomic<float>			mBoundsMinY[4];
		atomic<float>			mBoundsMinZ[4];
		atomic<float>			mBoundsMaxX[4];
		atomic<float>			mBoundsMaxY[4];
		atomic<float>			mBoundsMaxZ[4];

		/// Index of child node or body ID.
		AtomicNodeID			mChildNodeID[4];

		/// Index of the parent node.
		/// Note: This value is unreliable during the UpdatePrepare/Finalize() function as a node may be relinked to the newly built tree.
		atomic<uint32>			mParentNodeIndex = cInvalidNodeIndex;

		/// If this part of the tree has changed, if not, we will treat this sub tree as a single body during the UpdatePrepare/Finalize().
		/// If any changes are made to an object inside this sub tree then the direct path from the body to the top of the tree will become changed.
		atomic<uint32>			mIsChanged;

		// Padding to align to 124 bytes
		uint32					mPadding = 0;
	};

	// Maximum size of the stack during tree walk
	static constexpr int		cStackSize = 128;

	static_assert(sizeof(atomic<float>) == 4, "Assuming that an atomic doesn't add any additional storage");
	static_assert(sizeof(atomic<uint32>) == 4, "Assuming that an atomic doesn't add any additional storage");
	static_assert(std::is_trivially_destructible<Node>(), "Assuming that we don't have a destructor");

public:
	/// Class that allocates tree nodes, can be shared between multiple trees
	using Allocator = FixedSizeFreeList<Node>;

	static_assert(Allocator::ObjectStorageSize == 128, "Node should be 128 bytes");

	/// Data to track location of a Body in the tree
	struct Tracking
	{
		/// Constructor to satisfy the vector class
								Tracking() = default;
								Tracking(const Tracking &inRHS) : mBroadPhaseLayer(inRHS.mBroadPhaseLayer.load()), mObjectLayer(inRHS.mObjectLayer.load()), mBodyLocation(inRHS.mBodyLocation.load()) { }

		/// Invalid body location identifier
		static const uint32		cInvalidBodyLocation = 0xffffffff;

		atomic<BroadPhaseLayer::Type> mBroadPhaseLayer = (BroadPhaseLayer::Type)cBroadPhaseLayerInvalid;
		atomic<ObjectLayer>		mObjectLayer = cObjectLayerInvalid;
		atomic<uint32>			mBodyLocation { cInvalidBodyLocation };
	};

	using TrackingVector = Array<Tracking>;

	/// Destructor
								~QuadTree();

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	/// Name of the tree for debugging purposes
	void						SetName(const char *inName)			{ mName = inName; }
	inline const char *			GetName() const						{ return mName; }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	/// Check if there is anything in the tree
	inline bool					HasBodies() const					{ return mNumBodies != 0; }

	/// Check if the tree needs an UpdatePrepare/Finalize()
	inline bool					IsDirty() const						{ return mIsDirty; }

	/// Check if this tree can get an UpdatePrepare/Finalize() or if it needs a DiscardOldTree() first
	inline bool					CanBeUpdated() const				{ return mFreeNodeBatch.mNumObjects == 0; }

	/// Initialization
	void						Init(Allocator &inAllocator);

	struct UpdateState
	{
		NodeID					mRootNodeID;						///< This will be the new root node id
	};

	/// Will throw away the previous frame's nodes so that we can start building a new tree in the background
	void						DiscardOldTree();

	/// Get the bounding box for this tree
	AABox						GetBounds() const;

	/// Update the broadphase, needs to be called regularly to achieve a tight fit of the tree when bodies have been modified.
	/// UpdatePrepare() will build the tree, UpdateFinalize() will lock the root of the tree shortly and swap the trees and afterwards clean up temporary data structures.
	void						UpdatePrepare(const BodyVector &inBodies, TrackingVector &ioTracking, UpdateState &outUpdateState, bool inFullRebuild);
	void						UpdateFinalize(const BodyVector &inBodies, const TrackingVector &inTracking, const UpdateState &inUpdateState);

	/// Temporary data structure to pass information between AddBodiesPrepare and AddBodiesFinalize/Abort
	struct AddState
	{
		NodeID					mLeafID = NodeID::sInvalid();
		AABox					mLeafBounds;
	};

	/// Prepare adding inNumber bodies at ioBodyIDs to the quad tree, returns the state in outState that should be used in AddBodiesFinalize.
	/// This can be done on a background thread without influencing the broadphase.
	/// ioBodyIDs may be shuffled around by this function.
	void						AddBodiesPrepare(const BodyVector &inBodies, TrackingVector &ioTracking, BodyID *ioBodyIDs, int inNumber, AddState &outState);

	/// Finalize adding bodies to the quadtree, supply the same number of bodies as in AddBodiesPrepare.
	void						AddBodiesFinalize(TrackingVector &ioTracking, int inNumberBodies, const AddState &inState);

	/// Abort adding bodies to the quadtree, supply the same bodies and state as in AddBodiesPrepare.
	/// This can be done on a background thread without influencing the broadphase.
	void						AddBodiesAbort(TrackingVector &ioTracking, const AddState &inState);

	/// Remove inNumber bodies in ioBodyIDs from the quadtree.
	void						RemoveBodies(const BodyVector &inBodies, TrackingVector &ioTracking, const BodyID *ioBodyIDs, int inNumber);

	/// Call whenever the aabb of a body changes.
	void						NotifyBodiesAABBChanged(const BodyVector &inBodies, const TrackingVector &inTracking, const BodyID *ioBodyIDs, int inNumber);

	/// Cast a ray and get the intersecting bodies in ioCollector.
	void						CastRay(const RayCast &inRay, RayCastBodyCollector &ioCollector, const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking) const;

	/// Get bodies intersecting with inBox in ioCollector
	void						CollideAABox(const AABox &inBox, CollideShapeBodyCollector &ioCollector, const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking) const;

	/// Get bodies intersecting with a sphere in ioCollector
	void						CollideSphere(Vec3Arg inCenter, float inRadius, CollideShapeBodyCollector &ioCollector, const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking) const;

	/// Get bodies intersecting with a point and any hits to ioCollector
	void						CollidePoint(Vec3Arg inPoint, CollideShapeBodyCollector &ioCollector, const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking) const;

	/// Get bodies intersecting with an oriented box and any hits to ioCollector
	void						CollideOrientedBox(const OrientedBox &inBox, CollideShapeBodyCollector &ioCollector, const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking) const;

	/// Cast a box and get intersecting bodies in ioCollector
	void						CastAABox(const AABoxCast &inBox, CastShapeBodyCollector &ioCollector, const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking) const;

	/// Find all colliding pairs between dynamic bodies, calls ioPairCollector for every pair found
	void						FindCollidingPairs(const BodyVector &inBodies, const BodyID *inActiveBodies, int inNumActiveBodies, float inSpeculativeContactDistance, BodyPairCollector &ioPairCollector, const ObjectLayerPairFilter &inObjectLayerPairFilter) const;

#ifdef JPH_TRACK_BROADPHASE_STATS
	/// Sum up all the ticks spent in the various layers
	uint64						GetTicks100Pct() const;

	/// Trace the stats of this tree to the TTY
	void						ReportStats(uint64 inTicks100Pct) const;
#endif // JPH_TRACK_BROADPHASE_STATS

private:
	/// Constants
	static const uint32			cInvalidNodeIndex = 0xffffffff;		///< Value used to indicate node index is invalid
	static const float			cLargeFloat;						///< A large floating point number that is small enough to not cause any overflows
	static const AABox			cInvalidBounds;						///< Invalid bounding box using cLargeFloat

	/// We alternate between two trees in order to let collision queries complete in parallel to adding/removing objects to the tree
	struct RootNode
	{
		/// Get the ID of the root node
		inline NodeID			GetNodeID() const					{ return NodeID::sFromNodeIndex(mIndex); }

		/// Index of the root node of the tree (this is always a node, never a body id)
		atomic<uint32>			mIndex { cInvalidNodeIndex };
	};

	/// Caches location of body inBodyID in the tracker, body can be found in mNodes[inNodeIdx].mChildNodeID[inChildIdx]
	void						GetBodyLocation(const TrackingVector &inTracking, BodyID inBodyID, uint32 &outNodeIdx, uint32 &outChildIdx) const;
	void						SetBodyLocation(TrackingVector &ioTracking, BodyID inBodyID, uint32 inNodeIdx, uint32 inChildIdx) const;
	static void					sInvalidateBodyLocation(TrackingVector &ioTracking, BodyID inBodyID);

	/// Get the current root of the tree
	JPH_INLINE const RootNode &	GetCurrentRoot() const				{ return mRootNode[mRootNodeIndex]; }
	JPH_INLINE RootNode &		GetCurrentRoot()					{ return mRootNode[mRootNodeIndex]; }

	/// Depending on if inNodeID is a body or tree node return the bounding box
	inline AABox				GetNodeOrBodyBounds(const BodyVector &inBodies, NodeID inNodeID) const;

	/// Mark node and all of its parents as changed
	inline void					MarkNodeAndParentsChanged(uint32 inNodeIndex);

	/// Widen parent bounds of node inNodeIndex to encapsulate inNewBounds, also mark node and all of its parents as changed
	inline void					WidenAndMarkNodeAndParentsChanged(uint32 inNodeIndex, const AABox &inNewBounds);

	/// Allocate a new node
	inline uint32				AllocateNode(bool inIsChanged);

	/// Try to insert a new leaf to the tree at inNodeIndex
	inline bool					TryInsertLeaf(TrackingVector &ioTracking, int inNodeIndex, NodeID inLeafID, const AABox &inLeafBounds, int inLeafNumBodies);

	/// Try to replace the existing root with a new root that contains both the existing root and the new leaf
	inline bool					TryCreateNewRoot(TrackingVector &ioTracking, atomic<uint32> &ioRootNodeIndex, NodeID inLeafID, const AABox &inLeafBounds, int inLeafNumBodies);

	/// Build a tree for ioBodyIDs, returns the NodeID of the root (which will be the ID of a single body if inNumber = 1). All tree levels up to inMaxDepthMarkChanged will be marked as 'changed'.
	NodeID						BuildTree(const BodyVector &inBodies, TrackingVector &ioTracking, NodeID *ioNodeIDs, int inNumber, uint inMaxDepthMarkChanged, AABox &outBounds);

	/// Sorts ioNodeIDs spatially into 2 groups. Second groups starts at ioNodeIDs + outMidPoint.
	/// After the function returns ioNodeIDs and ioNodeCenters will be shuffled
	static void					sPartition(NodeID *ioNodeIDs, Vec3 *ioNodeCenters, int inNumber, int &outMidPoint);

	/// Sorts ioNodeIDs from inBegin to (but excluding) inEnd spatially into 4 groups.
	/// outSplit needs to be 5 ints long, when the function returns each group runs from outSplit[i] to (but excluding) outSplit[i + 1]
	/// After the function returns ioNodeIDs and ioNodeCenters will be shuffled
	static void					sPartition4(NodeID *ioNodeIDs, Vec3 *ioNodeCenters, int inBegin, int inEnd, int *outSplit);

#ifdef JPH_DEBUG
	/// Validate that the tree is consistent.
	/// Note: This function only works if the tree is not modified while we're traversing it.
	void						ValidateTree(const BodyVector &inBodies, const TrackingVector &inTracking, uint32 inNodeIndex, uint32 inNumExpectedBodies) const;
#endif

#ifdef JPH_DUMP_BROADPHASE_TREE
	/// Dump the tree in DOT format (see: https://graphviz.org/)
	void						DumpTree(const NodeID &inRoot, const char *inFileNamePrefix) const;
#endif

	/// Allocator that controls adding / freeing nodes
	Allocator *					mAllocator = nullptr;

	/// This is a list of nodes that must be deleted after the trees are swapped and the old tree is no longer in use
	Allocator::Batch			mFreeNodeBatch;

	/// Number of bodies currently in the tree
	/// This is aligned to be in a different cache line from the `Allocator` pointer to prevent cross-thread syncs
	/// when reading nodes.
	alignas(JPH_CACHE_LINE_SIZE) atomic<uint32> mNumBodies { 0 };

	/// We alternate between two tree root nodes. When updating, we activate the new tree and we keep the old tree alive.
	/// for queries that are in progress until the next time DiscardOldTree() is called.
	RootNode					mRootNode[2];
	atomic<uint32>				mRootNodeIndex { 0 };

	/// Flag to keep track of changes to the broadphase, if false, we don't need to UpdatePrepare/Finalize()
	atomic<bool>				mIsDirty = false;

#ifdef JPH_TRACK_BROADPHASE_STATS
	/// Mutex protecting the various LayerToStats members
	mutable Mutex				mStatsMutex;

	struct Stat
	{
		uint64					mNumQueries = 0;
		uint64					mNodesVisited = 0;
		uint64					mBodiesVisited = 0;
		uint64					mHitsReported = 0;
		uint64					mTotalTicks = 0;
		uint64					mCollectorTicks = 0;
	};

	using LayerToStats = UnorderedMap<String, Stat>;

	/// Sum up all the ticks in a layer
	uint64						GetTicks100Pct(const LayerToStats &inLayer) const;

	/// Trace the stats of a single query type to the TTY
	void						ReportStats(const char *inName, const LayerToStats &inLayer, uint64 inTicks100Pct) const;

	mutable LayerToStats		mCastRayStats;
	mutable LayerToStats		mCollideAABoxStats;
	mutable LayerToStats		mCollideSphereStats;
	mutable LayerToStats		mCollidePointStats;
	mutable LayerToStats		mCollideOrientedBoxStats;
	mutable LayerToStats		mCastAABoxStats;
#endif // JPH_TRACK_BROADPHASE_STATS

	/// Debug function to get the depth of the tree from node inNodeID
	uint						GetMaxTreeDepth(const NodeID &inNodeID) const;

	/// Walk the node tree calling the Visitor::VisitNodes for each node encountered and Visitor::VisitBody for each body encountered
	template <class Visitor>
	JPH_INLINE void				WalkTree(const ObjectLayerFilter &inObjectLayerFilter, const TrackingVector &inTracking, Visitor &ioVisitor JPH_IF_TRACK_BROADPHASE_STATS(, LayerToStats &ioStats)) const;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	/// Name of this tree for debugging purposes
	const char *				mName = "Layer";
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
};

JPH_NAMESPACE_END
