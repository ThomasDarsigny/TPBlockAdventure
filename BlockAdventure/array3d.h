#ifndef ARRAY3D_H__
#define ARRAY3D_H__

#include "define.h"

template <typename T>
class Array3d
{
public:
    Array3d(int x, int y, int z) : m_x(x), m_y(y), m_z(z)
    {
        int size = x * y * z;
        m_data = new T[size];
        Reset(T());
    }

    ~Array3d()
    {
        delete[] m_data;
    }

    Array3d(const Array3d& array) : m_x(array.m_x), m_y(array.m_y), m_z(array.m_z)
    {
        int size = m_x * m_y * m_z;
        m_data = new T[size];
        for (int i = 0; i < size; i++)
        {
            m_data[i] = array.m_data[i];
        }
    }

    void Set(int x, int y, int z, const T& value)
    {
        int index = To1dIndex(x, y, z);
        if (index >= 0)
        {
            m_data[index] = value;
        }
    }

    T Get(int x, int y, int z) const
    {
        int index = To1dIndex(x, y, z);
        if (index >= 0)
        {
            return m_data[index];
        }
        return T();
    }

    void Reset(const T& value)
    {
        int size = m_x * m_y * m_z;
        for (int i = 0; i < size; i++)
        {
            m_data[i] = value;
        }
    }

private:
    int To1dIndex(int x, int y, int z) const
    {
        if (x < 0 || x >= m_x || y < 0 || y >= m_y || z < 0 || z >= m_z)
        {
            return -1;
        }
        return x + y * m_x + z * m_x * m_y;
    }

private:
    int m_x, m_y, m_z;
    T* m_data;
};

#endif // ARRAY3D_H__
