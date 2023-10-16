template <typename BlockType>
class BlockArray3d {
public:
    BlockArray3d(int x, int y, int z);
    ~BlockArray3d();
    BlockArray3d(const BlockArray3d& array);

    void Set(int x, int y, int z, BlockType type);
    BlockType Get(int x, int y, int z) const;
    void Reset(BlockType type);

private:
    int To1dIndex(int x, int y, int z) const;

    int m_x;
    int m_y;
    int m_z;
    BlockType* m_blocks;
};

template <typename BlockType>
BlockArray3d<BlockType>::BlockArray3d(int x, int y, int z) : m_x(x), m_y(y), m_z(z)
{
    m_blocks = new BlockType[m_x * m_y * m_z];
    Reset(BTYPE_AIR);
}

template <typename BlockType>
BlockArray3d<BlockType>::~BlockArray3d()
{
    delete[] m_blocks;
}

template <typename BlockType>
BlockArray3d<BlockType>::BlockArray3d(const BlockArray3d& array) : m_x(array.m_x), m_y(array.m_y), m_z(array.m_z)
{
    m_blocks = new BlockType[m_x * m_y * m_z];
    for (int i = 0; i < m_x * m_y * m_z; ++i)
        m_blocks[i] = array.m_blocks[i];
}

template <typename BlockType>
void BlockArray3d<BlockType>::Set(int x, int y, int z, BlockType type)
{
    m_blocks[To1dIndex(x, y, z)] = type;
}

template <typename BlockType>
BlockType BlockArray3d<BlockType>::Get(int x, int y, int z) const
{
    return m_blocks[To1dIndex(x, y, z)];
}

template <typename BlockType>
void BlockArray3d<BlockType>::Reset(BlockType type)
{
    for (int i = 0; i < m_x * m_y * m_z; ++i)
        m_blocks[i] = type;
}

template <typename BlockType>
int BlockArray3d<BlockType>::To1dIndex(int x, int y, int z) const
{
    return x + (z * m_x) + (y * m_z * m_x);
}
