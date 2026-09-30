#include "entity.h"
#include "world.h"
#include "noise.h"
#include <cmath>
#include <algorithm>

namespace
{
    const float GRAVITY       = 28.0f;
    const float JUMP_SPEED    = 7.6f;
    const float MAX_FALL      = 50.0f;

    const int   MAX_PASSIVE   = 14;
    const int   MAX_HOSTILE   = 16;
    const float SPAWN_PERIOD  = 2.0f;
    const float SPAWN_MIN     = 22.0f;
    const float SPAWN_MAX     = 46.0f;
    const float DESPAWN_DIST  = 88.0f;

    const EntityDef kDefs[ENT_COUNT] =
    {
        //  nom        larg  haut  vie  vitesse hostile degats  butin            n   corps                tete                 pattes
        { "Cochon",    0.9f, 0.9f, 10.f, 1.5f,  false,  0.0f,  BTYPE_PORKCHOP,  2, { 0.94f, 0.62f, 0.64f }, { 0.96f, 0.68f, 0.70f }, { 0.86f, 0.52f, 0.54f } },
        { "Vache",     0.9f, 1.4f, 10.f, 1.4f,  false,  0.0f,  BTYPE_BEEF,      2, { 0.30f, 0.24f, 0.20f }, { 0.88f, 0.86f, 0.82f }, { 0.24f, 0.19f, 0.16f } },
        { "Zombie",    0.6f, 1.95f, 20.f, 2.6f, true,   3.0f,  BTYPE_AIR,       0, { 0.24f, 0.42f, 0.64f }, { 0.32f, 0.55f, 0.35f }, { 0.20f, 0.26f, 0.48f } },
        { "Joueur",    0.6f, 1.8f, 20.f, 4.3f,  false,  0.0f,  BTYPE_AIR,       0, { 0.30f, 0.45f, 0.80f }, { 0.85f, 0.68f, 0.55f }, { 0.25f, 0.28f, 0.50f } }
    };

    inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // ------------------------------------------------------------------
    //  Dessin d'une boite pleine, en coordonnees locales de la creature.
    //  Les faces recoivent un ombrage fixe comme les blocs du terrain, ce
    //  qui suffit a donner du relief sans eclairage dynamique.
    // ------------------------------------------------------------------
    void DrawBox(float cx, float cy, float cz, float hx, float hy, float hz,
                 const float rgb[3], float brightness)
    {
        const float x0 = cx - hx, x1 = cx + hx;
        const float y0 = cy - hy, y1 = cy + hy;
        const float z0 = cz - hz, z1 = cz + hz;

        const float top = brightness;
        const float side = brightness * 0.78f;
        const float front = brightness * 0.88f;
        const float bottom = brightness * 0.55f;

        glBegin(GL_QUADS);

        glColor3f(rgb[0] * top, rgb[1] * top, rgb[2] * top);
        glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);

        glColor3f(rgb[0] * bottom, rgb[1] * bottom, rgb[2] * bottom);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);

        glColor3f(rgb[0] * side, rgb[1] * side, rgb[2] * side);
        glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
        glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);

        glColor3f(rgb[0] * front, rgb[1] * front, rgb[2] * front);
        glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
        glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);

        glEnd();
    }

    // Patte articulee autour de la hanche
    void DrawLeg(float hipX, float hipY, float hipZ, float len, float thick,
                 float swingDeg, const float rgb[3], float brightness)
    {
        glPushMatrix();
        glTranslatef(hipX, hipY, hipZ);
        glRotatef(swingDeg, 1.0f, 0.0f, 0.0f);
        DrawBox(0.0f, -len * 0.5f, 0.0f, thick, len * 0.5f, thick, rgb, brightness);
        glPopMatrix();
    }
}

const EntityDef& EntityInfo(EntityType type)
{
    return kDefs[type];
}

Entity::Entity()
    : id(0), type(ENT_PIG), position(0, 0, 0), velocity(0, 0, 0), yaw(0.0f),
      health(10.0f), onGround(false), dead(false),
      wanderTimer(0.0f), wanderYaw(0.0f), wanderMoving(false),
      attackCooldown(0.0f), hurtFlash(0.0f), walkPhase(0.0f), despawnTimer(0.0f)
{
}

EntityManager::EntityManager() : m_nextId(1), m_spawnTimer(0.0f), m_rng(0x1234abcdu)
{
}

