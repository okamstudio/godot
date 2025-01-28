// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Core/StreamUtils.h>

JPH_NAMESPACE_BEGIN

/// This class defines the setup of all particles and their constraints.
/// It is used during the simulation and can be shared between multiple soft bodies.
class JPH_EXPORT SoftBodySharedSettings : public RefTarget<SoftBodySharedSettings>
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, SoftBodySharedSettings)

public:
	/// Which type of bend constraint should be created
	enum class EBendType
	{
		None,														///< No bend constraints will be created
		Distance,													///< A simple distance constraint
		Dihedral,													///< A dihedral bend constraint (most expensive, but also supports triangles that are initially not in the same plane)
	};

	/// The type of long range attachment constraint to create
	enum class ELRAType
	{
		None,														///< Don't create a LRA constraint
		EuclideanDistance,											///< Create a LRA constraint based on Euclidean distance between the closest kinematic vertex and this vertex
		GeodesicDistance,											///< Create a LRA constraint based on the geodesic distance between the closest kinematic vertex and this vertex (follows the edge constraints)
	};

	/// Per vertex attributes used during the CreateConstraints function.
	/// For an edge or shear constraint, the compliance is averaged between the two attached vertices.
	/// For a bend constraint, the compliance is averaged between the two vertices on the shared edge.
	struct JPH_EXPORT VertexAttributes
	{
		/// Constructor
						VertexAttributes() = default;
						VertexAttributes(float inCompliance, float inShearCompliance, float inBendCompliance, ELRAType inLRAType = ELRAType::None, float inLRAMaxDistanceMultiplier = 1.0f) : mCompliance(inCompliance), mShearCompliance(inShearCompliance), mBendCompliance(inBendCompliance), mLRAType(inLRAType), mLRAMaxDistanceMultiplier(inLRAMaxDistanceMultiplier) { }

		float			mCompliance = 0.0f;							///< The compliance of the normal edges. Set to FLT_MAX to disable regular edges for any edge involving this vertex.
		float			mShearCompliance = 0.0f;					///< The compliance of the shear edges. Set to FLT_MAX to disable shear edges for any edge involving this vertex.
		float			mBendCompliance = FLT_MAX;					///< The compliance of the bend edges. Set to FLT_MAX to disable bend edges for any bend constraint involving this vertex.
		ELRAType		mLRAType = ELRAType::None;					///< The type of long range attachment constraint to create.
		float			mLRAMaxDistanceMultiplier = 1.0f;			///< Multiplier for the max distance of the LRA constraint, e.g. 1.01 means the max distance is 1% longer than the calculated distance in the rest pose.
	};

	/// Automatically create constraints based on the faces of the soft body
	/// @param inVertexAttributes A list of attributes for each vertex (1-on-1 with mVertices, note that if the list is smaller than mVertices the last element will be repeated). This defines the properties of the constraints that are created.
	/// @param inVertexAttributesLength The length of inVertexAttributes
	/// @param inBendType The type of bend constraint to create
	/// @param inAngleTolerance Shear edges are created when two connected triangles form a quad (are roughly in the same plane and form a square with roughly 90 degree angles). This defines the tolerance (in radians).
	void				CreateConstraints(const VertexAttributes *inVertexAttributes, uint inVertexAttributesLength, EBendType inBendType = EBendType::Distance, float inAngleTolerance = DegreesToRadians(8.0f));

	/// Calculate the initial lengths of all springs of the edges of this soft body (if you use CreateConstraint, this is already done)
	void				CalculateEdgeLengths();

	/// Calculate the max lengths for the long range attachment constraints based on Euclidean distance (if you use CreateConstraints, this is already done)
	/// @param inMaxDistanceMultiplier Multiplier for the max distance of the LRA constraint, e.g. 1.01 means the max distance is 1% longer than the calculated distance in the rest pose.
	void				CalculateLRALengths(float inMaxDistanceMultiplier = 1.0f);

	/// Calculate the constants for the bend constraints (if you use CreateConstraints, this is already done)
	void				CalculateBendConstraintConstants();

	/// Calculates the initial volume of all tetrahedra of this soft body
	void				CalculateVolumeConstraintVolumes();

	/// Calculate information needed to be able to calculate the skinned constraint normals at run-time
	void				CalculateSkinnedConstraintNormals();

	/// Information about the optimization of the soft body, the indices of certain elements may have changed.
	class OptimizationResults
	{
	public:
		Array<uint>		mEdgeRemap;									///< Maps old edge index to new edge index
		Array<uint>		mLRARemap;									///< Maps old LRA index to new LRA index
		Array<uint>		mDihedralBendRemap;							///< Maps old dihedral bend index to new dihedral bend index
		Array<uint>		mVolumeRemap;								///< Maps old volume constraint index to new volume constraint index
		Array<uint>		mSkinnedRemap;								///< Maps old skinned constraint index to new skinned constraint index
	};

	/// Optimize the soft body settings for simulation. This will reorder constraints so they can be executed in parallel.
	void				Optimize(OptimizationResults &outResults);

	/// Optimize the soft body settings without results
	void				Optimize()									{ OptimizationResults results; Optimize(results); }

	/// Clone this object
	Ref<SoftBodySharedSettings> Clone() const;

	/// Saves the state of this object in binary form to inStream. Doesn't store the material list.
	void				SaveBinaryState(StreamOut &inStream) const;

	/// Restore the state of this object from inStream. Doesn't restore the material list.
	void				RestoreBinaryState(StreamIn &inStream);

	using SharedSettingsToIDMap = StreamUtils::ObjectToIDMap<SoftBodySharedSettings>;
	using IDToSharedSettingsMap = StreamUtils::IDToObjectMap<SoftBodySharedSettings>;
	using MaterialToIDMap = StreamUtils::ObjectToIDMap<PhysicsMaterial>;
	using IDToMaterialMap = StreamUtils::IDToObjectMap<PhysicsMaterial>;

	/// Save this shared settings and its materials. Pass in an empty map ioSettingsMap / ioMaterialMap or reuse the same map while saving multiple settings objects to the same stream in order to avoid writing duplicates.
	void				SaveWithMaterials(StreamOut &inStream, SharedSettingsToIDMap &ioSettingsMap, MaterialToIDMap &ioMaterialMap) const;

	using SettingsResult = Result<Ref<SoftBodySharedSettings>>;

	/// Restore a shape and materials. Pass in an empty map in ioSettingsMap / ioMaterialMap or reuse the same map while reading multiple settings objects from the same stream in order to restore duplicates.
	static SettingsResult sRestoreWithMaterials(StreamIn &inStream, IDToSharedSettingsMap &ioSettingsMap, IDToMaterialMap &ioMaterialMap);

	/// A vertex is a particle, the data in this structure is only used during creation of the soft body and not during simulation
	struct JPH_EXPORT Vertex
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Vertex)

		/// Constructor
						Vertex() = default;
						Vertex(const Float3 &inPosition, const Float3 &inVelocity = Float3(0, 0, 0), float inInvMass = 1.0f) : mPosition(inPosition), mVelocity(inVelocity), mInvMass(inInvMass) { }

		Float3			mPosition { 0, 0, 0 };						///< Initial position of the vertex
		Float3			mVelocity { 0, 0, 0 };						///< Initial velocity of the vertex
		float			mInvMass = 1.0f;							///< Initial inverse of the mass of the vertex
	};

	/// A face defines the surface of the body
	struct JPH_EXPORT Face
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Face)

		/// Constructor
						Face() = default;
						Face(uint32 inVertex1, uint32 inVertex2, uint32 inVertex3, uint32 inMaterialIndex = 0) : mVertex { inVertex1, inVertex2, inVertex3 }, mMaterialIndex(inMaterialIndex) { }

		/// Check if this is a degenerate face (a face which points to the same vertex twice)
		bool			IsDegenerate() const						{ return mVertex[0] == mVertex[1] || mVertex[0] == mVertex[2] || mVertex[1] == mVertex[2]; }

		uint32			mVertex[3];									///< Indices of the vertices that form the face
		uint32			mMaterialIndex = 0;							///< Index of the material of the face in SoftBodySharedSettings::mMaterials
	};

	/// An edge keeps two vertices at a constant distance using a spring: |x1 - x2| = rest length
	struct JPH_EXPORT Edge
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Edge)

		/// Constructor
						Edge() = default;
						Edge(uint32 inVertex1, uint32 inVertex2, float inCompliance = 0.0f) : mVertex { inVertex1, inVertex2 }, mCompliance(inCompliance) { }

		/// Return the lowest vertex index of this constraint
		uint32			GetMinVertexIndex() const					{ return min(mVertex[0], mVertex[1]); }

		uint32			mVertex[2];									///< Indices of the vertices that form the edge
		float			mRestLength = 1.0f;							///< Rest length of the spring
		float			mCompliance = 0.0f;							///< Inverse of the stiffness of the spring
	};

	/**
	 * A dihedral bend constraint keeps the angle between two triangles constant along their shared edge.
	 *
	 *        x2
	 *       /  \
	 *      / t0 \
	 *     x0----x1
	 *      \ t1 /
	 *       \  /
	 *        x3
	 *
	 * x0..x3 are the vertices, t0 and t1 are the triangles that share the edge x0..x1
	 *
	 * Based on:
	 * - "Position Based Dynamics" - Matthias Muller et al.
	 * - "Strain Based Dynamics" - Matthias Muller et al.
	 * - "Simulation of Clothing with Folds and Wrinkles" - R. Bridson et al.
	 */
	struct JPH_EXPORT DihedralBend
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, DihedralBend)

		/// Constructor
						DihedralBend() = default;
						DihedralBend(uint32 inVertex1, uint32 inVertex2, uint32 inVertex3, uint32 inVertex4, float inCompliance = 0.0f) : mVertex { inVertex1, inVertex2, inVertex3, inVertex4 }, mCompliance(inCompliance) { }

		/// Return the lowest vertex index of this constraint
		uint32			GetMinVertexIndex() const					{ return min(min(mVertex[0], mVertex[1]), min(mVertex[2], mVertex[3])); }

		uint32			mVertex[4];									///< Indices of the vertices of the 2 triangles that share an edge (the first 2 vertices are the shared edge)
		float			mCompliance = 0.0f;							///< Inverse of the stiffness of the constraint
		float			mInitialAngle = 0.0f;						///< Initial angle between the normals of the triangles (pi - dihedral angle).
	};

	/// Volume constraint, keeps the volume of a tetrahedron constant
	struct JPH_EXPORT Volume
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Volume)

		/// Constructor
						Volume() = default;
						Volume(uint32 inVertex1, uint32 inVertex2, uint32 inVertex3, uint32 inVertex4, float inCompliance = 0.0f) : mVertex { inVertex1, inVertex2, inVertex3, inVertex4 }, mCompliance(inCompliance) { }

		/// Return the lowest vertex index of this constraint
		uint32			GetMinVertexIndex() const					{ return min(min(mVertex[0], mVertex[1]), min(mVertex[2], mVertex[3])); }

		uint32			mVertex[4];									///< Indices of the vertices that form the tetrahedron
		float			mSixRestVolume = 1.0f;						///< 6 times the rest volume of the tetrahedron (calculated by CalculateVolumeConstraintVolumes())
		float			mCompliance = 0.0f;							///< Inverse of the stiffness of the constraint
	};

	/// An inverse bind matrix take a skinned vertex from its bind pose into joint local space
	class JPH_EXPORT InvBind
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, InvBind)

	public:
		/// Constructor
						InvBind() = default;
						InvBind(uint32 inJointIndex, Mat44Arg inInvBind) : mJointIndex(inJointIndex), mInvBind(inInvBind) { }

		uint32			mJointIndex = 0;							///< Joint index to which this is attached
		Mat44			mInvBind = Mat44::sIdentity();				///< The inverse bind matrix, this takes a vertex in its bind pose (Vertex::mPosition) to joint local space
	};

	/// A joint and its skin weight
	class JPH_EXPORT SkinWeight
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, SkinWeight)

	public:
		/// Constructor
						SkinWeight() = default;
						SkinWeight(uint32 inInvBindIndex, float inWeight) : mInvBindIndex(inInvBindIndex), mWeight(inWeight) { }

		uint32			mInvBindIndex = 0;							///< Index in mInvBindMatrices
		float			mWeight = 0.0f;								///< Weight with which it is skinned
	};

	/// A constraint that skins a vertex to joints and limits the distance that the simulated vertex can travel from this vertex
	class JPH_EXPORT Skinned
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Skinned)

	public:
		/// Constructor
						Skinned() = default;
						Skinned(uint32 inVertex, float inMaxDistance, float inBackStopDistance, float inBackStopRadius) : mVertex(inVertex), mMaxDistance(inMaxDistance), mBackStopDistance(inBackStopDistance), mBackStopRadius(inBackStopRadius) { }

		/// Normalize the weights so that they add up to 1
		void			NormalizeWeights()
		{
			// Get the total weight
			float total = 0.0f;
			for (const SkinWeight &w : mWeights)
				total += w.mWeight;

			// Normalize
			if (total > 0.0f)
				for (SkinWeight &w : mWeights)
					w.mWeight /= total;
		}

		/// Maximum number of skin weights
		static constexpr uint cMaxSkinWeights = 4;

		uint32			mVertex = 0;								///< Index in mVertices which indicates which vertex is being skinned
		SkinWeight		mWeights[cMaxSkinWeights];					///< Skin weights, the bind pose of the vertex is assumed to be stored in Vertex::mPosition. The first weight that is zero indicates the end of the list. Weights should add up to 1.
		float			mMaxDistance = FLT_MAX;						///< Maximum distance that this vertex can reach from the skinned vertex, disabled when FLT_MAX. 0 when you want to hard skin the vertex to the skinned vertex.
		float			mBackStopDistance = FLT_MAX;				///< Disabled if mBackStopDistance >= mMaxDistance. The faces surrounding mVertex determine an average normal. mBackStopDistance behind the vertex in the opposite direction of this normal, the back stop sphere starts. The simulated vertex will be pushed out of this sphere and it can be used to approximate the volume of the skinned mesh behind the skinned vertex.
		float			mBackStopRadius = 40.0f;					///< Radius of the backstop sphere. By default this is a fairly large radius so the sphere approximates a plane.
		uint32			mNormalInfo = 0;							///< Information needed to calculate the normal of this vertex, lowest 24 bit is start index in mSkinnedConstraintNormals, highest 8 bit is number of faces (generated by CalculateSkinnedConstraintNormals())
	};

	/// A long range attachment constraint, this is a constraint that sets a max distance between a kinematic vertex and a dynamic vertex
	/// See: "Long Range Attachments - A Method to Simulate Inextensible Clothing in Computer Games", Tae-Yong Kim, Nuttapong Chentanez and Matthias Mueller-Fischer
	class JPH_EXPORT LRA
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, LRA)

	public:
		/// Constructor
						LRA() = default;
						LRA(uint32 inVertex1, uint32 inVertex2, float inMaxDistance) : mVertex { inVertex1, inVertex2 }, mMaxDistance(inMaxDistance) { }

		/// Return the lowest vertex index of this constraint
		uint32			GetMinVertexIndex() const					{ return min(mVertex[0], mVertex[1]); }

		uint32			mVertex[2];									///< The vertices that are connected. The first vertex should be kinematic, the 2nd dynamic.
		float			mMaxDistance = 0.0f;						///< The maximum distance between the vertices
	};

	/// Add a face to this soft body
	void				AddFace(const Face &inFace)					{ JPH_ASSERT(!inFace.IsDegenerate()); mFaces.push_back(inFace); }

	Array<Vertex>		mVertices;									///< The list of vertices or particles of the body
	Array<Face>			mFaces;										///< The list of faces of the body
	Array<Edge>			mEdgeConstraints;							///< The list of edges or springs of the body
	Array<DihedralBend>	mDihedralBendConstraints;					///< The list of dihedral bend constraints of the body
	Array<Volume>		mVolumeConstraints;							///< The list of volume constraints of the body that keep the volume of tetrahedra in the soft body constant
	Array<Skinned>		mSkinnedConstraints;						///< The list of vertices that are constrained to a skinned vertex
	Array<InvBind>		mInvBindMatrices;							///< The list of inverse bind matrices for skinning vertices
	Array<LRA>			mLRAConstraints;							///< The list of long range attachment constraints
	PhysicsMaterialList mMaterials { PhysicsMaterial::sDefault };	///< The materials of the faces of the body, referenced by Face::mMaterialIndex
	float				mVertexRadius = 0.0f;						///< How big the particles are, can be used to push the vertices a little bit away from the surface of other bodies to prevent z-fighting

