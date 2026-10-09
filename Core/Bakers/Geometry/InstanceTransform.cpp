#include "StdAfx.h"
#include "InstanceTransform.h"

#include <Cry3DEngine/I3DEngine.h>

namespace JDKLevelMaps::JDKEditorSource::Geometry
{
	namespace
	{
		[[nodiscard]] Vec3 CalculateAlignmentNormal(const Vec3& pos)
		{
			constexpr float kRange = 0.5f + 0.05f;
			const auto z = [](float x, float y)
				{ return gEnv->p3DEngine->GetTerrainElevation(x, y); };

			const Vec3 v1(pos.x - kRange, pos.y - kRange, z(pos.x - kRange, pos.y - kRange));
			const Vec3 v2(pos.x - kRange, pos.y + kRange, z(pos.x - kRange, pos.y + kRange));
			const Vec3 v3(pos.x + kRange, pos.y - kRange, z(pos.x + kRange, pos.y - kRange));
			const Vec3 v4(pos.x + kRange, pos.y + kRange, z(pos.x + kRange, pos.y + kRange));

			return (v3 - v2).Cross(v4 - v1).GetNormalized();
		}
	}

	SPlanTransform CalculateInstancePlanTransform(const Vec3& pos, float scale, float angleZ, float angleX, float angleY, float alignToTerrain)
	{
		Matrix34 m;

		if (alignToTerrain == 0.0f)
			m = Matrix33::CreateRotationXYZ(Ang3(angleX, angleY, angleZ));
		else
		{
			Vec3 normal = CalculateAlignmentNormal(pos);
			normal = LERP(Vec3(0, 0, 1.0f), normal, alignToTerrain);
			normal.Normalize();

			const Vec3 dir = Vec3(-1, 0, 0).Cross(normal);
			const Matrix33 align = Matrix33::CreateOrientation(dir, -normal, 0);

			m = align * Matrix33::CreateRotationZ(angleZ);
		}

		SPlanTransform t;
		t.a00 = scale * m.m00;
		t.a01 = scale * m.m01;
		t.a10 = scale * m.m10;
		t.a11 = scale * m.m11;
		t.leanX = scale * m.m02;
		t.leanY = scale * m.m12;
		return t;
	}

	float QuantizeEditorAngle(float radians) noexcept
	{
		const uint8 byte = static_cast<uint8>(static_cast<int>(radians * (255.0f / gf_PI2)));
		return static_cast<float>(byte) * (gf_PI2 / 255.0f);
	}
}