float EntityManager::Random()
{
    m_rng = Noise::Hash(m_rng + 0x9e3779b9u);
    return (float)(m_rng & 0xffffff) / (float)0xffffff;
}

int EntityManager::CountOf(bool hostile) const
{
    int n = 0;
    for (size_t i = 0; i < m_entities.size(); ++i)
        if (m_entities[i].type != ENT_REMOTE_PLAYER &&
            kDefs[m_entities[i].type].hostile == hostile)
            ++n;
    return n;
}

Entity* EntityManager::Spawn(EntityType type, const Vector3f& position)
{
    Entity e;
    e.id = m_nextId++;
    e.type = type;
    e.position = position;
    e.health = kDefs[type].maxHealth;
    e.yaw = Random() * 360.0f;
    e.wanderYaw = e.yaw;
    m_entities.push_back(e);
    return &m_entities.back();
}

// ---------------------------------------------------------------------------
//  Collisions (memes regles que le joueur, en plus court)
// ---------------------------------------------------------------------------
bool EntityManager::Collides(const World& world, const Vector3f& p, float w, float h)
{
    const float hw = w * 0.5f - 0.001f;

    const int x0 = (int)floorf(p.x - hw), x1 = (int)floorf(p.x + hw);
    const int z0 = (int)floorf(p.z - hw), z1 = (int)floorf(p.z + hw);
    const int y0 = (int)floorf(p.y + 0.001f), y1 = (int)floorf(p.y + h - 0.001f);

    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (world.IsSolid(x, y, z))
                    return true;

    return false;
}

float EntityManager::MoveAxis(const World& world, Entity& e, int axis, float delta)
{
    if (delta == 0.0f)
        return 0.0f;

    const EntityDef& def = kDefs[e.type];

    Vector3f test = e.position;
    float* comp[3] = { &test.x, &test.y, &test.z };

    *comp[axis] += delta;
    if (!Collides(world, test, def.width, def.height))
    {
        e.position = test;
        return delta;
    }

    float lo = 0.0f, hi = delta;
    for (int i = 0; i < 5; ++i)
    {
        const float mid = (lo + hi) * 0.5f;
        test = e.position;
        *comp[axis] += mid;
        if (Collides(world, test, def.width, def.height)) hi = mid;
        else                                              lo = mid;
    }

    test = e.position;
    *comp[axis] += lo;
    e.position = test;
    return lo;
}

