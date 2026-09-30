#include "world.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <iostream>

namespace
{
    const int UNLOAD_MARGIN = 3;      // chunks conserves au-dela de la distance d'affichage
    // Insertion d'un chunk = allumage complet + reveil des liquides. Plutot
    // qu'un nombre fixe, on s'accorde un budget de temps: une machine rapide
    // charge le monde bien plus vite, une machine lente ne saccade pas.
    const int    MAX_INSERT_PER_FRAME = 8;
    const double INSERT_BUDGET_MS = 2.0;
    const int MAX_MESH_JOBS_PER_FRAME = 8;
    const int MAX_QUEUED_GEN = 96;    // limite la file pour rester reactif

    // Rythme de la simulation des blocs (ecoulement, chute).
    const float BLOCK_TICK = 0.20f;
    const int   MAX_UPDATES_PER_TICK = 4000;
}

// ---------------------------------------------------------------------------
//  Construction / destruction
// ---------------------------------------------------------------------------
World::World(uint32_t seed)
    : m_gen(seed), m_renderDistance(DEFAULT_RENDER_DISTANCE), m_drawnVertices(0),
      m_playerCx(0), m_playerCz(0), m_firstUpdate(true), m_tickAccum(0.0f),
      m_running(false), m_inFlight(0)
{
    StartWorkers();
}

World::~World()
{
    Shutdown();
}

void World::StartWorkers()
{
    unsigned int n = std::thread::hardware_concurrency();
    if (n < 2) n = 2;
    if (n > 7) n = 7;
    n -= 1;                       // on laisse un coeur au thread principal
    if (n < 1) n = 1;

    m_running = true;
    for (unsigned int i = 0; i < n; ++i)
        m_workers.push_back(std::thread(&World::WorkerLoop, this));

    std::cout << "[World] " << n << " threads de generation" << std::endl;
}

void World::Shutdown()
{
    if (!m_running)
        return;

    m_running = false;
    m_queueCv.notify_all();

    for (size_t i = 0; i < m_workers.size(); ++i)
        if (m_workers[i].joinable())
            m_workers[i].join();
    m_workers.clear();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (size_t i = 0; i < m_meshQueue.size(); ++i)
            delete m_meshQueue[i];
        m_meshQueue.clear();
        m_genQueue.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        for (size_t i = 0; i < m_genResults.size(); ++i)
            delete m_genResults[i].chunk;
        m_genResults.clear();
        for (size_t i = 0; i < m_meshResults.size(); ++i)
            delete m_meshResults[i];
        m_meshResults.clear();
    }

    for (std::unordered_map<Key, Chunk*>::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
        delete it->second;
    m_chunks.clear();
    m_visible.clear();
}

// ---------------------------------------------------------------------------
//  Threads de travail
// ---------------------------------------------------------------------------
void World::WorkerLoop()
{
    for (;;)
    {
        MeshJob* mesh = 0;
        GenJob gen;
        bool hasGen = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait(lock, [this] {
                return !m_running || !m_meshQueue.empty() || !m_genQueue.empty();
            });

            if (!m_running)
                return;

            // Les maillages sont prioritaires: un chunk deja genere mais non
            // maille est un trou visible a l'ecran.
            if (!m_meshQueue.empty())
            {
                mesh = m_meshQueue.front();
                m_meshQueue.pop_front();
            }
            else if (!m_genQueue.empty())
            {
                gen = m_genQueue.front();
                m_genQueue.pop_front();
                hasGen = true;
            }
        }

        if (mesh)
        {
            MeshResult* res = new MeshResult();
            res->cx = mesh->cx;
            res->cz = mesh->cz;
            ChunkMesher::Build(mesh->snap, res->data);
            delete mesh;

            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_meshResults.push_back(res);
        }
        else if (hasGen)
        {
            Chunk* c = new Chunk(gen.cx, gen.cz);
            m_gen.Generate(*c);

            GenResult res;
            res.chunk = c;

            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_genResults.push_back(res);
        }
    }
}

int World::PendingJobs() const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return (int)(m_genQueue.size() + m_meshQueue.size());
}

// ---------------------------------------------------------------------------
//  Acces aux chunks / blocs
// ---------------------------------------------------------------------------
Chunk* World::GetChunk(int cx, int cz)
{
    std::unordered_map<Key, Chunk*>::iterator it = m_chunks.find(MakeKey(cx, cz));
    return (it == m_chunks.end()) ? 0 : it->second;
}

const Chunk* World::GetChunk(int cx, int cz) const
{
    std::unordered_map<Key, Chunk*>::const_iterator it = m_chunks.find(MakeKey(cx, cz));
    return (it == m_chunks.end()) ? 0 : it->second;
}

BlockType World::GetBlock(int wx, int wy, int wz) const
{
    if (wy < 0 || wy >= CHUNK_SIZE_Y)
        return BTYPE_AIR;

    const Chunk* c = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!c) return BTYPE_AIR;

    return c->GetBlock(LocalCoord(wx), wy, LocalCoord(wz));
}

unsigned char World::GetSkyLight(int wx, int wy, int wz) const
{
    if (wy < 0) return 0;
    if (wy >= CHUNK_SIZE_Y) return MAX_LIGHT;

    const Chunk* c = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!c) return MAX_LIGHT;

    return c->GetSkyLight(LocalCoord(wx), wy, LocalCoord(wz));
}

unsigned char World::GetBlockLight(int wx, int wy, int wz) const
{
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return 0;

    const Chunk* c = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!c) return 0;

    return c->GetBlockLight(LocalCoord(wx), wy, LocalCoord(wz));
}

unsigned char World::GetMeta(int wx, int wy, int wz) const
{
    if (wy < 0 || wy >= CHUNK_SIZE_Y)
        return 0;

    const Chunk* c = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!c) return 0;

    return c->GetMeta(LocalCoord(wx), wy, LocalCoord(wz));
}

bool World::IsSolid(int wx, int wy, int wz) const
{
    return Blocks::IsSolid(GetBlock(wx, wy, wz));
}

bool World::IsLiquidAt(const Vector3f& p) const
{
    return Blocks::IsLiquid(GetBlock((int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z)));
}

// Ecriture sans recalcul de lumiere: la chunk est notee, on rallumera tout
// en une fois a la fin du tick.
bool World::SetBlockQuiet(int wx, int wy, int wz, BlockType t, unsigned char meta)
{
    if (wy < 0 || wy >= CHUNK_SIZE_Y)
        return false;

    const int cx = ChunkCoord(wx), cz = ChunkCoord(wz);
    Chunk* c = GetChunk(cx, cz);
    if (!c || !c->generated)
        return false;

    const int lx = LocalCoord(wx), lz = LocalCoord(wz);
    const BlockType oldType = c->GetBlock(lx, wy, lz);

    if (oldType == t && c->GetMeta(lx, wy, lz) == meta)
        return false;

    c->SetBlock(lx, wy, lz, t);
    c->SetMeta(lx, wy, lz, meta);
    c->UpdateHighest(lx, wy, lz, t);
    c->modified = true;

    m_relightTouched.insert(MakeKey(cx, cz));

    if (oldType != t)
        UpdateLightAt(wx, wy, wz, oldType, t);

    return true;
}

