#ifndef ARRAY3D_H__
#define ARRAY3D_H__

#include "define.h"

template <typename T>
class Array2d
{
public:
    Array3d(int x, int y) : m_x(x), m_y(y)
    {
        int size = x * y;
        m_data = new T[size];
        Reset(T());
    }

    ~Array3d()
    {
        delete[] m_data;
    }

    Array3d(const Array3d& array) : m_x(array.m_x), m_y(array.m_y)
    {
        int size = m_x * m_y;
        m_data = new T[size];
        for (int i = 0; i < size; i++)
        {
            m_data[i] = array.m_data[i];
        }
    }

    void Set(int x, int y, const T& value)
    {
        int index = To1dIndex(x, y);
        if (index >= 0)
        {
            m_data[index] = value;
        }
    }

    T Get(int x, int y) const
    {
        int index = To1dIndex(x, y);
        if (index >= 0)
        {
            return m_data[index];
        }
        return T();
    }

    void Reset(const T& value)
    {
        int size = m_x * m_y;
        for (int i = 0; i < size; i++)
        {
            m_data[i] = value;
        }
    }

private:
    int To1dIndex(int x, int y) const
    {
        if (x < 0 || x >= m_x || y < 0 || y >= m_y)
        {
            return -1;
        }
        return x + y * m_x;
    }

private:
    int m_x, m_y;
    T* m_data;
};

#endif // ARRAY3D_H__