// ---------------------------------------------------------------------------
//  Simulation
// ---------------------------------------------------------------------------
void EntityManager::UpdateOne(World& world, Entity& e, const Vector3f& playerPos,
                              float& playerDamage, float dayFactor, float elapsed)
{
    const EntityDef& def = kDefs[e.type];

    if (e.hurtFlash > 0.0f) e.hurtFlash -= elapsed * 3.0f;
    if (e.attackCooldown > 0.0f) e.attackCooldown -= elapsed;

    // --- decision ---------------------------------------------------------
    float wishX = 0.0f, wishZ = 0.0f;
    bool wantJump = false;

    const Vector3f toPlayer = playerPos - e.position;
    const float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;

    if (def.hostile && distSq < 18.0f * 18.0f)
    {
        // Poursuite: on se tourne vers le joueur et on avance
        e.yaw = atan2f(toPlayer.x, -toPlayer.z) * 57.29578f;

        const float d = sqrtf(distSq);
        if (d > 1.2f)
        {
            wishX = toPlayer.x / d;
            wishZ = toPlayer.z / d;
        }
        else if (e.attackCooldown <= 0.0f)
        {
            playerDamage += def.attackDamage;
            e.attackCooldown = 1.1f;
        }

        // Franchir une marche
        if (toPlayer.y > 0.6f && e.onGround)
            wantJump = true;
    }
    else
    {
        // Deambulation: on change de cap de temps en temps
        e.wanderTimer -= elapsed;
        if (e.wanderTimer <= 0.0f)
        {
            e.wanderTimer = 2.5f + Random() * 5.0f;
            e.wanderMoving = Random() < 0.65f;
            e.wanderYaw = Random() * 360.0f;
        }

        if (e.wanderMoving)
        {
            e.yaw = e.wanderYaw;
            const float r = e.yaw * 0.017453293f;
            wishX = sinf(r);
            wishZ = -cosf(r);
        }
    }

    // --- deplacement ------------------------------------------------------
    const float speed = def.speed;
    const float accel = e.onGround ? 12.0f : 3.0f;

    e.velocity.x += (wishX * speed - e.velocity.x) * Clamp(accel * elapsed, 0.0f, 1.0f);
    e.velocity.z += (wishZ * speed - e.velocity.z) * Clamp(accel * elapsed, 0.0f, 1.0f);

    // Dans l'eau, la creature flotte
    const bool inWater = Blocks::IsLiquid(world.GetBlock((int)floorf(e.position.x),
                                                         (int)floorf(e.position.y + 0.2f),
                                                         (int)floorf(e.position.z)));
    if (inWater)
    {
        e.velocity.y += 9.0f * elapsed;
        e.velocity.y = Clamp(e.velocity.y, -2.0f, 2.5f);
    }
    else
    {
        if (e.onGround && wantJump)
        {
            e.velocity.y = JUMP_SPEED;
            e.onGround = false;
        }
        e.velocity.y -= GRAVITY * elapsed;
        if (e.velocity.y < -MAX_FALL) e.velocity.y = -MAX_FALL;
    }

    const float dx = e.velocity.x * elapsed;
    const float dz = e.velocity.z * elapsed;
    const float dy = e.velocity.y * elapsed;

    const bool blockedX = (MoveAxis(world, e, 0, dx) != dx);
    const bool blockedZ = (MoveAxis(world, e, 2, dz) != dz);

    // Un obstacle bas se franchit d'un saut
    if ((blockedX || blockedZ) && e.onGround)
    {
        e.velocity.y = JUMP_SPEED;
        e.onGround = false;
    }
    if (blockedX) e.velocity.x = 0.0f;
    if (blockedZ) e.velocity.z = 0.0f;

    const float movedY = MoveAxis(world, e, 1, dy);
    if (movedY != dy)
    {
        if (dy < 0.0f) e.onGround = true;
        e.velocity.y = 0.0f;
    }
    else if (dy != 0.0f)
    {
        e.onGround = false;
    }

    // --- animation --------------------------------------------------------
    const float horizontal = sqrtf(e.velocity.x * e.velocity.x + e.velocity.z * e.velocity.z);
    e.walkPhase += horizontal * elapsed * 3.4f;

    // --- le soleil brule les morts-vivants --------------------------------
    if (def.hostile && dayFactor > 0.55f)
    {
        const int bx = (int)floorf(e.position.x);
        const int by = (int)floorf(e.position.y + def.height * 0.5f);
        const int bz = (int)floorf(e.position.z);

        if (world.GetSkyLight(bx, by, bz) >= 14)
        {
            e.health -= elapsed * 3.0f;
            e.hurtFlash = 1.0f;
            if (e.health <= 0.0f) e.dead = true;
        }
    }

    // --- noyade et vide ---------------------------------------------------
    if (e.position.y < -4.0f)
        e.dead = true;

    // --- disparition au loin ----------------------------------------------
    const float dist = sqrtf(distSq);
    if (dist > DESPAWN_DIST)
        e.dead = true;
}

void EntityManager::TrySpawn(World& world, const Vector3f& playerPos, float dayFactor)
{
    const int passive = CountOf(false);
    const int hostile = CountOf(true);

    for (int attempt = 0; attempt < 6; ++attempt)
    {
        const float angle = Random() * 6.2831853f;
        const float dist = SPAWN_MIN + Random() * (SPAWN_MAX - SPAWN_MIN);

        const int wx = (int)(playerPos.x + cosf(angle) * dist);
        const int wz = (int)(playerPos.z + sinf(angle) * dist);

        // On cherche le sol depuis un peu au-dessus du joueur
        int wy = -1;
        const int top = (int)playerPos.y + 24;
        for (int y = (top < CHUNK_SIZE_Y - 1 ? top : CHUNK_SIZE_Y - 2); y > 2; --y)
        {
            if (world.IsSolid(wx, y, wz) &&
                !world.IsSolid(wx, y + 1, wz) &&
                !world.IsSolid(wx, y + 2, wz))
            {
                wy = y + 1;
                break;
            }
        }
        if (wy < 0)
            continue;

        const BlockType ground = world.GetBlock(wx, wy - 1, wz);
        if (Blocks::IsLiquid(ground) || ground == BTYPE_AIR)
            continue;

        const int sky = world.GetSkyLight(wx, wy, wz);
        const int blk = world.GetBlockLight(wx, wy, wz);
        const float lit = (float)sky * dayFactor + (float)blk;

        const Vector3f pos((float)wx + 0.5f, (float)wy, (float)wz + 0.5f);

        if (lit < 7.0f)
        {
            // Endroit sombre: creature hostile
            if (hostile >= MAX_HOSTILE)
                continue;
            Spawn(ENT_ZOMBIE, pos);
            return;
        }

        // Plein jour sur de l'herbe: animaux
        if (passive >= MAX_PASSIVE)
            continue;
        if (ground != BTYPE_GRASS && ground != BTYPE_PODZOL)
            continue;
        if (sky < 9)
            continue;

        Spawn(Random() < 0.5f ? ENT_PIG : ENT_COW, pos);
        return;
    }
}