bool World::SetBlock(int wx, int wy, int wz, BlockType t, unsigned char meta)
{
    if (!SetBlockQuiet(wx, wy, wz, t, meta))
        return false;

    FlushDirtyChunks();

    // Le bloc pose ou casse peut declencher un ecoulement ou une chute
    ScheduleUpdate(wx, wy, wz);
    ScheduleNeighbours(wx, wy, wz);
    return true;
}

// ---------------------------------------------------------------------------
//  Simulation des blocs
// ---------------------------------------------------------------------------
void World::ScheduleUpdate(int wx, int wy, int wz)
{
    if (wy < 0 || wy >= CHUNK_SIZE_Y)
        return;

    const uint64_t k = PosKey(wx, wy, wz);
    if (!m_updateSet.insert(k).second)
        return;

    PendingUpdate u; u.x = wx; u.y = wy; u.z = wz;
    m_updates.push_back(u);
}

void World::ScheduleNeighbours(int wx, int wy, int wz)
{
    ScheduleUpdate(wx + 1, wy, wz);
    ScheduleUpdate(wx - 1, wy, wz);
    ScheduleUpdate(wx, wy, wz + 1);
    ScheduleUpdate(wx, wy, wz - 1);
    ScheduleUpdate(wx, wy + 1, wz);
    ScheduleUpdate(wx, wy - 1, wz);
}

void World::Tick(float elapsed)
{
    m_tickAccum += elapsed;
    if (m_tickAccum < BLOCK_TICK)
        return;
    m_tickAccum = 0.0f;

    if (m_updates.empty())
        return;

    // On travaille sur une copie: les mises a jour engendrees pendant ce tick
    // seront traitees au suivant, ce qui donne une progression reguliere de
    // l'ecoulement plutot qu'une propagation instantanee.
    std::vector<PendingUpdate> batch;
    batch.swap(m_updates);
    m_updateSet.clear();

    const size_t count = (batch.size() < (size_t)MAX_UPDATES_PER_TICK)
                       ? batch.size() : (size_t)MAX_UPDATES_PER_TICK;

    for (size_t i = 0; i < count; ++i)
        UpdateBlock(batch[i].x, batch[i].y, batch[i].z);

    // Le reste attendra le tick suivant
    for (size_t i = count; i < batch.size(); ++i)
        ScheduleUpdate(batch[i].x, batch[i].y, batch[i].z);

    FlushDirtyChunks();
}

void World::UpdateBlock(int wx, int wy, int wz)
{
    const BlockType t = GetBlock(wx, wy, wz);

    if (Blocks::IsLiquid(t))
        UpdateFluid(wx, wy, wz, t);
    else if (Blocks::HasGravity(t))
        UpdateFalling(wx, wy, wz, t);
}

void World::UpdateFalling(int wx, int wy, int wz, BlockType t)
{
    if (wy <= 1)
        return;

    const BlockType below = GetBlock(wx, wy - 1, wz);
    if (!Blocks::IsReplaceable(below))
        return;

    SetBlockQuiet(wx, wy, wz, BTYPE_AIR, 0);
    SetBlockQuiet(wx, wy - 1, wz, t, 0);

    ScheduleUpdate(wx, wy - 1, wz);
    ScheduleNeighbours(wx, wy, wz);
}

void World::UpdateFluid(int wx, int wy, int wz, BlockType t)
{
    static const int kSide[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

    const int range = Blocks::FluidRange(t);
    const unsigned char meta = GetMeta(wx, wy, wz);
    const int level = meta & FLUID_LEVEL_MASK;
    const bool falling = (meta & FLUID_FALLING) != 0;
    const bool isSource = (level == 0) && !falling;

    // --- La lave qui touche de l'eau se fige --------------------------------
    if (t == BTYPE_LAVA)
    {
        bool touchesWater = false;
        for (int s = 0; s < 4 && !touchesWater; ++s)
            if (GetBlock(wx + kSide[s][0], wy, wz + kSide[s][1]) == BTYPE_WATER)
                touchesWater = true;
        if (!touchesWater && GetBlock(wx, wy + 1, wz) == BTYPE_WATER)
            touchesWater = true;

        if (touchesWater)
        {
            SetBlockQuiet(wx, wy, wz, isSource ? BTYPE_OBSIDIAN : BTYPE_COBBLESTONE, 0);
            ScheduleNeighbours(wx, wy, wz);
            return;
        }
    }

    // --- Un filet non alimente se retire ------------------------------------
    if (!isSource)
    {
        int best = 99;
        bool fedFromAbove = (GetBlock(wx, wy + 1, wz) == t);

        if (!fedFromAbove)
        {
            for (int s = 0; s < 4; ++s)
            {
                const int nx = wx + kSide[s][0], nz = wz + kSide[s][1];
                if (GetBlock(nx, wy, nz) != t)
                    continue;

                const unsigned char nm = GetMeta(nx, wy, nz);
                if (nm & FLUID_FALLING)
                    continue;                     // une chute n'alimente pas les cotes

                const int nl = (nm & FLUID_LEVEL_MASK) + 1;
                if (nl < best) best = nl;
            }
        }

        const int wanted = fedFromAbove ? 0 : best;
        const bool wantFalling = fedFromAbove;

        if (!fedFromAbove && wanted > range)
        {
            SetBlockQuiet(wx, wy, wz, BTYPE_AIR, 0);
            ScheduleNeighbours(wx, wy, wz);
            return;
        }

        if (wanted != level || wantFalling != falling)
        {
            SetBlockQuiet(wx, wy, wz, t,
                          (unsigned char)(wanted | (wantFalling ? FLUID_FALLING : 0)));
            ScheduleNeighbours(wx, wy, wz);
            return;
        }
    }

    // --- Ecoulement vers le bas --------------------------------------------
    const BlockType below = GetBlock(wx, wy - 1, wz);
    if (wy > 0 && below != t && Blocks::IsReplaceable(below))
    {
        if (t == BTYPE_LAVA && below == BTYPE_WATER)
        {
            SetBlockQuiet(wx, wy - 1, wz, BTYPE_COBBLESTONE, 0);
        }
        else
        {
            SetBlockQuiet(wx, wy - 1, wz, t, FLUID_FALLING);
            ScheduleUpdate(wx, wy - 1, wz);
        }
        ScheduleNeighbours(wx, wy - 1, wz);
        return;   // un liquide qui tombe ne s'etale pas sur les cotes
    }

    if (below == t)
        return;   // deja alimente vers le bas

    // --- Etalement horizontal ----------------------------------------------
    const int nextLevel = (falling ? 0 : level) + 1;
    if (nextLevel > range)
        return;

    for (int s = 0; s < 4; ++s)
    {
        const int nx = wx + kSide[s][0], nz = wz + kSide[s][1];
        const BlockType nb = GetBlock(nx, wy, nz);

        if (nb == t)
        {
            const unsigned char nm = GetMeta(nx, wy, nz);
            if ((nm & FLUID_FALLING) == 0 && (nm & FLUID_LEVEL_MASK) > nextLevel)
            {
                SetBlockQuiet(nx, wy, nz, t, (unsigned char)nextLevel);
                ScheduleUpdate(nx, wy, nz);
                ScheduleNeighbours(nx, wy, nz);
            }
        }
        else if (Blocks::IsReplaceable(nb))
        {
            if (t == BTYPE_LAVA && nb == BTYPE_WATER)
                continue;                      // gere par la regle de figeage

            SetBlockQuiet(nx, wy, nz, t, (unsigned char)nextLevel);
            ScheduleUpdate(nx, wy, nz);
            ScheduleNeighbours(nx, wy, nz);
        }
    }
}

// Au chargement d'une chunk: reveiller les liquides qui ne reposent sur rien
// (une grotte a pu percer le fond d'un lac).
void World::ScanChunkForUpdates(const Chunk& chunk)
{
    const int ox = chunk.Cx() * CHUNK_SIZE_X;
    const int oz = chunk.Cz() * CHUNK_SIZE_Z;

    const std::vector<BlockType>& blocks = chunk.Blocks();

    // Balayage a plat: l'index d'un etage est contigu, celui du dessous est
    // simplement a -stride. Aucun calcul d'indice par bloc.
    const int stride = CHUNK_SIZE_X * CHUNK_SIZE_Z;
    const BlockType* b = blocks.data();

    for (int y = 1; y < CHUNK_SIZE_Y; ++y)
    {
        const BlockType* row = b + y * stride;
        const BlockType* under = row - stride;

        for (int i = 0; i < stride; ++i)
        {
            const BlockType t = row[i];
            if (!Blocks::IsLiquid(t))
                continue;

            const BlockType d = under[i];
            if (d == t || !Blocks::IsReplaceable(d))
                continue;

            ScheduleUpdate(ox + (i % CHUNK_SIZE_X), y, oz + (i / CHUNK_SIZE_X));
        }
    }
}

// ---------------------------------------------------------------------------
//  Lumiere
// ---------------------------------------------------------------------------
Chunk* World::Neighbour(Chunk* from, int& lx, int& lz)
{
    if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z)
        return from;

    int cx = from->Cx(), cz = from->Cz();
    while (lx < 0)              { lx += CHUNK_SIZE_X; --cx; }
    while (lx >= CHUNK_SIZE_X)  { lx -= CHUNK_SIZE_X; ++cx; }
    while (lz < 0)              { lz += CHUNK_SIZE_Z; --cz; }
    while (lz >= CHUNK_SIZE_Z)  { lz -= CHUNK_SIZE_Z; ++cz; }

    return GetChunk(cx, cz);
}

