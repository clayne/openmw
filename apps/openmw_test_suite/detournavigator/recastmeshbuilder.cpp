#include "operators.hpp"

#include <components/detournavigator/recastmeshbuilder.hpp>
#include <components/detournavigator/recastmesh.hpp>
#include <components/detournavigator/exceptions.hpp>
#include <components/esm/loadland.hpp>
#include <components/misc/convert.hpp>
#include <components/debug/debuglog.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>

#include <DetourCommon.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>

namespace DetourNavigator
{
    static inline bool operator ==(const Water& lhs, const Water& rhs)
    {
        const auto tie = [] (const Water& v) { return std::tie(v.mCellSize, v.mLevel); };
        return tie(lhs) == tie(rhs);
    }

    static inline bool operator ==(const CellWater& lhs, const CellWater& rhs)
    {
        const auto tie = [] (const CellWater& v) { return std::tie(v.mCellPosition, v.mWater); };
        return tie(lhs) == tie(rhs);
    }

    static inline bool operator==(const Heightfield& lhs, const Heightfield& rhs)
    {
        return makeTuple(lhs) == makeTuple(rhs);
    }

    static inline bool operator==(const FlatHeightfield& lhs, const FlatHeightfield& rhs)
    {
        const auto tie = [] (const FlatHeightfield& v)
        {
            return std::tie(v.mCellPosition, v.mCellSize, v.mHeight);
        };
        return tie(lhs) == tie(rhs);
    }
}

namespace
{
    using namespace testing;
    using namespace DetourNavigator;

    struct DetourNavigatorRecastMeshBuilderTest : Test
    {
        TileBounds mBounds;
        const std::size_t mGeneration = 0;
        const std::size_t mRevision = 0;
        const osg::ref_ptr<const Resource::BulletShape> mSource {nullptr};
        const ObjectTransform mObjectTransform {ESM::Position {{0, 0, 0}, {0, 0, 0}}, 0.0f};

        DetourNavigatorRecastMeshBuilderTest()
        {
            mBounds.mMin = osg::Vec2f(-std::numeric_limits<float>::max() * std::numeric_limits<float>::epsilon(),
                                      -std::numeric_limits<float>::max() * std::numeric_limits<float>::epsilon());
            mBounds.mMax = osg::Vec2f(std::numeric_limits<float>::max() * std::numeric_limits<float>::epsilon(),
                                      std::numeric_limits<float>::max() * std::numeric_limits<float>::epsilon());
        }
    };