void EntityManager::Update(World& world, const Vector3f& playerPos, float& playerDamage,
                           float dayFactor, float elapsed, bool spawning)
{
    for (size_t i = 0; i < m_entities.size(); ++i)
        if (m_entities[i].type != ENT_REMOTE_PLAYER)
            UpdateOne(world, m_entities[i], playerPos, playerDamage, dayFactor, elapsed);

    // Retrait des creatures mortes ou trop loin
    for (size_t i = 0; i < m_entities.size(); )
    {
        if (m_entities[i].dead)
        {
            m_entities[i] = m_entities.back();
            m_entities.pop_back();
        }
        else
        {
            ++i;
        }
    }

    if (!spawning)
        return;

    m_spawnTimer += elapsed;
    if (m_spawnTimer >= SPAWN_PERIOD)
    {
        m_spawnTimer = 0.0f;
        TrySpawn(world, playerPos, dayFactor);
    }
}

// ---------------------------------------------------------------------------
//  Combat
// ---------------------------------------------------------------------------
Entity* EntityManager::Pick(const Vector3f& origin, const Vector3f& dir, float maxDist, float& outDist)
{
    Vector3f d = dir;
    d.Normalize();

    Entity* best = 0;
    outDist = maxDist;

    for (size_t i = 0; i < m_entities.size(); ++i)
    {
        Entity& e = m_entities[i];
        if (e.type == ENT_REMOTE_PLAYER)
            continue;

        const EntityDef& def = kDefs[e.type];

        // Intersection rayon / boite alignee sur les axes
        const float hw = def.width * 0.5f;
        const float bx0 = e.position.x - hw, bx1 = e.position.x + hw;
        const float by0 = e.position.y,      by1 = e.position.y + def.height;
        const float bz0 = e.position.z - hw, bz1 = e.position.z + hw;

        float tmin = 0.0f, tmax = maxDist;
        bool hit = true;

        const float o[3] = { origin.x, origin.y, origin.z };
        const float dd[3] = { d.x, d.y, d.z };
        const float lo[3] = { bx0, by0, bz0 };
        const float hi[3] = { bx1, by1, bz1 };

        for (int a = 0; a < 3 && hit; ++a)
        {
            if (fabsf(dd[a]) < 1e-6f)
            {
                if (o[a] < lo[a] || o[a] > hi[a]) hit = false;
            }
            else
            {
                float t1 = (lo[a] - o[a]) / dd[a];
                float t2 = (hi[a] - o[a]) / dd[a];
                if (t1 > t2) { const float t = t1; t1 = t2; t2 = t; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) hit = false;
            }
        }

        if (hit && tmin < outDist)
        {
            outDist = tmin;
            best = &e;
        }
    }

    return best;
}

bool EntityManager::Damage(Entity& e, float amount, BlockType& drop, int& dropCount)
{
    e.health -= amount;
    e.hurtFlash = 1.0f;

    // Recul
    e.velocity.y = 4.0f;

    if (e.health > 0.0f)
        return false;

    const EntityDef& def = kDefs[e.type];
    drop = def.drop;
    dropCount = def.dropCount;
    e.dead = true;
    return true;
}