namespace
{
    const int kDir[6][3] =
    {
        {  0,  1,  0 }, {  0, -1,  0 },
        { -1,  0,  0 }, {  1,  0,  0 },
        {  0,  0,  1 }, {  0,  0, -1 }
    };
}

void World::PropagateSky(std::vector<LightNode>& queue)
{
    size_t head = 0;
    while (head < queue.size())
    {
        const LightNode node = queue[head++];
        if (node.level <= 1)
            continue;

        // Une cellule peut avoir ete relevee depuis son empilement: c'est le
        // noeud le plus lumineux qui compte, celui-ci n'a plus rien a dire.
        if (node.chunk->GetSkyLight(node.x, node.y, node.z) > node.level)
            continue;

        for (int d = 0; d < 6; ++d)
        {
            int nx = node.x + kDir[d][0];
            int ny = node.y + kDir[d][1];
            int nz = node.z + kDir[d][2];

            if (ny < 0 || ny >= CHUNK_SIZE_Y)
                continue;

            Chunk* nc = Neighbour(node.chunk, nx, nz);
            if (!nc)
                continue;

            const BlockType nb = nc->GetBlock(nx, ny, nz);
            const int opacity = Blocks::Opacity(nb);
            if (opacity >= MAX_LIGHT)
                continue;

            // La lumiere du ciel descend sans perte tant qu'elle vaut 15
            int newLevel;
            if (d == 1 && node.level == MAX_LIGHT && opacity == 0)
                newLevel = MAX_LIGHT;
            else
                newLevel = node.level - (opacity > 1 ? opacity : 1);

            if (newLevel <= 0)
                continue;

            if (nc->GetSkyLight(nx, ny, nz) >= newLevel)
                continue;

            nc->SetSkyLight(nx, ny, nz, (unsigned char)newLevel);
            TouchChunk(nc);

            LightNode next;
            next.chunk = nc;
            next.x = (int16_t)nx; next.y = (int16_t)ny; next.z = (int16_t)nz;
            next.level = (uint8_t)newLevel;
            queue.push_back(next);
        }
    }
}

void World::PropagateBlock(std::vector<LightNode>& queue)
{
    size_t head = 0;
    while (head < queue.size())
    {
        const LightNode node = queue[head++];
        if (node.level <= 1)
            continue;

        if (node.chunk->GetBlockLight(node.x, node.y, node.z) > node.level)
            continue;

        for (int d = 0; d < 6; ++d)
        {
            int nx = node.x + kDir[d][0];
            int ny = node.y + kDir[d][1];
            int nz = node.z + kDir[d][2];

            if (ny < 0 || ny >= CHUNK_SIZE_Y)
                continue;

            Chunk* nc = Neighbour(node.chunk, nx, nz);
            if (!nc)
                continue;

            const BlockType nb = nc->GetBlock(nx, ny, nz);
            const int opacity = Blocks::Opacity(nb);
            if (opacity >= MAX_LIGHT)
                continue;

            const int newLevel = node.level - (opacity > 1 ? opacity : 1);
            if (newLevel <= 0)
                continue;

            if (nc->GetBlockLight(nx, ny, nz) >= newLevel)
                continue;

            nc->SetBlockLight(nx, ny, nz, (unsigned char)newLevel);
            TouchChunk(nc);

            LightNode next;
            next.chunk = nc;
            next.x = (int16_t)nx; next.y = (int16_t)ny; next.z = (int16_t)nz;
            next.level = (uint8_t)newLevel;
            queue.push_back(next);
        }
    }
}

