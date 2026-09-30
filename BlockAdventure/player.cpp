#include "player.h"
#include "world.h"
#include <cmath>

const float Player::WIDTH  = 0.6f;
const float Player::HEIGHT = 1.8f;
const float Player::EYE    = 1.62f;

namespace
{
    const float GRAVITY        = 28.0f;
    const float JUMP_SPEED     = 8.6f;
    const float WALK_SPEED     = 4.4f;
    const float SPRINT_SPEED   = 6.1f;
    const float SNEAK_SPEED    = 1.5f;
    const float FLY_SPEED      = 13.0f;
    const float SWIM_SPEED     = 3.2f;
    const float MAX_FALL       = 60.0f;
    const float WATER_FALL     = 3.0f;
    const float ACCEL_GROUND   = 14.0f;
    const float ACCEL_AIR      = 3.0f;
    const float FRICTION       = 11.0f;
    const float SAFE_FALL      = 3.0f;   // hauteur de chute sans degats, en blocs

    inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

Player::Player()
    : m_position(0.0f, 80.0f, 0.0f), m_velocity(0.0f, 0.0f, 0.0f),
      m_rotX(0.0f), m_rotY(0.0f),
      m_forward(false), m_back(false), m_left(false), m_right(false),
      m_jump(false), m_sneak(false), m_sprint(false),
      m_onGround(false), m_inWater(false), m_headInWater(false), m_inLava(false),
      m_flying(false), m_creative(false),
      m_health(20.0f), m_hunger(20.0f), m_oxygen(10.0f),
      m_damageFlash(0.0f), m_damageCooldown(0.0f), m_fallStartY(0.0f), m_falling(false),
      m_lavaBurn(0.0f)
{
}

void Player::Reset(const Vector3f& feetPosition)
{
    m_position = feetPosition;
    m_velocity = Vector3f(0.0f, 0.0f, 0.0f);
    m_health = 20.0f;
    m_hunger = 20.0f;
    m_oxygen = 10.0f;
    m_falling = false;
    m_damageFlash = 0.0f;
}

void Player::TurnLeftRight(float value)
{
    m_rotY += value;
    while (m_rotY > 360.0f) m_rotY -= 360.0f;
    while (m_rotY < 0.0f)   m_rotY += 360.0f;
}

void Player::TurnTopBottom(float value)
{
    m_rotX = Clamp(m_rotX + value, -89.9f, 89.9f);
}

float Player::EyeHeight() const
{
    return m_sneak ? (EYE - 0.22f) : EYE;
}

Vector3f Player::EyePosition() const
{
    return Vector3f(m_position.x, m_position.y + EyeHeight(), m_position.z);
}

Vector3f Player::Direction() const
{
    const float ry = m_rotY * 0.017453293f;
    const float rx = m_rotX * 0.017453293f;
    const float cx = cosf(rx);
    return Vector3f(sinf(ry) * cx, -sinf(rx), -cosf(ry) * cx);
}

void Player::ApplyTransformation(Transformation& transformation) const
{
    // Matrix4::ApplyRotation applique la rotation inverse de glRotatef,
    // d'ou les angles negatifs (convention conservee du code d'origine).
    transformation.ApplyRotation(-m_rotX, 1.0f, 0.0f, 0.0f);
    transformation.ApplyRotation(-m_rotY, 0.0f, 1.0f, 0.0f);
    transformation.ApplyTranslation(-EyePosition());
}

void Player::SetMoveInput(bool forward, bool back, bool left, bool right)
{
    m_forward = forward;
    m_back = back;
    m_left = left;
    m_right = right;
}

void Player::ToggleFly()
{
    m_flying = !m_flying;
    if (m_flying)
        m_velocity.y = 0.0f;
}

void Player::SetCreative(bool v)
{
    m_creative = v;
    if (!m_creative)
        m_flying = false;
    else
        m_health = 20.0f;
}

void Player::ApplyDamage(float amount)
{
    if (m_creative || amount <= 0.0f) return;
    if (m_damageCooldown > 0.0f) return;

    m_health -= amount;
    if (m_health < 0.0f) m_health = 0.0f;
    m_damageFlash = 1.0f;
    m_damageCooldown = 0.5f;
}

void Player::Unstuck(const World& world)
{
    if (!Collides(world, m_position))
        return;

    for (int i = 0; i < 24 && Collides(world, m_position); ++i)
        m_position.y += 1.0f;

    m_velocity = Vector3f(0.0f, 0.0f, 0.0f);
    m_falling = false;
}

void Player::Heal(float amount)
{
    m_health = Clamp(m_health + amount, 0.0f, 20.0f);
}

// ---------------------------------------------------------------------------
//  Collisions
// ---------------------------------------------------------------------------
bool Player::Collides(const World& world, const Vector3f& p) const
{
    const float hw = WIDTH * 0.5f - 0.001f;

    const int x0 = (int)floorf(p.x - hw);
    const int x1 = (int)floorf(p.x + hw);
    const int z0 = (int)floorf(p.z - hw);
    const int z1 = (int)floorf(p.z + hw);
    const int y0 = (int)floorf(p.y + 0.001f);
    const int y1 = (int)floorf(p.y + HEIGHT - 0.001f);

    for (int y = y0; y <= y1; ++y)
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
                if (world.IsSolid(x, y, z))
                    return true;

    return false;
}

// Deplace le joueur sur un axe et retourne la distance reellement parcourue.
// En cas d'obstacle on affine par dichotomie pour venir se coller au bloc
// plutot que de rester bloque a plusieurs centimetres.
float Player::MoveAxis(World& world, int axis, float delta)
{
    if (delta == 0.0f)
        return 0.0f;

    Vector3f test = m_position;
    float* comp[3] = { &test.x, &test.y, &test.z };

    *comp[axis] += delta;
    if (!Collides(world, test))
    {
        m_position = test;
        return delta;
    }

    float lo = 0.0f, hi = delta;
    for (int i = 0; i < 6; ++i)
    {
        const float mid = (lo + hi) * 0.5f;
        test = m_position;
        *comp[axis] += mid;
        if (Collides(world, test)) hi = mid;
        else                       lo = mid;
    }

    test = m_position;
    *comp[axis] += lo;
    m_position = test;
    return lo;
}

void Player::UpdateEnvironment(const World& world)
{
    const int fx = (int)floorf(m_position.x);
    const int fz = (int)floorf(m_position.z);

    const BlockType feet = world.GetBlock(fx, (int)floorf(m_position.y + 0.2f), fz);
    const BlockType head = world.GetBlock(fx, (int)floorf(m_position.y + EyeHeight()), fz);

    m_inWater = (feet == BTYPE_WATER) || (head == BTYPE_WATER);
    m_headInWater = (head == BTYPE_WATER);
    m_inLava = (feet == BTYPE_LAVA) || (head == BTYPE_LAVA);
}

// ---------------------------------------------------------------------------
//  Simulation
// ---------------------------------------------------------------------------
void Player::Update(World& world, float elapsed)
{
    if (elapsed <= 0.0f) return;
    if (elapsed > 0.1f) elapsed = 0.1f;   // evite les tunnels apres un freeze

    UpdateEnvironment(world);

    if (m_damageCooldown > 0.0f) m_damageCooldown -= elapsed;
    if (m_damageFlash > 0.0f)    m_damageFlash -= elapsed * 1.8f;
    if (m_damageFlash < 0.0f)    m_damageFlash = 0.0f;

    // --- direction voulue -------------------------------------------------
    const float ry = m_rotY * 0.017453293f;
    Vector3f wish(0.0f, 0.0f, 0.0f);

    if (m_forward) wish += Vector3f( sinf(ry), 0.0f, -cosf(ry));
    if (m_back)    wish += Vector3f(-sinf(ry), 0.0f,  cosf(ry));
    if (m_left)    wish += Vector3f(-cosf(ry), 0.0f, -sinf(ry));
    if (m_right)   wish += Vector3f( cosf(ry), 0.0f,  sinf(ry));

    const float wishLen = wish.Length();
    if (wishLen > 0.0001f)
        wish /= wishLen;

    float speed = WALK_SPEED;
    if (m_flying)                        speed = FLY_SPEED * (m_sprint ? 2.0f : 1.0f);
    else if (m_inWater)                  speed = SWIM_SPEED;
    else if (m_sneak)                    speed = SNEAK_SPEED;
    else if (m_sprint && m_forward)      speed = SPRINT_SPEED;

    // --- integration de la vitesse horizontale ----------------------------
    const float accel = (m_onGround || m_flying || m_inWater) ? ACCEL_GROUND : ACCEL_AIR;
    const Vector3f target = wish * speed;

    m_velocity.x += (target.x - m_velocity.x) * Clamp(accel * elapsed, 0.0f, 1.0f);
    m_velocity.z += (target.z - m_velocity.z) * Clamp(accel * elapsed, 0.0f, 1.0f);

    if (wishLen < 0.0001f && (m_onGround || m_flying || m_inWater))
    {
        const float f = Clamp(FRICTION * elapsed, 0.0f, 1.0f);
        m_velocity.x -= m_velocity.x * f;
        m_velocity.z -= m_velocity.z * f;
    }

    // --- vertical ---------------------------------------------------------
    if (m_flying)
    {
        float vy = 0.0f;
        if (m_jump)  vy += FLY_SPEED;
        if (m_sneak) vy -= FLY_SPEED;
        m_velocity.y = vy;
        m_falling = false;
    }
    else if (m_inWater)
    {
        // Poussee d'Archimede + nage
        m_velocity.y -= GRAVITY * 0.28f * elapsed;
        if (m_jump)  m_velocity.y += 11.0f * elapsed;
        if (m_sneak) m_velocity.y -= 6.0f * elapsed;
        m_velocity.y = Clamp(m_velocity.y, -WATER_FALL, 4.0f);
        m_falling = false;
    }
    else
    {
        if (m_onGround && m_jump)
        {
            m_velocity.y = JUMP_SPEED;
            m_onGround = false;
        }
        m_velocity.y -= GRAVITY * elapsed;
        if (m_velocity.y < -MAX_FALL) m_velocity.y = -MAX_FALL;

        if (!m_onGround && !m_falling && m_velocity.y < 0.0f)
        {
            m_falling = true;
            m_fallStartY = m_position.y;
        }
    }

    // --- deplacement + collisions ----------------------------------------
    const float dx = m_velocity.x * elapsed;
    const float dy = m_velocity.y * elapsed;
    const float dz = m_velocity.z * elapsed;

    if (MoveAxis(world, 0, dx) != dx) m_velocity.x = 0.0f;
    if (MoveAxis(world, 2, dz) != dz) m_velocity.z = 0.0f;

    const float movedY = MoveAxis(world, 1, dy);
    const bool blockedY = (movedY != dy);

    bool landed = false;
    if (blockedY)
    {
        if (dy < 0.0f)
        {
            landed = !m_onGround;
            m_onGround = true;
        }
        m_velocity.y = 0.0f;
    }
    else if (dy != 0.0f)
    {
        m_onGround = false;
    }

    // Contact au sol meme sans mouvement vertical (bord de bloc)
    if (!m_onGround && fabsf(m_velocity.y) < 0.001f)
    {
        Vector3f probe = m_position;
        probe.y -= 0.02f;
        if (Collides(world, probe))
            m_onGround = true;
    }

    // --- degats de chute --------------------------------------------------
    if (landed && m_falling)
    {
        const float distance = m_fallStartY - m_position.y;
        if (distance > SAFE_FALL && !m_inWater)
            ApplyDamage(distance - SAFE_FALL);
        m_falling = false;
    }
    if (m_onGround || m_inWater)
        m_falling = false;

    // --- noyade -----------------------------------------------------------
    if (m_headInWater && !m_creative)
    {
        m_oxygen -= elapsed;
        if (m_oxygen <= 0.0f)
        {
            m_oxygen = 0.0f;
            m_damageCooldown = 0.0f;
            ApplyDamage(elapsed * 2.0f);
        }
    }
    else
    {
        m_oxygen = Clamp(m_oxygen + elapsed * 3.0f, 0.0f, MaxOxygen());
    }

    // --- lave -------------------------------------------------------------
    if (m_inLava && !m_creative)
    {
        m_lavaBurn += elapsed;
        if (m_lavaBurn > 0.5f)
        {
            m_lavaBurn = 0.0f;
            m_damageCooldown = 0.0f;
            ApplyDamage(2.0f);
        }
    }

    // --- faim et regeneration --------------------------------------------
    if (!m_creative)
    {
        const float drain = IsSprinting() ? 0.020f : 0.006f;
        m_hunger = Clamp(m_hunger - drain * elapsed, 0.0f, 20.0f);

        if (m_hunger > 17.0f && m_health < 20.0f)
            Heal(elapsed * 0.35f);
        else if (m_hunger <= 0.0f)
            ApplyDamage(elapsed * 0.4f);
    }

    // --- filet de securite: ne jamais tomber hors du monde ---------------
    if (m_position.y < -6.0f)
    {
        m_position.y = (float)(CHUNK_SIZE_Y - 2);
        m_velocity = Vector3f(0.0f, 0.0f, 0.0f);
        m_falling = false;
    }
}