private:
	friend class SoftBodyMotionProperties;

	/// Calculate the closest kinematic vertex array
	void				CalculateClosestKinematic();

	/// Tracks the closest kinematic vertex
	struct ClosestKinematic
	{
		uint32			mVertex = 0xffffffff;						///< Vertex index of closest kinematic vertex
		float			mDistance = FLT_MAX;						///< Distance to the closest kinematic vertex
	};

	/// Tracks the end indices of the various constraint groups
	struct UpdateGroup
	{
		uint			mEdgeEndIndex;								///< The end index of the edge constraints in this group
		uint			mLRAEndIndex;								///< The end index of the LRA constraints in this group
		uint			mDihedralBendEndIndex;						///< The end index of the dihedral bend constraints in this group
		uint			mVolumeEndIndex;							///< The end index of the volume constraints in this group
		uint			mSkinnedEndIndex;							///< The end index of the skinned constraints in this group
	};

	Array<ClosestKinematic> mClosestKinematic;						///< The closest kinematic vertex to each vertex in mVertices
	Array<UpdateGroup>	mUpdateGroups;								///< The end indices for each group of constraints that can be updated in parallel
	Array<uint32>		mSkinnedConstraintNormals;					///< A list of indices in the mFaces array used by mSkinnedConstraints, calculated by CalculateSkinnedConstraintNormals()
};

JPH_NAMESPACE_END