void World::SeedFromNeighbours(Chunk& chunk, std::vector<LightNode>& sky, std::vector<LightNode>& blk)
{
    static const int kSide[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

    for (int s = 0; s < 4; ++s)
    {
        Chunk* n = GetChunk(chunk.Cx() + kSide[s][0], chunk.Cz() + kSide[s][1]);
        if (!n || !n->lit)
            continue;

        // Colonne de bordure du voisin qui touche notre chunk
        const int bx = (kSide[s][0] == -1) ? CHUNK_SIZE_X - 1 : (kSide[s][0] == 1 ? 0 : -1);
        const int bz = (kSide[s][1] == -1) ? CHUNK_SIZE_Z - 1 : (kSide[s][1] == 1 ? 0 : -1);

        const int x0 = (bx >= 0) ? bx : 0;
        const int x1 = (bx >= 0) ? bx : CHUNK_SIZE_X - 1;
        const int z0 = (bz >= 0) ? bz : 0;
        const int z1 = (bz >= 0) ? bz : CHUNK_SIZE_Z - 1;

        // Le voisin etait deja entierement eclaire quand il a ete insere:
        // ses cellules ne peuvent plus rien s'apprendre entre elles. La seule
        // chose que ces graines peuvent encore faire, c'est pousser de la
        // lumiere dans le chunk qui vient d'arriver. On ne garde donc que
        // celles qui eclaireraient effectivement la cellule d'en face --
        // au-dessus du sol, ou les deux cotes sont deja a 15, cela elimine
        // la quasi-totalite des milliers de graines d'autrefois.
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                {
                    // Cellule de notre chunk collee a cette cellule du voisin
                    const int ax = (kSide[s][0] == -1) ? 0 : (kSide[s][0] == 1 ? CHUNK_SIZE_X - 1 : x);
                    const int az = (kSide[s][1] == -1) ? 0 : (kSide[s][1] == 1 ? CHUNK_SIZE_Z - 1 : z);

                    const int op = Blocks::Opacity(chunk.GetBlock(ax, y, az));
                    if (op >= MAX_LIGHT)
                        continue;                       // notre cote est opaque

                    const int loss = (op > 1) ? op : 1;

                    const unsigned char sl = n->GetSkyLight(x, y, z);
                    if (sl > 1 && (int)sl - loss > (int)chunk.GetSkyLight(ax, y, az))
                    {
                        LightNode nd; nd.chunk = n;
                        nd.x = (int16_t)x; nd.y = (int16_t)y; nd.z = (int16_t)z;
                        nd.level = sl;
                        sky.push_back(nd);
                    }

                    const unsigned char bl = n->GetBlockLight(x, y, z);
                    if (bl > 1 && (int)bl - loss > (int)chunk.GetBlockLight(ax, y, az))
                    {
                        LightNode nd; nd.chunk = n;
                        nd.x = (int16_t)x; nd.y = (int16_t)y; nd.z = (int16_t)z;
                        nd.level = bl;
                        blk.push_back(nd);
                    }
                }
    }
}

// Altitude du plus haut bloc opaque d'une colonne voisine, en tenant compte
// du passage d'un chunk a l'autre. Si le voisin n'est pas encore charge on
// renvoie -1: il nous ensemencera lui-meme a son arrivee.
int World::NeighbourHighest(const Chunk& chunk, int lx, int lz) const
{
    if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z)
        return chunk.Highest(lx, lz);

    int cx = chunk.Cx(), cz = chunk.Cz();
    while (lx < 0)             { lx += CHUNK_SIZE_X; --cx; }
    while (lx >= CHUNK_SIZE_X) { lx -= CHUNK_SIZE_X; ++cx; }
    while (lz < 0)             { lz += CHUNK_SIZE_Z; --cz; }
    while (lz >= CHUNK_SIZE_Z) { lz -= CHUNK_SIZE_Z; ++cz; }

    const Chunk* n = GetChunk(cx, cz);
    return n ? n->Highest(lx, lz) : -1;
}

void World::InitialLight(Chunk& chunk)
{
    std::vector<LightNode> skyQueue;
    std::vector<LightNode> blkQueue;
    skyQueue.reserve(4096);
    blkQueue.reserve(256);

    // --- 1. Colonnes de lumiere du ciel ---
    // Au-dessus du plus haut bloc opaque tout vaut 15. Inutile d'empiler ces
    // cellules comme points de depart: elles ne peuvent rien eclairer de plus.
    // Seules celles qui longent une colonne plus haute (falaise, tronc,
    // surplomb) servent a propager la lumiere lateralement.
    for (int z = 0; z < CHUNK_SIZE_Z; ++z)
    {
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            const int h = chunk.Highest(x, z);

            int wall = h;
            const int hx0 = NeighbourHighest(chunk, x - 1, z);
            const int hx1 = NeighbourHighest(chunk, x + 1, z);
            const int hz0 = NeighbourHighest(chunk, x, z - 1);
            const int hz1 = NeighbourHighest(chunk, x, z + 1);
            if (hx0 > wall) wall = hx0;
            if (hx1 > wall) wall = hx1;
            if (hz0 > wall) wall = hz0;
            if (hz1 > wall) wall = hz1;

            // Tout ce qui surplombe le sol vaut 15: un seul passage suffit.
            chunk.FillSkyColumn(x, z, h + 1, CHUNK_SIZE_Y - 1, MAX_LIGHT);

            // Seules les cellules qui longent une colonne plus haute peuvent
            // encore eclairer quelque chose lateralement.
            int top = wall + 1;
            if (top > CHUNK_SIZE_Y - 1) top = CHUNK_SIZE_Y - 1;

            for (int y = top; y > h; --y)
            {
                LightNode nd; nd.chunk = &chunk;
                nd.x = (int16_t)x; nd.y = (int16_t)y; nd.z = (int16_t)z;
                nd.level = MAX_LIGHT;
                skyQueue.push_back(nd);
            }

            // Sous le sommet: la lumiere s'attenue selon l'opacite traversee
            int level = MAX_LIGHT;
            for (int y = h; y >= 0; --y)
            {
                const int op = Blocks::Opacity(chunk.GetBlock(x, y, z));
                level -= (op > 0) ? op : 0;
                if (level <= 0)
                    break;

                chunk.SetSkyLight(x, y, z, (unsigned char)level);

                LightNode nd; nd.chunk = &chunk;
                nd.x = (int16_t)x; nd.y = (int16_t)y; nd.z = (int16_t)z;
                nd.level = (uint8_t)level;
                skyQueue.push_back(nd);
            }
        }
    }

    // --- 2. Sources de lumiere (torches, lave, pierre lumineuse) ---
    // Balayage direct du tableau: la plupart des chunks n'en contiennent
    // aucune, autant eviter les conversions de coordonnees.
    {
        const std::vector<BlockType>& blocks = chunk.Blocks();
        const size_t n = blocks.size();
        for (size_t i = 0; i < n; ++i)
        {
            const unsigned char e = Blocks::Emission(blocks[i]);
            if (e == 0) continue;

            const int x = (int)(i % CHUNK_SIZE_X);
            const int z = (int)((i / CHUNK_SIZE_X) % CHUNK_SIZE_Z);
            const int y = (int)(i / (CHUNK_SIZE_X * CHUNK_SIZE_Z));

            chunk.SetBlockLight(x, y, z, e);

            LightNode nd; nd.chunk = &chunk;
            nd.x = (int16_t)x; nd.y = (int16_t)y; nd.z = (int16_t)z;
            nd.level = e;
            blkQueue.push_back(nd);
        }
    }

    // --- 3. Lumiere venant des chunks deja eclairees ---
    SeedFromNeighbours(chunk, skyQueue, blkQueue);

    PropagateSky(skyQueue);
    PropagateBlock(blkQueue);

    chunk.lit = true;
    MarkDirty(chunk.Cx(), chunk.Cz());
}

void World::TouchChunk(const Chunk* c)
{
    if (c) m_relightTouched.insert(MakeKey(c->Cx(), c->Cz()));
}

void World::FlushDirtyChunks()
{
    for (std::unordered_set<Key>::const_iterator it = m_relightTouched.begin();
         it != m_relightTouched.end(); ++it)
    {
        MarkDirty((int)((int64_t)*it >> 32), (int)(int32_t)(*it & 0xffffffffu));
    }
    m_relightTouched.clear();
}

