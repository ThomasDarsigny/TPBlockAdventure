#ifndef WORLD_H__
#define WORLD_H__

#include "define.h"
#include "chunk.h"
#include "chunkmesher.h"
#include "worldgen.h"
#include "vector3.h"

#include <unordered_map>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_set>

// ---------------------------------------------------------------------------
//  Le monde: une carte de chunks charges dynamiquement autour du joueur.
//
//  Repartition du travail:
//    - threads secondaires : generation du terrain et construction des
//      maillages (aucun appel OpenGL)
//    - thread principal    : insertion des chunks, propagation de la lumiere,
//      envoi des VBO au GPU
//
//  Les chunks trop loin sont dechargees; s'ils ont ete modifies par le joueur
//  leur contenu est conserve en memoire (et sur disque) pour etre restaure a
//  l'identique au retour.
// ---------------------------------------------------------------------------
class World
{
public:

    explicit World(uint32_t seed);
    ~World();

    // ---- cycle de vie ----
    void Update(const Vector3f& playerPos, int maxUploadsPerFrame = 4);
    void Shutdown();

    // ---- acces aux chunks ----
    Chunk* GetChunk(int cx, int cz);
    const Chunk* GetChunk(int cx, int cz) const;

    static int ChunkCoord(int world) { return (world >= 0) ? (world / CHUNK_SIZE_X) : ((world + 1) / CHUNK_SIZE_X - 1); }
    static int LocalCoord(int world) { int m = world % CHUNK_SIZE_X; return (m < 0) ? m + CHUNK_SIZE_X : m; }

    // ---- acces aux blocs (coordonnees monde) ----
    BlockType GetBlock(int wx, int wy, int wz) const;
    unsigned char GetMeta(int wx, int wy, int wz) const;

    // Pose d'un bloc par le joueur: recalcule la lumiere immediatement et
    // reveille les blocs voisins (ecoulement, chute...).
    bool SetBlock(int wx, int wy, int wz, BlockType t, unsigned char meta = 0);

    // ---- simulation des blocs (fluides, sable qui tombe) ----
    void Tick(float elapsed);
    void ScheduleUpdate(int wx, int wy, int wz);
    void ScheduleNeighbours(int wx, int wy, int wz);
    int  PendingBlockUpdates() const { return (int)m_updates.size(); }

    unsigned char GetSkyLight(int wx, int wy, int wz) const;
    unsigned char GetBlockLight(int wx, int wy, int wz) const;

    bool IsSolid(int wx, int wy, int wz) const;
    bool IsLiquidAt(const Vector3f& p) const;

    // ---- selection ----
    // Lance un rayon et retourne le premier bloc touche (algorithme DDA).
    bool Raycast(const Vector3f& origin, const Vector3f& dir, float maxDist,
                 Vector3i& hitBlock, Vector3i& hitNormal) const;

    // ---- rendu ----
    // Chunks pretes a etre dessinees, triees du plus proche au plus loin.
    const std::vector<Chunk*>& VisibleChunks() const { return m_visible; }
    void BuildVisibleList(const Vector3f& camPos, const float frustum[6][4]);

    // ---- reglages / statistiques ----
    void SetRenderDistance(int d);
    int  RenderDistance() const { return m_renderDistance; }

    int  LoadedChunks() const { return (int)m_chunks.size(); }
    int  PendingJobs() const;
    int  DrawnChunks() const { return (int)m_visible.size(); }
    int  DrawnVertices() const { return m_drawnVertices; }

    const WorldGenerator& Generator() const { return m_gen; }
    uint32_t Seed() const { return m_gen.Seed(); }

    // ---- persistance ----
    bool Save(const std::string& filename) const;
    bool Load(const std::string& filename);
    int  ModifiedChunkCount() const { return (int)m_savedBlocks.size(); }

    // Recharge tout (utilise apres une recreation de contexte OpenGL)
    void InvalidateAllMeshes();

private:
    typedef uint64_t Key;
    static Key MakeKey(int cx, int cz)
    {
        return ((uint64_t)(uint32_t)cx << 32) | (uint64_t)(uint32_t)cz;
    }

    struct GenJob   { int cx, cz; };
    struct GenResult{ Chunk* chunk; };
    struct MeshJob  { int cx, cz; MeshSnapshot snap; };
    struct MeshResult { int cx, cz; ChunkMeshData data; };