    TEST_F(DetourNavigatorRecastMeshBuilderTest, create_for_empty_should_return_empty)
    {
        RecastMeshBuilder builder(mBounds);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>());
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>());
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>());
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_bhv_triangle_mesh_shape)
    {
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape shape(&mesh, true);

        RecastMeshBuilder builder(mBounds);
        builder.addObject(static_cast<const btCollisionShape&>(shape), btTransform::getIdentity(),
                          AreaType_ground, mSource, mObjectTransform);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -1, -1, 0,
            -1, 1, 0,
            1, -1, 0,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_transformed_bhv_triangle_mesh_shape)
    {
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape shape(&mesh, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform(btMatrix3x3::getIdentity().scaled(btVector3(1, 2, 3)), btVector3(1, 2, 3)),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            0, 0, 3,
            0, 4, 3,
            2, 0, 3,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_heightfield_terrian_shape)
    {
        const std::array<btScalar, 4> heightfieldData {{0, 0, 0, 0}};
        btHeightfieldTerrainShape shape(2, 2, heightfieldData.data(), 1, 0, 0, 2, PHY_FLOAT, false);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(static_cast<const btCollisionShape&>(shape), btTransform::getIdentity(),
                          AreaType_ground, mSource, mObjectTransform);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -0.5, -0.5, 0,
            -0.5, 0.5, 0,
            0.5, -0.5, 0,
            0.5, 0.5, 0,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({0, 1, 2, 2, 1, 3}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground, AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_box_shape_should_produce_12_triangles)
    {
        btBoxShape shape(btVector3(1, 1, 2));
        RecastMeshBuilder builder(mBounds);
        builder.addObject(static_cast<const btCollisionShape&>(shape), btTransform::getIdentity(),
                          AreaType_ground, mSource, mObjectTransform);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -1, -1, -2,
            -1, -1, 2,
            -1, 1, -2,
            -1, 1, 2,
            1, -1, -2,
            1, -1, 2,
            1, 1, -2,
            1, 1, 2,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({
            0, 1, 5,
            0, 2, 3,
            0, 4, 6,
            1, 3, 7,
            2, 6, 7,
            3, 1, 0,
            4, 5, 7,
            5, 4, 0,
            6, 2, 0,
            7, 3, 2,
            7, 5, 1,
            7, 6, 4,
        })) << recastMesh->getMesh().getIndices();
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>(12, AreaType_ground));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_compound_shape)
    {
        btTriangleMesh mesh1;
        mesh1.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape triangle1(&mesh1, true);
        btBoxShape box(btVector3(1, 1, 2));
        btTriangleMesh mesh2;
        mesh2.addTriangle(btVector3(1, 1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape triangle2(&mesh2, true);
        btCompoundShape shape;
        shape.addChildShape(btTransform::getIdentity(), &triangle1);
        shape.addChildShape(btTransform::getIdentity(), &box);
        shape.addChildShape(btTransform::getIdentity(), &triangle2);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform::getIdentity(),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -1, -1, -2,
            -1, -1, 0,
            -1, -1, 2,
            -1, 1, -2,
            -1, 1, 0,
            -1, 1, 2,
            1, -1, -2,
            1, -1, 0,
            1, -1, 2,
            1, 1, -2,
            1, 1, 0,
            1, 1, 2,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({
            0, 2, 8,
            0, 3, 5,
            0, 6, 9,
            2, 5, 11,
            3, 9, 11,
            5, 2, 0,
            6, 8, 11,
            7, 4, 1,
            7, 4, 10,
            8, 6, 0,
            9, 3, 0,
            11, 5, 3,
            11, 8, 2,
            11, 9, 6,
        })) << recastMesh->getMesh().getIndices();
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>(14, AreaType_ground));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_transformed_compound_shape)
    {
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape triangle(&mesh, true);
        btCompoundShape shape;
        shape.addChildShape(btTransform::getIdentity(), &triangle);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform(btMatrix3x3::getIdentity().scaled(btVector3(1, 2, 3)), btVector3(1, 2, 3)),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            0, 0, 3,
            0, 4, 3,
            2, 0, 3,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_transformed_compound_shape_with_transformed_bhv_triangle_shape)
    {
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape triangle(&mesh, true);
        btCompoundShape shape;
        shape.addChildShape(btTransform(btMatrix3x3::getIdentity().scaled(btVector3(1, 2, 3)), btVector3(1, 2, 3)),
                            &triangle);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform(btMatrix3x3::getIdentity().scaled(btVector3(1, 2, 3)), btVector3(1, 2, 3)),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            1, 2, 12,
            1, 10, 12,
            3, 2, 12,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, without_bounds_add_bhv_triangle_shape_should_not_filter_by_bounds)
    {
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        mesh.addTriangle(btVector3(-3, -3, 0), btVector3(-3, -2, 0), btVector3(-2, -3, 0));
        btBvhTriangleMeshShape shape(&mesh, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform::getIdentity(),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -3, -3, 0,
            -3, -2, 0,
            -2, -3, 0,
            -1, -1, 0,
            -1, 1, 0,
            1, -1, 0,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0, 5, 4, 3}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>(2, AreaType_ground));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, with_bounds_add_bhv_triangle_shape_should_filter_by_bounds)
    {
        mBounds.mMin = osg::Vec2f(-3, -3);
        mBounds.mMax = osg::Vec2f(-2, -2);
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        mesh.addTriangle(btVector3(-3, -3, 0), btVector3(-3, -2, 0), btVector3(-2, -3, 0));
        btBvhTriangleMeshShape shape(&mesh, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform::getIdentity(),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -3, -3, 0,
            -3, -2, 0,
            -2, -3, 0,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, with_bounds_add_rotated_by_x_bhv_triangle_shape_should_filter_by_bounds)
    {
        mBounds.mMin = osg::Vec2f(-5, -5);
        mBounds.mMax = osg::Vec2f(5, -2);
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(0, -1, -1), btVector3(0, -1, -1), btVector3(0, 1, -1));
        mesh.addTriangle(btVector3(0, -3, -3), btVector3(0, -3, -2), btVector3(0, -2, -3));
        btBvhTriangleMeshShape shape(&mesh, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform(btQuaternion(btVector3(1, 0, 0),
            static_cast<btScalar>(-osg::PI_4))),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_THAT(recastMesh->getMesh().getVertices(), Pointwise(FloatNear(1e-5), std::vector<float>({
            0, -4.24264049530029296875, 4.44089209850062616169452667236328125e-16,
            0, -3.535533905029296875, -0.707106769084930419921875,
            0, -3.535533905029296875, 0.707106769084930419921875,
        }))) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({1, 2, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, with_bounds_add_rotated_by_y_bhv_triangle_shape_should_filter_by_bounds)
    {
        mBounds.mMin = osg::Vec2f(-5, -5);
        mBounds.mMax = osg::Vec2f(-3, 5);
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, 0, -1), btVector3(-1, 0, 1), btVector3(1, 0, -1));
        mesh.addTriangle(btVector3(-3, 0, -3), btVector3(-3, 0, -2), btVector3(-2, 0, -3));
        btBvhTriangleMeshShape shape(&mesh, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform(btQuaternion(btVector3(0, 1, 0),
            static_cast<btScalar>(osg::PI_4))),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_THAT(recastMesh->getMesh().getVertices(), Pointwise(FloatNear(1e-5), std::vector<float>({
            -4.24264049530029296875, 0, 4.44089209850062616169452667236328125e-16,
            -3.535533905029296875, 0, -0.707106769084930419921875,
            -3.535533905029296875, 0, 0.707106769084930419921875,
        }))) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({1, 2, 0}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, with_bounds_add_rotated_by_z_bhv_triangle_shape_should_filter_by_bounds)
    {
        mBounds.mMin = osg::Vec2f(-5, -5);
        mBounds.mMax = osg::Vec2f(-1, -1);
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        mesh.addTriangle(btVector3(-3, -3, 0), btVector3(-3, -2, 0), btVector3(-2, -3, 0));
        btBvhTriangleMeshShape shape(&mesh, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape),
            btTransform(btQuaternion(btVector3(0, 0, 1),
            static_cast<btScalar>(osg::PI_4))),
            AreaType_ground, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_THAT(recastMesh->getMesh().getVertices(), Pointwise(FloatNear(1e-5), std::vector<float>({
            -1.41421353816986083984375, -1.1102230246251565404236316680908203125e-16, 0,
            1.1102230246251565404236316680908203125e-16, -1.41421353816986083984375, 0,
            1.41421353816986083984375, 1.1102230246251565404236316680908203125e-16, 0,
        }))) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 0, 1}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, flags_values_should_be_corresponding_to_added_objects)
    {
        btTriangleMesh mesh1;
        mesh1.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape shape1(&mesh1, true);
        btTriangleMesh mesh2;
        mesh2.addTriangle(btVector3(-3, -3, 0), btVector3(-3, -2, 0), btVector3(-2, -3, 0));
        btBvhTriangleMeshShape shape2(&mesh2, true);
        RecastMeshBuilder builder(mBounds);
        builder.addObject(
            static_cast<const btCollisionShape&>(shape1),
            btTransform::getIdentity(),
            AreaType_ground, mSource, mObjectTransform
        );
        builder.addObject(
            static_cast<const btCollisionShape&>(shape2),
            btTransform::getIdentity(),
            AreaType_null, mSource, mObjectTransform
        );
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -3, -3, 0,
            -3, -2, 0,
            -2, -3, 0,
            -1, -1, 0,
            -1, 1, 0,
            1, -1, 0,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0, 5, 4, 3}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_null, AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_water_then_get_water_should_return_it)
    {
        RecastMeshBuilder builder(mBounds);
        builder.addWater(osg::Vec2i(1, 2), Water {1000, 300.0f});
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getWater(), std::vector<CellWater>({
            CellWater {osg::Vec2i(1, 2), Water {1000, 300.0f}}
        }));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_bhv_triangle_mesh_shape_with_duplicated_vertices)
    {
        btTriangleMesh mesh;
        mesh.addTriangle(btVector3(-1, -1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        mesh.addTriangle(btVector3(1, 1, 0), btVector3(-1, 1, 0), btVector3(1, -1, 0));
        btBvhTriangleMeshShape shape(&mesh, true);

        RecastMeshBuilder builder(mBounds);
        builder.addObject(static_cast<const btCollisionShape&>(shape), btTransform::getIdentity(), AreaType_ground, mSource, mObjectTransform);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getMesh().getVertices(), std::vector<float>({
            -1, -1, 0,
            -1, 1, 0,
            1, -1, 0,
            1, 1, 0,
        })) << recastMesh->getMesh().getVertices();
        EXPECT_EQ(recastMesh->getMesh().getIndices(), std::vector<int>({2, 1, 0, 2, 1, 3}));
        EXPECT_EQ(recastMesh->getMesh().getAreaTypes(), std::vector<AreaType>({AreaType_ground, AreaType_ground}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_flat_heightfield_should_add_intersection)
    {
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 1000;
        const float height = 10;
        mBounds.mMin = osg::Vec2f(100, 100);
        RecastMeshBuilder builder(mBounds);
        builder.addHeightfield(cellPosition, cellSize, height);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        EXPECT_EQ(recastMesh->getFlatHeightfields(), std::vector<FlatHeightfield>({
            FlatHeightfield {cellPosition, cellSize, height},
        }));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_heightfield_inside_tile)
    {
        constexpr std::size_t size = 3;
        constexpr std::array<float, size * size> heights {{
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
        }};
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 1000;
        const float minHeight = 0;
        const float maxHeight = 8;
        RecastMeshBuilder builder(mBounds);
        builder.addHeightfield(cellPosition, cellSize, heights.data(), size, minHeight, maxHeight);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        Heightfield expected;
        expected.mCellPosition = cellPosition;
        expected.mCellSize = cellSize;
        expected.mLength = size;
        expected.mMinHeight = minHeight;
        expected.mMaxHeight = maxHeight;
        expected.mHeights = {
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
        };
        expected.mOriginalSize = 3;
        expected.mMinX = 0;
        expected.mMinY = 0;
        EXPECT_EQ(recastMesh->getHeightfields(), std::vector<Heightfield>({expected}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_heightfield_to_shifted_cell_inside_tile)
    {
        constexpr std::size_t size = 3;
        constexpr std::array<float, size * size> heights {{
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
        }};
        const osg::Vec2i cellPosition(1, 2);
        const int cellSize = 1000;
        const float minHeight = 0;
        const float maxHeight = 8;
        RecastMeshBuilder builder(maxCellTileBounds(cellPosition, cellSize));
        builder.addHeightfield(cellPosition, cellSize, heights.data(), size, minHeight, maxHeight);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        Heightfield expected;
        expected.mCellPosition = cellPosition;
        expected.mCellSize = cellSize;
        expected.mLength = size;
        expected.mMinHeight = minHeight;
        expected.mMaxHeight = maxHeight;
        expected.mHeights = {
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
        };
        expected.mOriginalSize = 3;
        expected.mMinX = 0;
        expected.mMinY = 0;
        EXPECT_EQ(recastMesh->getHeightfields(), std::vector<Heightfield>({expected}));
    }

    TEST_F(DetourNavigatorRecastMeshBuilderTest, add_heightfield_should_add_intersection)
    {
        constexpr std::size_t size = 3;
        constexpr std::array<float, 3 * 3> heights {{
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
        }};
        const osg::Vec2i cellPosition(0, 0);
        const int cellSize = 1000;
        const float minHeight = 0;
        const float maxHeight = 8;
        mBounds.mMin = osg::Vec2f(750, 750);
        RecastMeshBuilder builder(mBounds);
        builder.addHeightfield(cellPosition, cellSize, heights.data(), size, minHeight, maxHeight);
        const auto recastMesh = std::move(builder).create(mGeneration, mRevision);
        Heightfield expected;
        expected.mCellPosition = cellPosition;
        expected.mCellSize = cellSize;
        expected.mLength = 2;
        expected.mMinHeight = 0;
        expected.mMaxHeight = 8;
        expected.mHeights = {
            4, 5,
            7, 8,
        };
        expected.mOriginalSize = 3;
        expected.mMinX = 1;
        expected.mMinY = 1;
        EXPECT_EQ(recastMesh->getHeightfields(), std::vector<Heightfield>({expected}));
    }
}