// ---------------------------------------------------------------------------
//  Retrait de lumiere
//
//  On efface la lumiere qui provenait de la cellule modifiee (une cellule
//  voisine moins lumineuse tenait forcement sa lumiere de nous), tout en
//  collectant les cellules encore alimentees: elles serviront de graines pour
//  la repropagation.
// ---------------------------------------------------------------------------
void World::RemoveBlockLight(int wx, int wy, int wz, int level)
{
    Chunk* start = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!start) return;

    struct RNode { Chunk* chunk; int16_t x, y, z; uint8_t level; };
    std::vector<RNode> removal;
    std::vector<LightNode> readd;

    RNode first;
    first.chunk = start;
    first.x = (int16_t)LocalCoord(wx); first.y = (int16_t)wy; first.z = (int16_t)LocalCoord(wz);
    first.level = (uint8_t)level;
    removal.push_back(first);

    start->SetBlockLight(first.x, wy, first.z, 0);
    TouchChunk(start);

    size_t head = 0;
    while (head < removal.size())
    {
        const RNode node = removal[head++];

        for (int d = 0; d < 6; ++d)
        {
            int nx = node.x + kDir[d][0];
            int ny = node.y + kDir[d][1];
            int nz = node.z + kDir[d][2];
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            Chunk* nc = Neighbour(node.chunk, nx, nz);
            if (!nc) continue;

            const int nl = nc->GetBlockLight(nx, ny, nz);
            if (nl == 0) continue;

            if (nl < node.level)
            {
                nc->SetBlockLight(nx, ny, nz, 0);
                TouchChunk(nc);

                RNode next;
                next.chunk = nc;
                next.x = (int16_t)nx; next.y = (int16_t)ny; next.z = (int16_t)nz;
                next.level = (uint8_t)nl;
                removal.push_back(next);
            }
            else
            {
                LightNode seed;
                seed.chunk = nc;
                seed.x = (int16_t)nx; seed.y = (int16_t)ny; seed.z = (int16_t)nz;
                seed.level = (uint8_t)nl;
                readd.push_back(seed);
            }
        }
    }

    PropagateBlock(readd);
}

void World::RemoveSkyLight(int wx, int wy, int wz, int level)
{
    Chunk* start = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!start) return;

    struct RNode { Chunk* chunk; int16_t x, y, z; uint8_t level; };
    std::vector<RNode> removal;
    std::vector<LightNode> readd;

    RNode first;
    first.chunk = start;
    first.x = (int16_t)LocalCoord(wx); first.y = (int16_t)wy; first.z = (int16_t)LocalCoord(wz);
    first.level = (uint8_t)level;
    removal.push_back(first);

    start->SetSkyLight(first.x, wy, first.z, 0);
    TouchChunk(start);

    size_t head = 0;
    while (head < removal.size())
    {
        const RNode node = removal[head++];

        for (int d = 0; d < 6; ++d)
        {
            int nx = node.x + kDir[d][0];
            int ny = node.y + kDir[d][1];
            int nz = node.z + kDir[d][2];
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            Chunk* nc = Neighbour(node.chunk, nx, nz);
            if (!nc) continue;

            const int nl = nc->GetSkyLight(nx, ny, nz);
            if (nl == 0) continue;

            // Une colonne de plein jour se propage vers le bas sans perte:
            // toute la colonne sous le bloc pose doit donc etre effacee.
            const bool column = (d == 1 && node.level == MAX_LIGHT && nl == MAX_LIGHT);

            if (column || nl < node.level)
            {
                nc->SetSkyLight(nx, ny, nz, 0);
                TouchChunk(nc);

                RNode next;
                next.chunk = nc;
                next.x = (int16_t)nx; next.y = (int16_t)ny; next.z = (int16_t)nz;
                next.level = (uint8_t)nl;
                removal.push_back(next);
            }
            else
            {
                LightNode seed;
                seed.chunk = nc;
                seed.x = (int16_t)nx; seed.y = (int16_t)ny; seed.z = (int16_t)nz;
                seed.level = (uint8_t)nl;
                readd.push_back(seed);
            }
        }
    }

    PropagateSky(readd);
}

void World::UpdateLightAt(int wx, int wy, int wz, BlockType oldType, BlockType newType)
{
    Chunk* c = GetChunk(ChunkCoord(wx), ChunkCoord(wz));
    if (!c) return;

    const int lx = LocalCoord(wx), lz = LocalCoord(wz);
    TouchChunk(c);

    // ---- lumiere des blocs ----
    const int previous = c->GetBlockLight(lx, wy, lz);
    if (previous > 0)
        RemoveBlockLight(wx, wy, wz, previous);

    {
        std::vector<LightNode> add;
        const unsigned char emit = Blocks::Emission(newType);
        if (emit > 0)
        {
            c->SetBlockLight(lx, wy, lz, emit);
            LightNode nd; nd.chunk = c;
            nd.x = (int16_t)lx; nd.y = (int16_t)wy; nd.z = (int16_t)lz;
            nd.level = emit;
            add.push_back(nd);
        }

        // Le bloc a peut-etre libere le passage: les voisins peuvent
        // maintenant eclairer la cellule.
        for (int d = 0; d < 6; ++d)
        {
            int nx = lx + kDir[d][0];
            int ny = wy + kDir[d][1];
            int nz = lz + kDir[d][2];
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            Chunk* nc = Neighbour(c, nx, nz);
            if (!nc) continue;

            const unsigned char l = nc->GetBlockLight(nx, ny, nz);
            if (l > 1)
            {
                LightNode nd; nd.chunk = nc;
                nd.x = (int16_t)nx; nd.y = (int16_t)ny; nd.z = (int16_t)nz;
                nd.level = l;
                add.push_back(nd);
            }
        }

        PropagateBlock(add);
    }

    // ---- lumiere du ciel ----
    const int previousSky = c->GetSkyLight(lx, wy, lz);
    if (previousSky > 0)
        RemoveSkyLight(wx, wy, wz, previousSky);

    {
        std::vector<LightNode> add;

        // Ciel ouvert juste au-dessus ?
        const int above = (wy + 1 < CHUNK_SIZE_Y) ? c->GetSkyLight(lx, wy + 1, lz) : MAX_LIGHT;
        const int opacity = Blocks::Opacity(newType);

        if (opacity < MAX_LIGHT)
        {
            int level = above - (opacity > 0 ? opacity : 0);
            if (above == MAX_LIGHT && opacity == 0)
                level = MAX_LIGHT;
            if (level > 0)
            {
                c->SetSkyLight(lx, wy, lz, (unsigned char)level);
                LightNode nd; nd.chunk = c;
                nd.x = (int16_t)lx; nd.y = (int16_t)wy; nd.z = (int16_t)lz;
                nd.level = (uint8_t)level;
                add.push_back(nd);
            }
        }

        for (int d = 0; d < 6; ++d)
        {
            int nx = lx + kDir[d][0];
            int ny = wy + kDir[d][1];
            int nz = lz + kDir[d][2];
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            Chunk* nc = Neighbour(c, nx, nz);
            if (!nc) continue;

            const unsigned char l = nc->GetSkyLight(nx, ny, nz);
            if (l > 1)
            {
                LightNode nd; nd.chunk = nc;
                nd.x = (int16_t)nx; nd.y = (int16_t)ny; nd.z = (int16_t)nz;
                nd.level = l;
                add.push_back(nd);
            }
        }

        PropagateSky(add);
    }
}

void World::MarkDirty(int cx, int cz)
{
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
        {
            Chunk* c = GetChunk(cx + dx, cz + dz);
            if (c) c->meshDirty = true;
        }
}

void World::MarkDirtyAround(int wx, int wz)
{
    MarkDirty(ChunkCoord(wx), ChunkCoord(wz));
}

// ---------------------------------------------------------------------------
//  Streaming
// ---------------------------------------------------------------------------
void World::SetRenderDistance(int d)
{
    if (d < MIN_RENDER_DISTANCE) d = MIN_RENDER_DISTANCE;
    if (d > MAX_RENDER_DISTANCE) d = MAX_RENDER_DISTANCE;
    m_renderDistance = d;
    m_firstUpdate = true;   // force une reevaluation complete
}

