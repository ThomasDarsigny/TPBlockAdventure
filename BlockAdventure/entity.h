#ifndef ENTITY_H__
#define ENTITY_H__

#include "define.h"
#include "vector3.h"
#include <vector>
#include <string>

class World;

enum EntityType
{
    ENT_PIG = 0,
    ENT_COW,
    ENT_ZOMBIE,
    ENT_REMOTE_PLAYER,     // autre joueur, en reseau
    ENT_COUNT
};

// ---------------------------------------------------------------------------
//  Description figee d'une espece: gabarit, comportement et couleurs des
//  differentes parties du modele (le modele lui-meme est un assemblage de
//  boites, comme dans Minecraft).
// ---------------------------------------------------------------------------
struct EntityDef
{
    const char* name;
    float width;
    float height;
    float maxHealth;
    float speed;
    bool  hostile;
    float attackDamage;
    BlockType drop;
    int   dropCount;

    float bodyColor[3];
    float headColor[3];
    float legColor[3];
};

const EntityDef& EntityInfo(EntityType type);

// ---------------------------------------------------------------------------
//  Une creature
// ---------------------------------------------------------------------------
struct Entity
{
    int         id;
    EntityType  type;
    Vector3f    position;      // pieds, au centre de la boite
    Vector3f    velocity;
    float       yaw;           // degres, 0 = vers -Z
    float       health;
    bool        onGround;
    bool        dead;

    float       wanderTimer;   // temps restant avant de changer de cap
    float       wanderYaw;
    bool        wanderMoving;
    float       attackCooldown;
    float       hurtFlash;
    float       walkPhase;     // animation des pattes
    float       despawnTimer;

    Entity();
};

// ---------------------------------------------------------------------------
//  Gestion de l'ensemble des creatures
// ---------------------------------------------------------------------------
class EntityManager
{
public:
    EntityManager();

    void Clear() { m_entities.clear(); }

    // simulation: deplacement, IA, apparition et disparition
    void Update(World& world, const Vector3f& playerPos, float& playerDamage,
                float dayFactor, float elapsed, bool spawning);

    // dessin (pipeline fixe, apres la passe opaque du terrain)
    void Render(const World& world, float dayFactor) const;

    Entity* Spawn(EntityType type, const Vector3f& position);

    // Creature visee par le rayon du joueur, ou 0
    Entity* Pick(const Vector3f& origin, const Vector3f& dir, float maxDist, float& outDist);

    // Retourne true si la creature meurt
    bool Damage(Entity& e, float amount, BlockType& drop, int& dropCount);

    const std::vector<Entity>& All() const { return m_entities; }
    std::vector<Entity>&       All()       { return m_entities; }

    int Count() const { return (int)m_entities.size(); }
    int CountOf(bool hostile) const;

private:
    void UpdateOne(World& world, Entity& e, const Vector3f& playerPos,
                   float& playerDamage, float dayFactor, float elapsed);
    void TrySpawn(World& world, const Vector3f& playerPos, float dayFactor);

    static bool Collides(const World& world, const Vector3f& p, float w, float h);
    static float MoveAxis(const World& world, Entity& e, int axis, float delta);

private:
    std::vector<Entity> m_entities;
    int   m_nextId;
    float m_spawnTimer;
    uint32_t m_rng;

    float Random();
};

#endif // ENTITY_H__