// ---------------------------------------------------------------------------
//  Rendu
// ---------------------------------------------------------------------------
void EntityManager::Render(const World& world, float dayFactor) const
{
    if (m_entities.empty())
        return;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    for (size_t i = 0; i < m_entities.size(); ++i)
    {
        const Entity& e = m_entities[i];
        const EntityDef& def = kDefs[e.type];

        // Eclairage: on reprend la lumiere du bloc ou se tient la creature
        const int bx = (int)floorf(e.position.x);
        const int by = (int)floorf(e.position.y + def.height * 0.5f);
        const int bz = (int)floorf(e.position.z);

        const float sky = (float)world.GetSkyLight(bx, by, bz) / 15.0f;
        const float blk = (float)world.GetBlockLight(bx, by, bz) / 15.0f;
        float brightness = 0.16f + sky * dayFactor * 0.9f + blk * 0.8f;
        if (brightness > 1.25f) brightness = 1.25f;

        // Clignotement rouge quand la creature encaisse un coup
        float tint[3] = { 1.0f, 1.0f, 1.0f };
        if (e.hurtFlash > 0.0f)
        {
            const float f = Clamp(e.hurtFlash, 0.0f, 1.0f);
            tint[0] = 1.0f + f; tint[1] = 1.0f - f * 0.7f; tint[2] = 1.0f - f * 0.7f;
        }

        // Les couleurs sont donnees en sRGB comme les textures: on repasse en
        // lineaire, sinon le post-traitement les renvoie completement delavees.
        float body[3], head[3], leg[3];
        for (int c = 0; c < 3; ++c)
        {
            body[c] = powf(def.bodyColor[c], 2.2f) * tint[c];
            head[c] = powf(def.headColor[c], 2.2f) * tint[c];
            leg[c]  = powf(def.legColor[c],  2.2f) * tint[c];
        }

        const float swing = sinf(e.walkPhase) * 32.0f;

        glPushMatrix();
        glTranslatef(e.position.x, e.position.y, e.position.z);
        glRotatef(-e.yaw, 0.0f, 1.0f, 0.0f);

        if (e.type == ENT_ZOMBIE || e.type == ENT_REMOTE_PLAYER)
        {
            // Silhouette humanoide: jambes, torse, tete, bras tendus
            const float legLen = 0.75f;
            const float bodyH = 0.62f;

            DrawLeg(-0.11f, legLen, 0.0f, legLen, 0.09f,  swing, leg, brightness);
            DrawLeg( 0.11f, legLen, 0.0f, legLen, 0.09f, -swing, leg, brightness);

            DrawBox(0.0f, legLen + bodyH * 0.5f, 0.0f, 0.22f, bodyH * 0.5f, 0.12f, body, brightness);
            DrawBox(0.0f, legLen + bodyH + 0.22f, 0.0f, 0.22f, 0.22f, 0.22f, head, brightness);

            // Bras tendus vers l'avant pour le zombie
            const float armAngle = (e.type == ENT_ZOMBIE) ? -80.0f : swing * 0.6f;
            DrawLeg(-0.31f, legLen + bodyH, 0.0f, 0.66f, 0.09f,  armAngle, body, brightness);
            DrawLeg( 0.31f, legLen + bodyH, 0.0f, 0.66f, 0.09f,  armAngle, body, brightness);
        }
        else
        {
            // Quadrupede
            const float legLen = (e.type == ENT_COW) ? 0.62f : 0.36f;
            const float bodyH = (e.type == ENT_COW) ? 0.52f : 0.42f;
            const float bodyL = (e.type == ENT_COW) ? 0.62f : 0.50f;
            const float bodyW = (e.type == ENT_COW) ? 0.28f : 0.26f;
            const float headS = (e.type == ENT_COW) ? 0.24f : 0.22f;

            DrawLeg(-bodyW * 0.7f, legLen, -bodyL * 0.6f, legLen, 0.08f,  swing, leg, brightness);
            DrawLeg( bodyW * 0.7f, legLen, -bodyL * 0.6f, legLen, 0.08f, -swing, leg, brightness);
            DrawLeg(-bodyW * 0.7f, legLen,  bodyL * 0.6f, legLen, 0.08f, -swing, leg, brightness);
            DrawLeg( bodyW * 0.7f, legLen,  bodyL * 0.6f, legLen, 0.08f,  swing, leg, brightness);

            DrawBox(0.0f, legLen + bodyH * 0.5f, 0.0f, bodyW, bodyH * 0.5f, bodyL, body, brightness);
            DrawBox(0.0f, legLen + bodyH * 0.72f, -bodyL - headS * 0.8f,
                    headS, headS, headS, head, brightness);

            // Groin du cochon
            if (e.type == ENT_PIG)
                DrawBox(0.0f, legLen + bodyH * 0.66f, -bodyL - headS * 1.7f,
                        0.08f, 0.06f, 0.04f, leg, brightness);
        }

        glPopMatrix();
    }

    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
}