void World::QueueMissingChunks(int pcx, int pcz)
{
    // On demande les chunks du plus proche au plus loin: le joueur voit donc
    // son environnement immediat se remplir en premier.
    // La file d'attente est bornee. Quand elle est pleine, balayer le disque
    // de chunks ne sert a rien: on jetterait le resultat. C'est pourtant le
    // cas le plus frequent pendant l'exploration.
    int room;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        room = MAX_QUEUED_GEN - (int)m_genQueue.size();
    }
    if (room <= 0)
        return;

    const int r = m_renderDistance + 1;

    std::vector<GenJob> wanted;
    wanted.reserve((size_t)(2 * r + 1) * (2 * r + 1));

    for (int dz = -r; dz <= r; ++dz)
        for (int dx = -r; dx <= r; ++dx)
        {
            if (dx * dx + dz * dz > r * r)
                continue;

            const int cx = pcx + dx, cz = pcz + dz;
            const Key k = MakeKey(cx, cz);

            if (m_chunks.count(k) || m_queuedGen.count(k))
                continue;

            GenJob j; j.cx = cx; j.cz = cz;
            wanted.push_back(j);
        }

    if (wanted.empty())
        return;

    // Seuls les 'room' plus proches seront empiles: trier le reste ne sert
    // a rien, un tri partiel suffit.
    const int px = pcx, pz = pcz;
    const size_t keep = std::min((size_t)room, wanted.size());
    std::partial_sort(wanted.begin(), wanted.begin() + keep, wanted.end(),
                      [px, pz](const GenJob& a, const GenJob& b) {
        const int da = (a.cx - px) * (a.cx - px) + (a.cz - pz) * (a.cz - pz);
        const int db = (b.cx - px) * (b.cx - px) + (b.cz - pz) * (b.cz - pz);
        return da < db;
    });

    std::lock_guard<std::mutex> lock(m_queueMutex);

    for (size_t i = 0; i < keep; ++i)
    {
        m_genQueue.push_back(wanted[i]);
        m_queuedGen[MakeKey(wanted[i].cx, wanted[i].cz)] = 1;
    }

    m_queueCv.notify_all();
}

void World::CollectFinishedChunks()
{
    std::vector<GenResult> ready;
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        if (m_genResults.empty())
            return;

        const size_t n = std::min<size_t>(m_genResults.size(), MAX_INSERT_PER_FRAME);
        ready.assign(m_genResults.begin(), m_genResults.begin() + n);
        m_genResults.erase(m_genResults.begin(), m_genResults.begin() + n);
    }

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    size_t done = 0;

    for (size_t i = 0; i < ready.size(); ++i)
    {
        // Au-dela du budget, le reste repart dans la file: il sera insere a
        // la frame suivante plutot que de creuser un trou dans celle-ci.
        if (done > 0)
        {
            const double ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - start).count();
            if (ms >= INSERT_BUDGET_MS)
            {
                std::lock_guard<std::mutex> lock(m_resultMutex);
                m_genResults.insert(m_genResults.begin(), ready.begin() + i, ready.end());
                return;
            }
        }
        ++done;

        Chunk* c = ready[i].chunk;
        const Key k = MakeKey(c->Cx(), c->Cz());

        m_queuedGen.erase(k);

        if (m_chunks.count(k))
        {
            delete c;           // deja charge entre-temps
            continue;
        }

        // Restauration des modifications du joueur
        std::unordered_map<Key, SavedChunk>::const_iterator sit = m_savedBlocks.find(k);
        if (sit != m_savedBlocks.end() && sit->second.blocks.size() == c->Blocks().size())
        {
            c->Blocks() = sit->second.blocks;
            if (sit->second.meta.size() == c->Meta().size())
                c->Meta() = sit->second.meta;
            c->RecomputeHighest();
            c->modified = true;
        }

        m_chunks[k] = c;
        InitialLight(*c);
        ScanChunkForUpdates(*c);
    }
}

