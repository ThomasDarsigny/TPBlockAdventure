#ifndef TRANSFORMATION_H__
#define TRANSFORMATION_H__

#include "matrix4.h"
#include "vector3.h"
#include <stack>

class Transformation
{
    public:
        Transformation();

        void SetIdentity();

        void Push();
        void Pop();

        void ApplyTranslation(float x, float y, float z);
        void ApplyTranslation(const Vector3f& v);

        void ApplyRotation(float angle, float x, float y, float z);
        void ApplyRotation(float angle, const Vector3f& v);

        void ApplyScale(float x, float y, float z);
        void ApplyScale(const Vector3f& v);

        void Use() const;

        const Matrix4f& GetMatrix() const;

    private:
        std::stack<Matrix4f> m_stack;
};

#endif // TRANSFORMATION_H__