    // ---- threads ----
    void StartWorkers();
    void WorkerLoop();

    // ---- streaming ----
    void QueueMissingChunks(int pcx, int pcz);
    void CollectFinishedChunks();
    void UnloadFarChunks(int pcx, int pcz);
    void ScheduleMeshJobs(int pcx, int pcz);
    void UploadFinishedMeshes(int maxUploads);

    bool NeighboursReady(int cx, int cz) const;
    void FillSnapshot(int cx, int cz, MeshSnapshot& snap) const;

    // ---- lumiere ----
    struct LightNode
    {
        Chunk* chunk;
        int16_t x, y, z;
        uint8_t level;
    };

    int  NeighbourHighest(const Chunk& chunk, int lx, int lz) const;
    void InitialLight(Chunk& chunk);
    void SeedFromNeighbours(Chunk& chunk, std::vector<LightNode>& sky, std::vector<LightNode>& blk);
    void PropagateSky(std::vector<LightNode>& queue);
    void PropagateBlock(std::vector<LightNode>& queue);

    // Mise a jour incrementale apres le changement d'un bloc: on retire la
    // lumiere devenue invalide, puis on la repropage depuis les bords de la
    // zone effacee. Seules quelques dizaines de cellules sont touchees, la
    // ou un recalcul de chunk complet en visiterait quarante mille.
    void UpdateLightAt(int wx, int wy, int wz, BlockType oldType, BlockType newType);
    void RemoveSkyLight(int wx, int wy, int wz, int level);
    void RemoveBlockLight(int wx, int wy, int wz, int level);

    void TouchChunk(const Chunk* c);

    void MarkDirty(int cx, int cz);
    void MarkDirtyAround(int wx, int wz);

    // Marque comme a remailler toutes les chunks dont la lumiere a bouge
    void FlushDirtyChunks();

    // ---- simulation ----
    // Ecriture "silencieuse": pas de recalcul de lumiere immediat, la chunk
    // est simplement notee comme a rallumer en fin de tick.
    bool SetBlockQuiet(int wx, int wy, int wz, BlockType t, unsigned char meta);
    void UpdateBlock(int wx, int wy, int wz);
    void UpdateFluid(int wx, int wy, int wz, BlockType t);
    void UpdateFalling(int wx, int wy, int wz, BlockType t);
    void ScanChunkForUpdates(const Chunk& chunk);

    static uint64_t PosKey(int x, int y, int z)
    {
        return (((uint64_t)(uint32_t)(x + 0x800000) & 0xFFFFFFull) << 40)
             | (((uint64_t)(uint32_t)(z + 0x800000) & 0xFFFFFFull) << 16)
             | ((uint64_t)(uint32_t)y & 0xFFFFull);
    }

    // Acces rapide pour la propagation: reutilise le chunk courant tant que
    // les coordonnees locales restent dedans.
    Chunk* Neighbour(Chunk* from, int& lx, int& lz);

private:
    WorldGenerator m_gen;

    std::unordered_map<Key, Chunk*> m_chunks;
    std::vector<Chunk*> m_visible;

    // Blocs modifies par le joueur, conserves meme apres dechargement
    struct SavedChunk
    {
        std::vector<BlockType>     blocks;
        std::vector<unsigned char> meta;
    };
    std::unordered_map<Key, SavedChunk> m_savedBlocks;

    // ---- file de mise a jour des blocs ----
    struct PendingUpdate { int x, y, z; };
    std::vector<PendingUpdate>   m_updates;
    std::unordered_set<uint64_t> m_updateSet;
    std::unordered_set<Key>      m_relightTouched;
    float m_tickAccum;

    int m_renderDistance;
    int m_drawnVertices;
    int m_playerCx, m_playerCz;
    bool m_firstUpdate;

    // ---- pool de threads ----
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running;

    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::deque<GenJob> m_genQueue;
    std::deque<MeshJob*> m_meshQueue;

    mutable std::mutex m_resultMutex;
    std::vector<GenResult> m_genResults;
    std::vector<MeshResult*> m_meshResults;

    std::atomic<int> m_inFlight;

    // Chunks dont un job est deja en cours (evite les doublons)
    std::unordered_map<Key, char> m_queuedGen;
};

#endif // WORLD_H__