void World::UnloadFarChunks(int pcx, int pcz)
{
    const int limit = m_renderDistance + UNLOAD_MARGIN;
    const int limitSq = limit * limit;

    for (std::unordered_map<Key, Chunk*>::iterator it = m_chunks.begin(); it != m_chunks.end(); )
    {
        Chunk* c = it->second;
        const int dx = c->Cx() - pcx, dz = c->Cz() - pcz;

        if (dx * dx + dz * dz > limitSq)
        {
            if (c->modified)
            {
                SavedChunk& sc = m_savedBlocks[it->first];
                sc.blocks = c->Blocks();
                sc.meta = c->Meta();
            }

            c->FreeGpuResources();
            delete c;
            it = m_chunks.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool World::NeighboursReady(int cx, int cz) const
{
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const Chunk* c = GetChunk(cx + dx, cz + dz);
            if (!c || !c->generated || !c->lit)
                return false;
        }
    return true;
}

void World::FillSnapshot(int cx, int cz, MeshSnapshot& snap) const
{
    snap.cx = cx;
    snap.cz = cz;

    // Les neuf voisins recouvrent tout le volume rembourre sauf les deux
    // plans horizontaux extremes, remplis plus bas. Quand ils sont tous la
    // -- ce que ScheduleMeshJobs verifie avant d'appeler -- la remise a zero
    // est inutile: c'est 150 Ko ecrits pour rien par chunk.
    bool allNeighbours = true;
    for (int nz = -1; nz <= 1 && allNeighbours; ++nz)
        for (int nx = -1; nx <= 1 && allNeighbours; ++nx)
            if (!GetChunk(cx + nx, cz + nz))
                allNeighbours = false;

    if (!allNeighbours)
    {
        std::fill(snap.blocks.begin(), snap.blocks.end(), (BlockType)BTYPE_AIR);
        std::fill(snap.light.begin(), snap.light.end(), (unsigned char)0);
        std::fill(snap.meta.begin(), snap.meta.end(), (unsigned char)0);
    }

    for (int nz = -1; nz <= 1; ++nz)
    {
        for (int nx = -1; nx <= 1; ++nx)
        {
            const Chunk* c = GetChunk(cx + nx, cz + nz);
            if (!c) continue;

            // Portion du volume rembourre couverte par ce voisin
            const int px0 = (nx == -1) ? -1 : (nx == 0 ? 0 : CHUNK_SIZE_X);
            const int px1 = (nx == -1) ? -1 : (nx == 0 ? CHUNK_SIZE_X - 1 : CHUNK_SIZE_X);
            const int pz0 = (nz == -1) ? -1 : (nz == 0 ? 0 : CHUNK_SIZE_Z);
            const int pz1 = (nz == -1) ? -1 : (nz == 0 ? CHUNK_SIZE_Z - 1 : CHUNK_SIZE_Z);

            // Sur l'axe X les deux tableaux sont contigus: pour le chunk
            // central on recopie 16 blocs d'un coup, ce qui divise par
            // plusieurs le cout de la preparation du maillage.
            const int runLength = px1 - px0 + 1;
            const BlockType* srcBlocks = c->Blocks().data();
            const unsigned char* srcLight = c->Light().data();
            const unsigned char* srcMeta = c->Meta().data();
            BlockType* dstBlocks = snap.blocks.data();
            unsigned char* dstLight = snap.light.data();
            unsigned char* dstMeta = snap.meta.data();

            // Les bordures ne font qu'un bloc de large: trois appels a memcpy
            // pour un octet chacun coutent bien plus cher que l'affectation.
            const int lx = px0 - nx * CHUNK_SIZE_X;

            if (runLength == 1)
            {
                for (int y = 0; y < CHUNK_SIZE_Y; ++y)
                    for (int pz = pz0; pz <= pz1; ++pz)
                    {
                        const int dst = MeshSnapshot::Index(px0, y, pz);
                        const int src = Chunk::Index(lx, y, pz - nz * CHUNK_SIZE_Z);

                        dstBlocks[dst] = srcBlocks[src];
                        dstLight[dst]  = srcLight[src];
                        dstMeta[dst]   = srcMeta[src];
                    }
            }
            else
            {
                for (int y = 0; y < CHUNK_SIZE_Y; ++y)
                    for (int pz = pz0; pz <= pz1; ++pz)
                    {
                        const int dst = MeshSnapshot::Index(px0, y, pz);
                        const int src = Chunk::Index(lx, y, pz - nz * CHUNK_SIZE_Z);

                        memcpy(dstBlocks + dst, srcBlocks + src, (size_t)runLength * sizeof(BlockType));
                        memcpy(dstLight + dst, srcLight + src, (size_t)runLength);
                        memcpy(dstMeta + dst, srcMeta + src, (size_t)runLength);
                    }
            }
        }
    }

    // Bouchons haut et bas: ciel en haut, roche opaque en bas
    for (int pz = -1; pz <= CHUNK_SIZE_Z; ++pz)
        for (int px = -1; px <= CHUNK_SIZE_X; ++px)
        {
            const int top = MeshSnapshot::Index(px, CHUNK_SIZE_Y, pz);
            const int bot = MeshSnapshot::Index(px, -1, pz);

            snap.blocks[top] = BTYPE_AIR;
            snap.light[top]  = (unsigned char)(MAX_LIGHT << 4);
            snap.meta[top]   = 0;
            snap.blocks[bot] = BTYPE_BEDROCK;
            snap.light[bot]  = 0;
            snap.meta[bot]   = 0;
        }
}

void World::ScheduleMeshJobs(int pcx, int pcz)
{
    struct Candidate { Chunk* c; int dist; };
    std::vector<Candidate> candidates;

    const int r = m_renderDistance;
    for (std::unordered_map<Key, Chunk*>::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
    {
        Chunk* c = it->second;
        if (!c->meshDirty || c->meshPending || !c->generated)
            continue;

        const int dx = c->Cx() - pcx, dz = c->Cz() - pcz;
        const int d = dx * dx + dz * dz;
        if (d > (r + 1) * (r + 1))
            continue;

        if (!NeighboursReady(c->Cx(), c->Cz()))
            continue;

        Candidate cand; cand.c = c; cand.dist = d;
        candidates.push_back(cand);
    }

    if (candidates.empty())
        return;

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });

    const int n = std::min((int)candidates.size(), MAX_MESH_JOBS_PER_FRAME);

    std::vector<MeshJob*> jobs;
    jobs.reserve(n);

    for (int i = 0; i < n; ++i)
    {
        Chunk* c = candidates[i].c;
        MeshJob* job = new MeshJob();
        job->cx = c->Cx();
        job->cz = c->Cz();
        FillSnapshot(job->cx, job->cz, job->snap);

        c->meshDirty = false;
        c->meshPending = true;
        jobs.push_back(job);
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (size_t i = 0; i < jobs.size(); ++i)
            m_meshQueue.push_back(jobs[i]);
    }
    m_queueCv.notify_all();
}

void World::UploadFinishedMeshes(int maxUploads)
{
    std::vector<MeshResult*> ready;
    {
        std::lock_guard<std::mutex> lock(m_resultMutex);
        if (m_meshResults.empty())
            return;

        const size_t n = std::min<size_t>(m_meshResults.size(), (size_t)maxUploads);
        ready.assign(m_meshResults.begin(), m_meshResults.begin() + n);
        m_meshResults.erase(m_meshResults.begin(), m_meshResults.begin() + n);
    }

    for (size_t i = 0; i < ready.size(); ++i)
    {
        MeshResult* res = ready[i];
        Chunk* c = GetChunk(res->cx, res->cz);
        if (c)
        {
            c->UploadMesh(res->data);
            c->meshPending = false;
        }
        delete res;
    }
}

void World::Update(const Vector3f& playerPos, int maxUploadsPerFrame)
{
    const int pcx = ChunkCoord((int)floorf(playerPos.x));
    const int pcz = ChunkCoord((int)floorf(playerPos.z));

    if (m_firstUpdate || pcx != m_playerCx || pcz != m_playerCz)
    {
        m_playerCx = pcx;
        m_playerCz = pcz;
        m_firstUpdate = false;
        UnloadFarChunks(pcx, pcz);
    }

    QueueMissingChunks(pcx, pcz);
    CollectFinishedChunks();
    ScheduleMeshJobs(pcx, pcz);
    UploadFinishedMeshes(maxUploadsPerFrame);
}

void World::InvalidateAllMeshes()
{
    for (std::unordered_map<Key, Chunk*>::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
    {
        it->second->FreeGpuResources();
        it->second->meshDirty = true;
        it->second->meshPending = false;
    }
}

// ---------------------------------------------------------------------------
//  Visibilite
// ---------------------------------------------------------------------------
void World::BuildVisibleList(const Vector3f& camPos, const float frustum[6][4])
{
    m_visible.clear();
    if (m_visible.capacity() < m_chunks.size())
        m_visible.reserve(m_chunks.size());
    m_drawnVertices = 0;

    for (std::unordered_map<Key, Chunk*>::iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
    {
        Chunk* c = it->second;
        if (!c->HasSolid() && !c->HasBlend())
            continue;

        const float x0 = (float)(c->Cx() * CHUNK_SIZE_X);
        const float z0 = (float)(c->Cz() * CHUNK_SIZE_Z);
        const float x1 = x0 + CHUNK_SIZE_X;
        const float z1 = z0 + CHUNK_SIZE_Z;
        const float y0 = (float)c->MinY();
        const float y1 = (float)c->MaxY();

        bool inside = true;
        for (int p = 0; p < 6 && inside; ++p)
        {
            // Coin le plus favorable du pave: s'il est derriere le plan, tout
            // le chunk l'est aussi.
            const float px = (frustum[p][0] >= 0.0f) ? x1 : x0;
            const float py = (frustum[p][1] >= 0.0f) ? y1 : y0;
            const float pz = (frustum[p][2] >= 0.0f) ? z1 : z0;

            if (frustum[p][0] * px + frustum[p][1] * py + frustum[p][2] * pz + frustum[p][3] < 0.0f)
                inside = false;
        }

        if (!inside)
            continue;

        m_visible.push_back(c);
        m_drawnVertices += c->VertexCount();
    }

    const float cx = camPos.x, cy = camPos.y, cz = camPos.z;
    std::sort(m_visible.begin(), m_visible.end(), [cx, cy, cz](const Chunk* a, const Chunk* b) {
        const float ax = (float)(a->Cx() * CHUNK_SIZE_X + 8) - cx;
        const float az = (float)(a->Cz() * CHUNK_SIZE_Z + 8) - cz;
        const float bx = (float)(b->Cx() * CHUNK_SIZE_X + 8) - cx;
        const float bz = (float)(b->Cz() * CHUNK_SIZE_Z + 8) - cz;
        return (ax * ax + az * az) < (bx * bx + bz * bz);
    });
}

// ---------------------------------------------------------------------------
//  Lancer de rayon (DDA voxel, Amanatides & Woo)
// ---------------------------------------------------------------------------
bool World::Raycast(const Vector3f& origin, const Vector3f& dir, float maxDist,
                    Vector3i& hitBlock, Vector3i& hitNormal) const
{
    Vector3f d = dir;
    d.Normalize();

    int x = (int)floorf(origin.x);
    int y = (int)floorf(origin.y);
    int z = (int)floorf(origin.z);

    const int stepX = (d.x > 0) ? 1 : (d.x < 0 ? -1 : 0);
    const int stepY = (d.y > 0) ? 1 : (d.y < 0 ? -1 : 0);
    const int stepZ = (d.z > 0) ? 1 : (d.z < 0 ? -1 : 0);

    const float big = 1e30f;
    const float tDeltaX = (stepX != 0) ? fabsf(1.0f / d.x) : big;
    const float tDeltaY = (stepY != 0) ? fabsf(1.0f / d.y) : big;
    const float tDeltaZ = (stepZ != 0) ? fabsf(1.0f / d.z) : big;

    float tMaxX = big, tMaxY = big, tMaxZ = big;
    if (stepX > 0) tMaxX = ((float)(x + 1) - origin.x) / d.x;
    else if (stepX < 0) tMaxX = ((float)x - origin.x) / d.x;
    if (stepY > 0) tMaxY = ((float)(y + 1) - origin.y) / d.y;
    else if (stepY < 0) tMaxY = ((float)y - origin.y) / d.y;
    if (stepZ > 0) tMaxZ = ((float)(z + 1) - origin.z) / d.z;
    else if (stepZ < 0) tMaxZ = ((float)z - origin.z) / d.z;

    Vector3i normal(0, 0, 0);
    float t = 0.0f;

    while (t <= maxDist)
    {
        const BlockType bt = GetBlock(x, y, z);
        if (bt != BTYPE_AIR && !Blocks::IsLiquid(bt))
        {
            hitBlock = Vector3i(x, y, z);
            hitNormal = normal;
            return true;
        }

        if (tMaxX < tMaxY && tMaxX < tMaxZ)
        {
            x += stepX; t = tMaxX; tMaxX += tDeltaX;
            normal = Vector3i(-stepX, 0, 0);
        }
        else if (tMaxY < tMaxZ)
        {
            y += stepY; t = tMaxY; tMaxY += tDeltaY;
            normal = Vector3i(0, -stepY, 0);
        }
        else
        {
            z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ;
            normal = Vector3i(0, 0, -stepZ);
        }

        if (y < 0 || y >= CHUNK_SIZE_Y)
            return false;
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Sauvegarde / chargement
// ---------------------------------------------------------------------------
namespace
{
    const char kMagic[8] = { 'B', 'L', 'K', 'A', 'D', 'V', '0', '3' };

    // Compression par plages: un tableau de blocs est fait de longues suites
    // identiques, il se reduit donc enormement.
    void WriteRle(FILE* f, const std::vector<unsigned char>& data)
    {
        std::vector<unsigned char> runs;
        runs.reserve(4096);

        size_t i = 0;
        uint32_t runCount = 0;
        while (i < data.size())
        {
            const unsigned char v = data[i];
            size_t j = i;
            while (j < data.size() && data[j] == v && (j - i) < 65535) ++j;

            const uint16_t len = (uint16_t)(j - i);
            runs.push_back((unsigned char)(len & 0xff));
            runs.push_back((unsigned char)(len >> 8));
            runs.push_back(v);
            ++runCount;
            i = j;
        }

        fwrite(&runCount, sizeof(runCount), 1, f);
        fwrite(runs.data(), 1, runs.size(), f);
    }

    bool ReadRle(FILE* f, std::vector<unsigned char>& out, size_t expected)
    {
        uint32_t runCount = 0;
        if (fread(&runCount, sizeof(runCount), 1, f) != 1)
            return false;

        out.clear();
        out.reserve(expected);

        for (uint32_t r = 0; r < runCount; ++r)
        {
            unsigned char lo, hi, v;
            if (fread(&lo, 1, 1, f) != 1 || fread(&hi, 1, 1, f) != 1 || fread(&v, 1, 1, f) != 1)
                return false;

            const uint16_t len = (uint16_t)(lo | (hi << 8));
            for (uint16_t k = 0; k < len && out.size() < expected; ++k)
                out.push_back(v);
        }

        return out.size() == expected;
    }
}

bool World::Save(const std::string& filename) const
{
    // On rassemble d'abord les chunks modifies encore en memoire
    std::unordered_map<Key, SavedChunk> all = m_savedBlocks;
    for (std::unordered_map<Key, Chunk*>::const_iterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
        if (it->second->modified)
        {
            SavedChunk& sc = all[it->first];
            sc.blocks = it->second->Blocks();
            sc.meta = it->second->Meta();
        }

    FILE* f = fopen(filename.c_str(), "wb");
    if (!f)
        return false;

    fwrite(kMagic, 1, 8, f);
    uint32_t seed = m_gen.Seed();
    fwrite(&seed, sizeof(seed), 1, f);
    uint32_t count = (uint32_t)all.size();
    fwrite(&count, sizeof(count), 1, f);

    for (std::unordered_map<Key, SavedChunk>::const_iterator it = all.begin(); it != all.end(); ++it)
    {
        const int32_t cx = (int32_t)(it->first >> 32);
        const int32_t cz = (int32_t)(it->first & 0xffffffffu);
        fwrite(&cx, sizeof(cx), 1, f);
        fwrite(&cz, sizeof(cz), 1, f);

        // Les blocs sont des uint8_t: on peut les ecrire tels quels
        WriteRle(f, *reinterpret_cast<const std::vector<unsigned char>*>(&it->second.blocks));
        WriteRle(f, it->second.meta);
    }

    fclose(f);
    return true;
}

bool World::Load(const std::string& filename)
{
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f)
        return false;

    char magic[8];
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kMagic, 8) != 0)
    {
        std::cerr << "[World] Sauvegarde d'un format different, ignoree" << std::endl;
        fclose(f);
        return false;
    }

    uint32_t seed = 0, count = 0;
    if (fread(&seed, sizeof(seed), 1, f) != 1 ||
        fread(&count, sizeof(count), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    const size_t volume = (size_t)CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

    for (uint32_t c = 0; c < count; ++c)
    {
        int32_t cx = 0, cz = 0;
        if (fread(&cx, sizeof(cx), 1, f) != 1) break;
        if (fread(&cz, sizeof(cz), 1, f) != 1) break;

        std::vector<unsigned char> blocks, meta;
        if (!ReadRle(f, blocks, volume)) break;
        if (!ReadRle(f, meta, volume)) break;

        SavedChunk& sc = m_savedBlocks[MakeKey(cx, cz)];
        sc.blocks.assign(blocks.begin(), blocks.end());
        sc.meta.swap(meta);
    }

    fclose(f);
    std::cout << "[World] " << m_savedBlocks.size() << " chunks modifies charges" << std::endl;
    return true;
}
