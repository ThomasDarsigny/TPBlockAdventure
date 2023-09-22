#include "transformation.h"

Transformation::Transformation()
{
    m_stack.push(Matrix4f::IDENTITY);
}

void Transformation::SetIdentity()
{
    m_stack.top().SetIdentity();
}

void Transformation::Push()
{
    m_stack.push(m_stack.top());
}

void Transformation::Pop()
{
    m_stack.pop();
}

void Transformation::ApplyTranslation(float x, float y, float z)
{
    m_stack.top().ApplyTranslation(x, y, z);
}

void Transformation::ApplyTranslation(const Vector3f& v)
{
    ApplyTranslation(v.x, v.y, v.z);
}


void Transformation::ApplyRotation(float angle, float x, float y, float z)
{
    m_stack.top().ApplyRotation(angle, x, y, z);
}

void Transformation::ApplyRotation(float angle, const Vector3f& v)
{
    ApplyRotation(angle, v.x, v.y, v.z);
}

void Transformation::ApplyScale(float x, float y, float z)
{
    m_stack.top().ApplyScale(x, y, z);
}

void Transformation::ApplyScale(const Vector3f& v)
{
    ApplyScale(v.x, v.y, v.z);
}

void Transformation::Use() const
{
    glLoadMatrixf(m_stack.top().GetInternalValues());
}

const Matrix4f& Transformation::GetMatrix() const
{
    return m_stack.top();
}

