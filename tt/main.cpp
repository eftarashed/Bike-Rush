// ====================================================================
//  MOTO RACER 3D  -  OpenGL / GLUT
//
//  Controls
//    LEFT / RIGHT ... change lane
//    UP / DOWN ...... throttle up / down
//    SHIFT (hold) ... BOOST (much higher speed)      [B = boost toggle]
//    P .............. pause          R ... restart
//    N / D / T ...... night / day / toggle
//    + / - .......... camera zoom     E or ESC ... exit
//
//  Build (MinGW):  g++ moto_racer.cpp -o moto_racer -lfreeglut -lopengl32 -lglu32
// ====================================================================
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// -------------------------------------------------------------------
//  Constants
// -------------------------------------------------------------------
const float PI              = 3.14159265f;
const float ROAD_HALF       = 1.0f;            // visible asphalt is x = -1 .. 1
const float LANE_LEFT_X     = -0.50f;
const float LANE_RIGHT_X    =  0.50f;
const float JUNGLE_START    = 300.0f;
const float FINISH_DISTANCE = 520.0f;
const float WORLD_LENGTH    = FINISH_DISTANCE + 80.0f;

const float BIKE_SCALE      = 1.25f;
const float CAR_SCALE       = 1.5f;
// Where (along the track, relative to the scrolled world) the bike is drawn.
const float BIKE_WORLD_Y    = -3.6f;

// Hit box: the bike (nose ~0.76, tail ~0.69) + car half length (~0.53) -> ~1.2.
// 1.15 means the game only ends when the two models really overlap.
const float COLLISION_WINDOW  = 1.15f;
const float COLLISION_LATERAL = 0.38f;

const float BOOST_MAX       = 1.8f;
const float CAM_DISTANCE_MIN = -14.0f;
const float CAM_DISTANCE_MAX = -6.5f;

const int   MAX_OBSTACLES   = 64;

enum { ST_PLAY = 0, ST_CRASH = 1, ST_WIN = 2 };

// -------------------------------------------------------------------
//  Game state
// -------------------------------------------------------------------
struct Obstacle { float pos; int lane; bool scored; };
Obstacle obstacles[MAX_OBSTACLES];
int      obstacleCount = 0;

int   gameState   = ST_PLAY;
float bikePos     = 0.0f;              // distance ridden along the track
float xp          = LANE_LEFT_X;       // bike lateral position (smoothly follows lane)
int   carpos      = 0;                 // 0 = left lane, 1 = right lane
float bikeLean    = 0.0f;
float wheelAngle  = 0.0f;
float curSpeed    = 0.0f;              // world units / second
float throttle    = 1.0f;              // UP / DOWN arrows
float boostSmooth = 1.0f;
bool  boostLatch  = false;
bool  shiftHeld   = false;
int   shiftSeenMs = -10000;
int   score       = 0;
bool  paused      = false;
float crashTimer  = 0.0f;
float crashDir    = 1.0f;
float finishTimer = 0.0f;
float playTime    = 0.0f;
bool  denseMode   = false;
float camDistance = -7.5f;
float cloudDrift  = 0.0f;
float animTime    = 0.0f;
int   winW = 800, winH = 500;
int   lastTimeMs  = 0;

// -------------------------------------------------------------------
//  Day / night
// -------------------------------------------------------------------
bool  isNight = false;
float sky_red = 0.62f, sky_green = 0.82f, sky_blue = 0.95f;   // horizon colour (also fog)
float zenith_r = 0.13f, zenith_g = 0.42f, zenith_b = 0.85f;
int   roadlight = 50;
float fogNear = 180.0f, fogFar = 470.0f;
float envDim  = 1.0f;                                         // darkens unlit ground at night

void setNight()
{
    sky_red = 0.03f; sky_green = 0.06f; sky_blue = 0.15f;
    zenith_r = 0.005f; zenith_g = 0.01f; zenith_b = 0.05f;
    roadlight = 255; isNight = true;
    fogNear = 110.0f; fogFar = 340.0f;
    envDim = 0.42f;
}
void setDay()
{
    sky_red = 0.62f; sky_green = 0.82f; sky_blue = 0.95f;
    zenith_r = 0.13f; zenith_g = 0.42f; zenith_b = 0.85f;
    roadlight = 50; isNight = false;
    fogNear = 180.0f; fogFar = 470.0f;
    envDim = 1.0f;
}

// -------------------------------------------------------------------
//  Small helpers
// -------------------------------------------------------------------
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static void lightOn()  { glEnable(GL_LIGHTING); }
static void lightOff() { glDisable(GL_LIGHTING); }

// Only draw scenery that is near the bike (behind a little / ahead until the fog ends)
static bool inView(float z)
{
    float d = z - bikePos;
    return d > -14.0f && d < fogFar + 25.0f;
}

static void box(float x, float y, float z, float sx, float sy, float sz)
{
    glPushMatrix();
        glTranslatef(x, y, z);
        glScalef(sx, sy, sz);
        glutSolidCube(1.0);
    glPopMatrix();
}

static void ball(float x, float y, float z, float r)
{
    glPushMatrix();
        glTranslatef(x, y, z);
        glutSolidSphere(r, 12, 10);
    glPopMatrix();
}

GLUquadric* treeQuad = NULL;

// A cylinder (tube) between two points - used for frame, forks, arms, legs...
static void beam(float ax, float ay, float az, float bx, float by, float bz, float r0, float r1)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f) return;
    glPushMatrix();
        glTranslatef(ax, ay, az);
        float ux = -dy, uy = dx;                       // cross((0,0,1), d)
        if (fabsf(ux) < 1e-6f && fabsf(uy) < 1e-6f)
        {
            if (dz < 0) glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
        }
        else
        {
            glRotatef(acosf(clampf(dz / len, -1.0f, 1.0f)) * 57.29578f, ux, uy, 0.0f);
        }
        gluCylinder(treeQuad, r0, r1, len, 10, 1);
    glPopMatrix();
}

// Soft dark ellipse on the ground (blob shadow)
static void shadow(float x, float y, float rx, float ry, float alpha)
{
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBegin(GL_TRIANGLE_FAN);
        glColor4f(0.0f, 0.0f, 0.0f, alpha);
        glVertex3f(x, y, 0.04f);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        for (int a = 0; a <= 20; a++)
        {
            float t = a * 2.0f * PI / 20.0f;
            glVertex3f(x + cosf(t) * rx, y + sinf(t) * ry, 0.04f);
        }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// -------------------------------------------------------------------
//  Obstacles (single source of truth for drawing + collision)
// -------------------------------------------------------------------
void buildObstacles()
{
    obstacleCount = 0;
    for (float p = 45.0f; p < FINISH_DISTANCE - 15.0f && obstacleCount < MAX_OBSTACLES;
         p += 24.0f + (rand() % 100) / 10.0f)
    {
        obstacles[obstacleCount].pos    = p;
        obstacles[obstacleCount].lane   = rand() % 2;
        obstacles[obstacleCount].scored = false;
        obstacleCount++;
    }
}

// -------------------------------------------------------------------
//  Sky, sun / moon, stars, clouds
// -------------------------------------------------------------------
struct Star  { float x, y; };
struct Cloud { float x, y, z, scale; };
Star  stars[90];
Cloud clouds[14];
bool  skyReady = false;

void initSky()
{
    if (skyReady) return;
    for (int s = 0; s < 90; s++)
    {
        stars[s].x = -45.0f + (rand() % 900) / 10.0f;
        stars[s].y = 13.0f + (rand() % 400) / 10.0f;
    }
    for (int c = 0; c < 14; c++)
    {
        clouds[c].x = -32.0f + (rand() % 640) / 10.0f;
        clouds[c].y = 15.0f + (rand() % 150) / 10.0f;
        clouds[c].z = 1.5f + (rand() % 25) / 10.0f;
        clouds[c].scale = 1.1f + (rand() % 20) / 10.0f;
    }
    skyReady = true;
}

static void cloudPuff(float x, float y, float z, float scale, float alpha)
{
    glPushMatrix();
        glTranslatef(x, y, z);
        glScalef(scale, scale * 0.55f, scale * 0.55f);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glutSolidSphere(1.0, 12, 10);
        glPushMatrix(); glTranslatef( 1.05f, 0.05f, 0.0f); glutSolidSphere(0.72, 10, 8); glPopMatrix();
        glPushMatrix(); glTranslatef(-1.05f, 0.02f, 0.0f); glutSolidSphere(0.78, 10, 8); glPopMatrix();
        glPushMatrix(); glTranslatef( 0.45f, 0.55f, 0.05f); glutSolidSphere(0.62, 10, 8); glPopMatrix();
        glPushMatrix(); glTranslatef(-0.50f, 0.48f, 0.05f); glutSolidSphere(0.58, 10, 8); glPopMatrix();
    glPopMatrix();
}

void sky()
{
    initSky();
    glPushMatrix();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glTranslatef(0.0f, 0.0f, -55.0f);

    // horizon colour band (below the horizon line) + gradient up to the zenith
    glBegin(GL_QUADS);
        glColor3f(sky_red, sky_green, sky_blue);
        glVertex3f(-500.0f, -60.0f, 0.0f); glVertex3f(500.0f, -60.0f, 0.0f);
        glVertex3f( 500.0f,  12.0f, 0.0f); glVertex3f(-500.0f, 12.0f, 0.0f);

        glColor3f(sky_red, sky_green, sky_blue);
        glVertex3f(-500.0f, 12.0f, 0.0f);  glVertex3f(500.0f, 12.0f, 0.0f);
        glColor3f(zenith_r, zenith_g, zenith_b);
        glVertex3f( 500.0f, 75.0f, 0.0f);  glVertex3f(-500.0f, 75.0f, 0.0f);
    glEnd();

    // Sun / moon with a soft glow
    float gx = isNight ? 18.0f : -18.0f;
    float gy = isNight ? 20.0f : 22.0f;
    float gr = isNight ? 6.0f : 10.0f;
    glPushMatrix();
        glTranslatef(gx, gy, 3.5f);
        glBegin(GL_TRIANGLE_FAN);
            if (isNight) glColor4f(0.55f, 0.65f, 0.95f, 0.35f);
            else         glColor4f(1.0f, 0.92f, 0.60f, 0.60f);
            glVertex3f(0, 0, 0);
            if (isNight) glColor4f(0.55f, 0.65f, 0.95f, 0.0f);
            else         glColor4f(1.0f, 0.92f, 0.60f, 0.0f);
            for (int a = 0; a <= 28; a++)
                glVertex3f(cosf(a * 2.0f * PI / 28.0f) * gr, sinf(a * 2.0f * PI / 28.0f) * gr, 0);
        glEnd();
        if (isNight) glColor3f(0.93f, 0.93f, 0.88f); else glColor3f(1.0f, 0.90f, 0.35f);
        glutSolidSphere(isNight ? 1.5 : 2.0, 24, 24);
    glPopMatrix();

    // Clouds drifting slowly
    float alpha = isNight ? 0.14f : 0.85f;
    for (int c = 0; c < 14; c++)
    {
        float x = fmodf(clouds[c].x + cloudDrift + 45.0f, 90.0f) - 45.0f;
        cloudPuff(x, clouds[c].y, clouds[c].z, clouds[c].scale, alpha);
    }

    if (isNight)
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for (int s = 0; s < 90; s++) glVertex3f(stars[s].x, stars[s].y, 3.0f);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glPopMatrix();
}

// -------------------------------------------------------------------
//  Ground, road, markings
// -------------------------------------------------------------------
static void viewRange(float step, float& a, float& b)
{
    a = floorf((bikePos - 16.0f) / step) * step;
    b = bikePos + fogFar + 30.0f;
}

void ground()
{
    lightOff();
    float d = envDim;
    float a, b;

    // huge far ground (sliced along the road so per-vertex fog stays correct)
    viewRange(25.0f, a, b);
    if (a < -25.0f) a = -25.0f;
    float lim = WORLD_LENGTH + 300.0f;
    glColor3f(0.02f * d, 0.50f * d, 0.08f * d);
    for (float z = a; z < b && z < lim; z += 25.0f)
    {
        glBegin(GL_QUADS);
            glVertex3f(-400.0f, z,        -0.12f);
            glVertex3f( 400.0f, z,        -0.12f);
            glVertex3f( 400.0f, z + 25.0f, -0.12f);
            glVertex3f(-400.0f, z + 25.0f, -0.12f);
        glEnd();
    }

    // mown-stripe grass on both sides of the road
    viewRange(5.0f, a, b);
    if (a < -10.0f) a = -10.0f;
    for (float z = a; z < b && z < WORLD_LENGTH + 200.0f; z += 5.0f)
    {
        bool alt = (((int)floorf(z / 5.0f)) & 1) != 0;
        float g = alt ? 0.62f : 0.55f;
        glColor3f(0.0f, g * d, 0.08f * d);
        glBegin(GL_QUADS);
            glVertex3f(-40.0f,            z,        -0.06f);
            glVertex3f(-ROAD_HALF - 0.12f, z,        -0.06f);
            glVertex3f(-ROAD_HALF - 0.12f, z + 5.0f, -0.06f);
            glVertex3f(-40.0f,            z + 5.0f, -0.06f);

            glVertex3f( ROAD_HALF + 0.12f, z,        -0.06f);
            glVertex3f( 40.0f,            z,        -0.06f);
            glVertex3f( 40.0f,            z + 5.0f, -0.06f);
            glVertex3f( ROAD_HALF + 0.12f, z + 5.0f, -0.06f);
        glEnd();
    }
}

void road()
{
    lightOff();
    float d = envDim;
    float a, b;

    // asphalt
    viewRange(20.0f, a, b);
    if (a < -10.0f) a = -10.0f;
    glColor3f(0.17f * d, 0.17f * d, 0.18f * d);
    for (float z = a; z < b && z < WORLD_LENGTH; z += 20.0f)
    {
        glBegin(GL_QUADS);
            glVertex3f(-ROAD_HALF, z,        0.0f);
            glVertex3f( ROAD_HALF, z,        0.0f);
            glVertex3f( ROAD_HALF, z + 20.0f, 0.0f);
            glVertex3f(-ROAD_HALF, z + 20.0f, 0.0f);
        glEnd();
    }

    // slightly darker tyre tracks in each lane
    glColor3f(0.13f * d, 0.13f * d, 0.14f * d);
    for (float z = a; z < b && z < WORLD_LENGTH; z += 20.0f)
    {
        for (int lane = 0; lane < 2; lane++)
        {
            float cx = lane == 0 ? LANE_LEFT_X : LANE_RIGHT_X;
            glBegin(GL_QUADS);
                glVertex3f(cx - 0.24f, z,        0.02f);
                glVertex3f(cx - 0.10f, z,        0.02f);
                glVertex3f(cx - 0.10f, z + 20.0f, 0.02f);
                glVertex3f(cx - 0.24f, z + 20.0f, 0.02f);
                glVertex3f(cx + 0.10f, z,        0.02f);
                glVertex3f(cx + 0.24f, z,        0.02f);
                glVertex3f(cx + 0.24f, z + 20.0f, 0.02f);
                glVertex3f(cx + 0.10f, z + 20.0f, 0.02f);
            glEnd();
        }
    }

    // curbs: red / white blocks
    viewRange(2.0f, a, b);
    if (a < -10.0f) a = -10.0f;
    glBegin(GL_QUADS);
    for (float z = a; z < b && z < WORLD_LENGTH; z += 2.0f)
    {
        bool red = (((int)floorf(z / 2.0f)) & 1) != 0;
        if (red) glColor3f(0.80f * d, 0.12f * d, 0.10f * d);
        else     glColor3f(0.90f * d, 0.90f * d, 0.90f * d);
        glVertex3f( ROAD_HALF,        z,        0.03f);
        glVertex3f( ROAD_HALF + 0.12f, z,        0.03f);
        glVertex3f( ROAD_HALF + 0.12f, z + 2.0f, 0.03f);
        glVertex3f( ROAD_HALF,        z + 2.0f, 0.03f);
        glVertex3f(-ROAD_HALF - 0.12f, z,        0.03f);
        glVertex3f(-ROAD_HALF,        z,        0.03f);
        glVertex3f(-ROAD_HALF,        z + 2.0f, 0.03f);
        glVertex3f(-ROAD_HALF - 0.12f, z + 2.0f, 0.03f);
    }
    glEnd();

    // dashed centre line + solid edge lines
    glColor3f(0.95f * d, 0.95f * d, 0.95f * d);
    viewRange(3.0f, a, b);
    if (a < -9.0f) a = -9.0f;
    glBegin(GL_QUADS);
    for (float z = a; z < b && z < WORLD_LENGTH; z += 3.0f)
    {
        glVertex3f(-0.025f, z,        0.035f);
        glVertex3f( 0.025f, z,        0.035f);
        glVertex3f( 0.025f, z + 1.6f, 0.035f);
        glVertex3f(-0.025f, z + 1.6f, 0.035f);
    }
    glEnd();
    viewRange(20.0f, a, b);
    if (a < -10.0f) a = -10.0f;
    glBegin(GL_QUADS);
    for (float z = a; z < b && z < WORLD_LENGTH; z += 20.0f)
    {
        glVertex3f(-0.94f, z,        0.035f); glVertex3f(-0.90f, z,        0.035f);
        glVertex3f(-0.90f, z + 20.0f, 0.035f); glVertex3f(-0.94f, z + 20.0f, 0.035f);
        glVertex3f( 0.90f, z,        0.035f); glVertex3f( 0.94f, z,        0.035f);
        glVertex3f( 0.94f, z + 20.0f, 0.035f); glVertex3f( 0.90f, z + 20.0f, 0.035f);
    }
    glEnd();
}

// Checkered finish line, posts and banner
void finishLine()
{
    if (!inView(FINISH_DISTANCE)) return;
    lightOff();
    float fz = FINISH_DISTANCE;

    int cols = 10;
    float stripeW = (2.0f * ROAD_HALF) / cols;
    for (int row = 0; row < 2; row++)
        for (int c = 0; c < cols; c++)
        {
            bool w = ((c + row) & 1) == 0;
            float v = w ? 1.0f : 0.05f;
            glColor3f(v * envDim, v * envDim, v * envDim);
            float cx = -ROAD_HALF + c * stripeW;
            float y0 = fz - 0.4f + row * 0.4f;
            glBegin(GL_QUADS);
                glVertex3f(cx,           y0,        0.04f);
                glVertex3f(cx + stripeW, y0,        0.04f);
                glVertex3f(cx + stripeW, y0 + 0.4f, 0.04f);
                glVertex3f(cx,           y0 + 0.4f, 0.04f);
            glEnd();
        }

    lightOn();
    glColor3ub(190, 190, 195);
    box(-1.25f, fz, 0.9f, 0.08f, 0.08f, 1.8f);
    box( 1.25f, fz, 0.9f, 0.08f, 0.08f, 1.8f);
    lightOff();

    int bands = 12;
    float bandW = 2.6f / bands;
    for (int row = 0; row < 2; row++)
        for (int c = 0; c < bands; c++)
        {
            bool w = ((c + row) & 1) == 0;
            float v = w ? 1.0f : 0.05f;
            glColor3f(v * envDim, v * envDim, v * envDim);
            float cx = -1.3f + c * bandW;
            float zz = 1.75f + row * 0.15f;
            glBegin(GL_QUADS);
                glVertex3f(cx,         fz, zz);
                glVertex3f(cx + bandW, fz, zz);
                glVertex3f(cx + bandW, fz, zz + 0.15f);
                glVertex3f(cx,         fz, zz + 0.15f);
            glEnd();
            // second face so the banner is visible from both directions
        }
}

// -------------------------------------------------------------------
//  Mountains
// -------------------------------------------------------------------
void mountains()
{
    lightOn();
    float dim = isNight ? 0.55f : 1.0f;
    float spacing = denseMode ? 24.0f : 40.0f;
    float heightMul = denseMode ? 1.7f : 1.15f;

    for (float z = -60.0f; z < WORLD_LENGTH + 40.0f; z += spacing)
    {
        if (!inView(z) && !inView(z + 26.0f)) continue;
        float h1 = (4.5f + 1.8f * sinf(z * 0.021f)) * heightMul;
        float h2 = (7.5f + 2.6f * cosf(z * 0.015f)) * heightMul;

        glPushMatrix();
            glColor3f(0.46f * dim, 0.53f * dim, 0.66f * dim);
            glTranslatef(-13.0f, z, -0.05f);
            glutSolidCone(3.4, h2, 7, 3);
        glPopMatrix();

        if (h2 > 8.0f)
        {
            glPushMatrix();
                glColor3f(0.95f * dim, 0.95f * dim, 0.98f * dim);
                glTranslatef(-13.0f, z, h2 * 0.70f - 0.05f);
                glutSolidCone(1.06, h2 * 0.31f, 7, 2);
            glPopMatrix();
        }

        glPushMatrix();
            glColor3f(0.30f * dim, 0.29f * dim, 0.25f * dim);
            glTranslatef(-9.0f, z + 26.0f, -0.05f);
            glutSolidCone(2.5, h1, 6, 2);
        glPopMatrix();
    }
    lightOff();
}

// -------------------------------------------------------------------
//  Lamp posts, grass tufts
// -------------------------------------------------------------------
static void lamppost(float z)
{
    glColor3f(0.22f, 0.22f, 0.24f);
    box(-1.06f, z, 0.62f, 0.045f, 0.045f, 1.24f);
    glColor3f(0.18f, 0.18f, 0.20f);
    box(-1.06f, z, 0.03f, 0.10f, 0.10f, 0.06f);
    glColor3f(0.20f, 0.20f, 0.22f);
    box(-0.82f, z, 1.22f, 0.44f, 0.04f, 0.04f);
    glColor3f(0.12f, 0.12f, 0.13f);
    box(-0.58f, z, 1.16f, 0.14f, 0.14f, 0.06f);

    // bulb (emissive)
    lightOff();
    glEnable(GL_BLEND);
    glColor4ub(255, 245, 190, roadlight);
    ball(-0.58f, z, 1.10f, 0.055f);
    glDisable(GL_BLEND);
    lightOn();
}

void roadside()
{
    lightOn();
    for (float z = -38.0f; z < WORLD_LENGTH; z += 9.0f)
        if (inView(z)) lamppost(z);

    // grass tufts, two passes: neat and scruffy
    float gdim = isNight ? 0.55f : 1.0f;
    for (float z = -10.0f; z < WORLD_LENGTH; z += 1.2f)
    {
        if (!inView(z)) continue;
        float jitter = sinf(z * 0.9f) * 0.20f;
        float shade = 0.70f + 0.30f * sinf(z * 0.5f);

        glPushMatrix();
            glColor3f(0.0f, 0.50f * shade * gdim, 0.04f * gdim);
            glTranslatef(-2.2f + jitter, z, -0.04f);
            glScalef(0.09f, 0.09f, 0.11f);
            glutSolidCone(0.5, 1.2, 5, 1);
        glPopMatrix();

        glPushMatrix();
            glColor3f(0.0f, 0.55f * (1.05f - shade) * gdim + 0.10f, 0.04f * gdim);
            glTranslatef(1.65f - jitter * 0.5f, z + 0.6f, -0.04f);
            glScalef(0.09f, 0.09f, 0.11f);
            glutSolidCone(0.5, 1.2, 5, 1);
        glPopMatrix();
    }
    for (float z = -8.0f; z < WORLD_LENGTH; z += 1.7f)
    {
        if (!inView(z)) continue;
        float rndA = sinf(z * 2.3f) * cosf(z * 0.7f);
        float rndB = cosf(z * 1.9f) * sinf(z * 1.1f);

        glPushMatrix();
            glColor3f(0.02f * gdim, 0.30f * gdim, 0.05f * gdim);
            glTranslatef(-2.6f + rndA, z + rndB * 0.5f, -0.04f);
            glRotatef(rndA * 40.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.07f + fabsf(rndB) * 0.05f, 0.07f + fabsf(rndB) * 0.05f, 0.09f + fabsf(rndA) * 0.06f);
            glutSolidCone(0.5, 1.2, 5, 1);
        glPopMatrix();

        glPushMatrix();
            glColor3f(0.02f * gdim, 0.28f * gdim, 0.05f * gdim);
            glTranslatef(1.75f - rndB * 0.30f, z + rndA * 0.5f, -0.04f);
            glRotatef(rndB * 40.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.07f + fabsf(rndA) * 0.05f, 0.07f + fabsf(rndA) * 0.05f, 0.09f + fabsf(rndB) * 0.06f);
            glutSolidCone(0.5, 1.2, 5, 1);
        glPopMatrix();
    }
    lightOff();
}

// -------------------------------------------------------------------
//  Buildings
// -------------------------------------------------------------------
// Brick wall block whose road-facing (-x) side has mortar lines
static void brickWallFace(float w, float d, float h, float baseR, float baseG, float baseB)
{
    glColor3f(baseR, baseG, baseB);
    glPushMatrix();
        glScalef(w, d, h);
        glutSolidCube(1.0);
    glPopMatrix();

    lightOff();
    glColor3f(baseR * 0.42f, baseG * 0.42f, baseB * 0.40f);
    int rows = 6, cols = 5;
    float faceX = -w / 2.0f - 0.006f;
    float rowH = h / rows, colW = d / cols;
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        for (int r = 1; r < rows; r++)
        {
            float fz = -h / 2.0f + rowH * r;
            glVertex3f(faceX, -d / 2.0f, fz);
            glVertex3f(faceX,  d / 2.0f, fz);
        }
        for (int r = 0; r < rows; r++)
        {
            float fz0 = -h / 2.0f + rowH * r, fz1 = fz0 + rowH;
            float offset = (r % 2 == 0) ? 0.0f : colW * 0.5f;
            for (int c = 0; c <= cols; c++)
            {
                float fy = -d / 2.0f + offset + colW * c;
                if (fy < -d / 2.0f || fy > d / 2.0f) continue;
                glVertex3f(faceX, fy, fz0);
                glVertex3f(faceX, fy, fz1);
            }
        }
    glEnd();
    lightOn();
}

static void windowColor()
{
    if (isNight) glColor3f(1.00f, 0.82f, 0.35f);   // lit windows
    else         glColor3f(0.62f, 0.80f, 0.92f);   // glass
}

void house()
{
    static const float palette[4][3] = {
        {0.95f, 0.90f, 0.80f}, {0.82f, 0.50f, 0.40f},
        {0.72f, 0.78f, 0.84f}, {0.88f, 0.72f, 0.50f}
    };
    int idx = 0;
    for (float z = -40.0f; z < JUNGLE_START; z += 9.6f)
    {
        int my = idx++;
        // leave room for the gas station, shops and hospital
        if (fabsf(z - 20.0f) < 9.0f || (z > 112.0f && z < 172.0f) || fabsf(z - 280.0f) < 10.0f) continue;
        if (!inView(z)) continue;

        bool twoFloor = (my % 2 == 0);
        float wr = palette[my % 4][0], wg = palette[my % 4][1], wb = palette[my % 4][2];

        glPushMatrix();
        glTranslatef(3.0f, z, 0.0f);
        lightOn();
        if (twoFloor)
        {
            glColor3f(wr, wg, wb);            box(0, 0, 0.30f, 1.0f, 1.0f, 1.0f);
            glColor3f(wr * .9f, wg * .9f, wb * .9f); box(0, 0, 1.10f, 0.82f, 0.82f, 0.70f);
            glColor3ub(120, 45, 35);
            glPushMatrix(); glTranslatef(0, 0, 1.44f); glRotatef(45, 0, 0, 1); glutSolidCone(0.80, 0.65, 4, 1); glPopMatrix();
            glColor3ub(60, 38, 20);           box(-0.50f, 0.0f, 0.22f, 0.05f, 0.20f, 0.40f);
            lightOff(); windowColor();
            box(-0.50f,  0.32f, 0.48f, 0.04f, 0.18f, 0.20f);
            box(-0.50f, -0.32f, 0.48f, 0.04f, 0.18f, 0.20f);
            box(-0.42f,  0.20f, 1.15f, 0.04f, 0.16f, 0.18f);
            box(-0.42f, -0.20f, 1.15f, 0.04f, 0.16f, 0.18f);
            lightOn();
        }
        else
        {
            glColor3f(wr, wg, wb);            box(0, 0, 0.22f, 1.05f, 1.0f, 0.55f);
            glColor3ub(70, 61, 46);
            glPushMatrix(); glTranslatef(0, 0, 0.49f); glRotatef(45, 0, 0, 1); glutSolidCone(0.95, 0.80, 4, 1); glPopMatrix();
            glColor3f(0.55f, 0.30f, 0.22f);   box(0.28f, 0.25f, 0.85f, 0.12f, 0.12f, 0.35f);   // chimney
            glColor3ub(60, 38, 20);           box(-0.53f, -0.20f, 0.17f, 0.05f, 0.22f, 0.34f);
            lightOff(); windowColor();
            box(-0.53f, 0.24f, 0.30f, 0.04f, 0.22f, 0.20f);
            lightOn();
        }
        glColor3f(0.10f, 0.45f, 0.12f);
        ball(-0.85f, -0.85f, 0.12f, 0.14f);
        lightOff();
        glPopMatrix();
    }
}

static void driveway(float z, float toX, float halfWidthZ)
{
    lightOff();
    float d = envDim;
    glColor3f(0.23f * d, 0.23f * d, 0.24f * d);
    glBegin(GL_QUADS);
        glVertex3f(1.12f, z - halfWidthZ, 0.03f);
        glVertex3f(toX,   z - halfWidthZ, 0.03f);
        glVertex3f(toX,   z + halfWidthZ, 0.03f);
        glVertex3f(1.12f, z + halfWidthZ, 0.03f);
    glEnd();
    glColor3f(0.9f * d, 0.9f * d, 0.9f * d);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex3f(1.12f, z - halfWidthZ, 0.04f); glVertex3f(toX, z - halfWidthZ, 0.04f);
        glVertex3f(1.12f, z + halfWidthZ, 0.04f); glVertex3f(toX, z + halfWidthZ, 0.04f);
    glEnd();
}

static void gasStation(float z, float xPos)
{
    glPushMatrix();
    glTranslatef(xPos, z, 0.0f);
    glScalef(2.0f, 2.0f, 1.4f);
    lightOn();

    glColor3f(0.85f, 0.15f, 0.10f);
    box(-0.30f, 0.0f, 0.85f, 1.3f, 1.6f, 0.06f);
    glColor3f(0.60f, 0.60f, 0.62f);
    for (int s = -1; s <= 1; s += 2)
        for (int t = -1; t <= 1; t += 2)
            box(-0.30f + s * 0.55f, t * 0.65f, 0.42f, 0.04f, 0.04f, 0.85f);

    for (int p = -1; p <= 1; p += 2)
    {
        glColor3f(0.90f, 0.90f, 0.90f); box(-0.30f, p * 0.30f, 0.16f, 0.10f, 0.10f, 0.32f);
        glColor3f(0.85f, 0.15f, 0.10f); box(-0.30f, p * 0.30f, 0.30f, 0.11f, 0.11f, 0.05f);
        glColor3f(0.08f, 0.08f, 0.08f); box(-0.23f, p * 0.30f, 0.20f, 0.015f, 0.10f, 0.015f);
    }

    glPushMatrix();
        glTranslatef(0.55f, 0.0f, 0.30f);
        brickWallFace(0.55f, 0.9f, 0.55f, 0.80f, 0.55f, 0.35f);
    glPopMatrix();
    glColor3f(0.30f, 0.30f, 0.32f);
    box(0.55f, 0.0f, 0.60f, 0.60f, 0.95f, 0.05f);
    glColor3f(0.55f, 0.80f, 0.85f);
    box(0.26f, 0.0f, 0.30f, 0.03f, 0.55f, 0.30f);

    glColor3f(0.20f, 0.20f, 0.22f);
    box(-1.10f, 0.90f, 0.55f, 0.04f, 0.04f, 1.10f);

    lightOff();
    glEnable(GL_BLEND);
    glColor4ub(250, 220, 20, isNight ? 255 : 220);
    box(-1.10f, 0.90f, 1.15f, 0.35f, 0.05f, 0.30f);
    glDisable(GL_BLEND);
    glPopMatrix();
}

static void shop(float z, int shopIdx, float xPos)
{
    static const float wallPalette[3][3] = {
        {0.82f, 0.35f, 0.28f}, {0.78f, 0.66f, 0.44f}, {0.58f, 0.58f, 0.62f}
    };
    static const float awningPalette[3][3] = {
        {0.90f, 0.20f, 0.20f}, {0.20f, 0.50f, 0.85f}, {0.15f, 0.60f, 0.25f}
    };
    int p = shopIdx % 3;
    bool useBrick = (p != 2);

    glPushMatrix();
    glTranslatef(xPos, z, 0.0f);
    glScalef(2.0f, 2.0f, 1.5f);
    lightOn();

    if (useBrick) brickWallFace(0.9f, 0.75f, 0.55f, wallPalette[p][0], wallPalette[p][1], wallPalette[p][2]);
    else { glColor3f(wallPalette[p][0], wallPalette[p][1], wallPalette[p][2]); box(0, 0, 0, 0.9f, 0.75f, 0.55f); }

    glColor3f(0.32f, 0.32f, 0.34f);
    box(0.0f, 0.0f, 0.29f, 0.95f, 0.80f, 0.04f);             // roof slab
    glColor3f(awningPalette[p][0], awningPalette[p][1], awningPalette[p][2]);
    box(-0.52f, 0.0f, 0.20f, 0.14f, 0.85f, 0.04f);          // awning

    lightOff(); windowColor();
    box(-0.46f, -0.14f, 0.05f, 0.03f, 0.30f, 0.22f);
    lightOn();
    glColor3ub(50, 35, 20);
    box(-0.46f, 0.30f, -0.03f, 0.03f, 0.16f, 0.30f);
    lightOff();
    glPopMatrix();
}

static void hospital(float z, float xPos)
{
    glPushMatrix();
    glTranslatef(xPos, z, 0.0f);
    lightOn();

    for (int f = 0; f < 3; f++)
    {
        glPushMatrix();
            glTranslatef(0.0f, 0.0f, 0.35f + f * 0.70f);
            brickWallFace(1.6f, 1.1f, 0.7f, 0.88f + 0.02f * f, 0.85f + 0.02f * f, 0.80f + 0.025f * f);
        glPopMatrix();
    }
    glColor3f(0.35f, 0.35f, 0.37f);
    box(0.0f, 0.0f, 2.12f, 1.65f, 1.15f, 0.06f);

    lightOff();
    windowColor();
    for (int f = 0; f < 3; f++)
        for (int w = -2; w <= 2; w++)
            box(-0.82f, w * 0.20f, 0.35f + f * 0.70f + 0.05f, 0.03f, 0.13f, 0.20f);

    glEnable(GL_BLEND);
    glColor4ub(220, 15, 15, isNight ? 255 : 230);
    box(-0.83f, 0.0f, 2.30f, 0.05f, 0.35f, 0.10f);
    box(-0.83f, 0.0f, 2.30f, 0.05f, 0.10f, 0.35f);
    glDisable(GL_BLEND);

    lightOn();
    glColor3ub(70, 130, 180);
    box(-0.82f, 0.0f, 0.12f, 0.05f, 0.30f, 0.24f);
    glColor3f(0.75f, 0.75f, 0.78f);
    box(-1.05f, 0.0f, 0.62f, 0.35f, 0.90f, 0.03f);
    lightOff();
    glPopMatrix();
}

void commercial()
{
    const float GAS_X = 4.2f, SHOP_X = 3.0f, HOSPITAL_X = 5.4f;

    if (inView(20.0f))  { driveway(20.0f, GAS_X, 1.3f);  gasStation(20.0f, GAS_X); }
    for (int s = 0; s < 4; s++)
    {
        float zc = 120.0f + s * 14.0f;
        if (!inView(zc)) continue;
        driveway(zc, SHOP_X, 0.85f);
        shop(zc, s, SHOP_X);
    }
    if (inView(280.0f)) { driveway(280.0f, HOSPITAL_X, 1.2f); hospital(280.0f, HOSPITAL_X); }
}

// -------------------------------------------------------------------
//  Trees
// -------------------------------------------------------------------
static void realTree(float x, float zAlong, float trunkH, float canopyR,
                     float r, float g, float b, bool jungle)
{
    float dim = isNight ? 0.6f : 1.0f;
    glPushMatrix();
        glTranslatef(x, zAlong, -0.05f);
        glColor3f(0.30f * dim, 0.19f * dim, 0.09f * dim);
        gluCylinder(treeQuad, trunkH * 0.09f, trunkH * 0.04f, trunkH, 7, 3);
        glTranslatef(0.0f, 0.0f, trunkH);

        glColor3f(r * dim, g * dim, b * dim);
        glutSolidSphere(canopyR, 10, 8);
        glColor3f(r * 0.90f * dim, g * 0.95f * dim, b * 0.90f * dim);
        glPushMatrix(); glTranslatef( canopyR * 0.55f,  canopyR * 0.10f, canopyR * 0.30f); glutSolidSphere(canopyR * 0.68f, 8, 7); glPopMatrix();
        glPushMatrix(); glTranslatef(-canopyR * 0.50f, -canopyR * 0.25f, canopyR * 0.22f); glutSolidSphere(canopyR * 0.60f, 8, 7); glPopMatrix();
        glColor3f(r * 1.12f * dim, g * 1.06f * dim, b * 1.10f * dim);
        glPushMatrix(); glTranslatef( canopyR * 0.05f,  canopyR * 0.45f, canopyR * 0.42f); glutSolidSphere(canopyR * 0.58f, 8, 7); glPopMatrix();
        if (jungle)
        {
            glColor3f(r * 1.2f * dim, g * 1.12f * dim, b * 1.18f * dim);
            glPushMatrix(); glTranslatef(0.0f, -canopyR * 0.1f, canopyR * 0.9f); glutSolidSphere(canopyR * 0.5f, 8, 7); glPopMatrix();
        }
    glPopMatrix();
}

static void middleTrees()
{
    for (float z = -50.0f; z < WORLD_LENGTH + 40.0f; z += 6.0f)
    {
        if (!inView(z)) continue;
        float heightMul = denseMode ? 1.3f : 1.0f;
        float h = (2.6f + 0.8f * sinf(z * 0.09f)) * heightMul;
        float xoff = -6.2f + 0.4f * sinf(z * 0.13f);
        realTree(xoff, z, h, 0.55f, 0.06f, 0.34f, 0.09f, true);
    }
}

void tree()
{
    lightOn();
    float heightMul = denseMode ? 1.55f : 1.15f;
    float step = denseMode ? 2.6f : 4.0f;

    for (float z = -40.0f; z < WORLD_LENGTH + 40.0f; z += step)
    {
        if (!inView(z)) continue;
        if (z < JUNGLE_START)
        {
            realTree(-1.55f, z,        0.62f * heightMul, 0.28f, 0.16f, 0.55f, 0.14f, false);
            realTree( 1.45f, z + 1.6f, 0.58f * heightMul, 0.26f, 0.18f, 0.60f, 0.15f, false);
        }
        else
        {
            float jitter = sinf(z * 0.37f) * 0.15f;
            float scaleL = 0.65f + 0.20f * sinf(z * 0.21f);
            float scaleR = 0.65f + 0.20f * cosf(z * 0.19f);
            realTree(-1.50f + jitter, z,        1.40f * scaleL * heightMul, 0.44f * scaleL, 0.05f, 0.30f, 0.06f, true);
            realTree( 1.45f - jitter, z + 2.0f, 1.40f * scaleR * heightMul, 0.44f * scaleR, 0.06f, 0.34f, 0.07f, true);
            if (fmodf(z, 12.0f) < 4.0f)
            {
                realTree(-1.95f, z + 1.0f, 1.10f * heightMul, 0.36f, 0.05f, 0.27f, 0.06f, true);
                realTree( 1.90f, z + 3.0f, 1.15f * heightMul, 0.38f, 0.06f, 0.31f, 0.07f, true);
            }
            if (denseMode && fmodf(z, 12.0f) < 4.0f)
            {
                realTree(-2.35f, z + 2.0f, 1.0f * heightMul, 0.32f, 0.05f, 0.27f, 0.06f, true);
                realTree( 2.30f, z + 0.5f, 1.05f * heightMul, 0.34f, 0.06f, 0.31f, 0.07f, true);
            }
        }
    }
    middleTrees();
    lightOff();
}

// -------------------------------------------------------------------
//  Obstacle cars
// -------------------------------------------------------------------
static void car(float laneX, float zp, float seed)
{
    float r = 0.55f + 0.40f * fabsf(sinf(seed));
    float g = 0.10f + 0.30f * fabsf(cosf(seed * 1.7f));
    float b = 0.15f + 0.45f * fabsf(sinf(seed * 0.6f));

    glPushMatrix();
        glTranslatef(laneX, zp, 0.0f);
        glScalef(CAR_SCALE, CAR_SCALE, CAR_SCALE);
        shadow(0.0f, 0.0f, 0.24f, 0.46f, 0.45f);
        lightOn();
        glColor3f(r, g, b);                     box(0.0f, 0.0f, 0.115f, 0.32f, 0.70f, 0.13f);   // body
        glColor3f(r * 0.85f, g * 0.85f, b * 0.85f); box(0.0f, -0.04f, 0.235f, 0.28f, 0.34f, 0.11f); // cabin
        glColor3f(0.10f, 0.16f, 0.24f);         box(0.0f, -0.04f, 0.245f, 0.285f, 0.30f, 0.075f); // glass
        glColor3f(0.04f, 0.04f, 0.04f);
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
            {
                glPushMatrix();
                    glTranslatef(sx * 0.165f, sy * 0.22f, 0.07f);
                    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
                    glutSolidTorus(0.025, 0.045, 8, 14);
                glPopMatrix();
            }
        lightOff();
        glColor3f(1.0f, 0.95f, 0.70f);                                     // headlights (far side)
        box(-0.10f, 0.352f, 0.13f, 0.06f, 0.012f, 0.03f);
        box( 0.10f, 0.352f, 0.13f, 0.06f, 0.012f, 0.03f);
        glColor3f(0.95f, 0.05f, 0.05f);                                    // tail lights (facing the rider)
        box(-0.11f, -0.352f, 0.15f, 0.07f, 0.012f, 0.035f);
        box( 0.11f, -0.352f, 0.15f, 0.07f, 0.012f, 0.035f);
    glPopMatrix();
}

void objectcube()
{
    for (int idx = 0; idx < obstacleCount; idx++)
    {
        if (!inView(obstacles[idx].pos)) continue;
        float laneX = (obstacles[idx].lane == 0) ? LANE_LEFT_X : LANE_RIGHT_X;
        car(laneX, obstacles[idx].pos, obstacles[idx].pos * 0.05f + obstacles[idx].lane * 2.0f);
    }
}

// -------------------------------------------------------------------
//  THE BIKE
//  Model space: origin on the ground under the middle of the bike,
//  +Y = up, +Z = FORWARD (nose), +X = side.  gamerbike() rotates this
//  so the nose points along the road, away from the camera.
// -------------------------------------------------------------------
static void wheel(float zc, float spin)
{
    glPushMatrix();
        glTranslatef(0.0f, 0.245f, zc);
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f);          // wheel axis -> model X
        glColor3f(0.04f, 0.04f, 0.04f);
        glutSolidTorus(0.045, 0.20, 12, 26);         // tyre
        glColor3f(0.65f, 0.65f, 0.70f);
        glutSolidTorus(0.012, 0.155, 8, 22);         // rim
        glColor3f(0.60f, 0.10f, 0.10f);
        glutSolidTorus(0.008, 0.095, 6, 18);         // brake disc
        glColor3f(0.75f, 0.75f, 0.78f);
        for (int s = 0; s < 6; s++)
        {
            glPushMatrix();
                glRotatef(s * 60.0f + spin, 0.0f, 0.0f, 1.0f);
                glTranslatef(0.0f, 0.078f, 0.0f);
                glScalef(0.022f, 0.15f, 0.012f);
                glutSolidCube(1.0);
            glPopMatrix();
        }
        glColor3f(0.82f, 0.82f, 0.86f);
        glutSolidSphere(0.03, 10, 8);
    glPopMatrix();
}

static void drawBikeModel(float spin)
{
    wheel(-0.36f, spin);
    wheel( 0.36f, spin);

    // ---- swingarm, rear shock ----
    glColor3f(0.12f, 0.12f, 0.14f);
    for (int s = -1; s <= 1; s += 2)
        beam(s * 0.055f, 0.245f, -0.36f, s * 0.055f, 0.28f, -0.06f, 0.016f, 0.020f);
    glColor3f(0.85f, 0.75f, 0.10f);
    beam(0.0f, 0.30f, -0.30f, 0.0f, 0.42f, -0.17f, 0.018f, 0.018f);

    // ---- engine block + cylinder + fins ----
    glColor3f(0.46f, 0.47f, 0.51f);
    box(0.0f, 0.21f, 0.00f, 0.17f, 0.17f, 0.26f);
    box(0.0f, 0.31f, 0.06f, 0.12f, 0.08f, 0.13f);
    glColor3f(0.62f, 0.62f, 0.66f);
    for (int f = 0; f < 4; f++) box(0.0f, 0.29f + f * 0.02f, 0.06f, 0.15f, 0.008f, 0.16f);

    // ---- frame ----
    glColor3f(0.10f, 0.10f, 0.11f);
    beam(0.0f, 0.48f, 0.24f, 0.0f, 0.38f, -0.25f, 0.022f, 0.022f);      // spine
    beam(0.0f, 0.47f, 0.25f, 0.0f, 0.20f, 0.05f, 0.020f, 0.020f);      // down tube
    beam(0.0f, 0.20f, 0.05f, 0.0f, 0.24f, -0.20f, 0.018f, 0.018f);     // belly

    // ---- tank + stripe ----
    glColor3f(0.80f, 0.05f, 0.06f);
    glPushMatrix();
        glTranslatef(0.0f, 0.49f, 0.07f);
        glScalef(0.17f, 0.14f, 0.30f);
        glutSolidSphere(0.5, 16, 12);
    glPopMatrix();
    glColor3f(0.96f, 0.96f, 0.96f);
    box(0.0f, 0.557f, 0.07f, 0.035f, 0.02f, 0.22f);

    // ---- seat, tail, tail light ----
    glColor3f(0.07f, 0.07f, 0.08f);
    box(0.0f, 0.415f, -0.19f, 0.15f, 0.05f, 0.30f);
    glColor3f(0.80f, 0.05f, 0.06f);
    glPushMatrix();
        glTranslatef(0.0f, 0.445f, -0.37f);
        glRotatef(12.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.12f, 0.05f, 0.22f);
        glutSolidCube(1.0);
    glPopMatrix();
    box(0.0f, 0.515f, 0.40f, 0.07f, 0.02f, 0.22f);                      // front fender

    // ---- front forks (raked back), sliders, clamp ----
    for (int s = -1; s <= 1; s += 2)
    {
        glColor3f(0.10f, 0.10f, 0.12f);
        beam(s * 0.055f, 0.245f, 0.36f, s * 0.055f, 0.35f, 0.32f, 0.024f, 0.024f);
        glColor3f(0.82f, 0.82f, 0.86f);
        beam(s * 0.055f, 0.35f, 0.32f, s * 0.055f, 0.50f, 0.25f, 0.016f, 0.016f);
    }
    glColor3f(0.20f, 0.20f, 0.22f);
    box(0.0f, 0.50f, 0.25f, 0.14f, 0.03f, 0.05f);
    beam(0.0f, 0.50f, 0.25f, 0.0f, 0.545f, 0.235f, 0.02f, 0.02f);

    // ---- handlebar, grips, levers, mirrors ----
    glColor3f(0.15f, 0.15f, 0.17f);
    beam(-0.17f, 0.56f, 0.22f, 0.17f, 0.56f, 0.22f, 0.013f, 0.013f);
    for (int s = -1; s <= 1; s += 2)
    {
        glColor3f(0.04f, 0.04f, 0.04f);
        beam(s * 0.13f, 0.56f, 0.22f, s * 0.20f, 0.56f, 0.22f, 0.02f, 0.02f);
        glColor3f(0.60f, 0.60f, 0.64f);
        beam(s * 0.12f, 0.565f, 0.235f, s * 0.15f, 0.565f, 0.29f, 0.006f, 0.004f);
        glColor3f(0.10f, 0.10f, 0.11f);
        beam(s * 0.15f, 0.57f, 0.21f, s * 0.17f, 0.66f, 0.18f, 0.005f, 0.005f);
        box(s * 0.17f, 0.675f, 0.18f, 0.06f, 0.04f, 0.012f);
    }

    // ---- headlight (housing + glowing lens) ----
    glColor3f(0.12f, 0.12f, 0.13f);
    glPushMatrix();
        glTranslatef(0.0f, 0.46f, 0.31f);
        glScalef(0.11f, 0.11f, 0.08f);
        glutSolidSphere(0.5, 14, 12);
    glPopMatrix();
    lightOff();
    glColor3f(1.0f, 0.97f, 0.80f);
    ball(0.0f, 0.46f, 0.345f, 0.034f);
    glColor3f(1.0f, 0.10f, 0.10f);                                      // tail light
    ball(0.0f, 0.47f, -0.50f, 0.022f);
    lightOn();

    // ---- exhaust ----
    glColor3f(0.82f, 0.82f, 0.86f);
    beam(0.09f, 0.19f, 0.12f, 0.10f, 0.09f, 0.00f, 0.022f, 0.022f);
    beam(0.10f, 0.09f, 0.00f, 0.115f, 0.12f, -0.42f, 0.032f, 0.040f);
    glColor3f(0.12f, 0.12f, 0.12f);
    ball(0.115f, 0.12f, -0.42f, 0.030f);

    // ---- footpegs ----
    glColor3f(0.20f, 0.20f, 0.22f);
    for (int s = -1; s <= 1; s += 2) box(s * 0.12f, 0.15f, -0.05f, 0.07f, 0.02f, 0.03f);

    // ================= RIDER (leaning forward, looking down the road) =================
    // legs
    for (int s = -1; s <= 1; s += 2)
    {
        glColor3f(0.11f, 0.13f, 0.20f);
        beam(s * 0.08f, 0.44f, -0.20f, s * 0.11f, 0.44f, 0.02f, 0.042f, 0.036f);   // thigh
        beam(s * 0.11f, 0.44f, 0.02f, s * 0.115f, 0.20f, -0.05f, 0.036f, 0.030f);  // shin
        ball(s * 0.11f, 0.44f, 0.02f, 0.038f);                                     // knee
        glColor3f(0.05f, 0.05f, 0.06f);
        box(s * 0.115f, 0.165f, -0.02f, 0.055f, 0.05f, 0.14f);                     // boot
    }
    // torso + shoulders
    glColor3f(0.14f, 0.14f, 0.20f);
    beam(0.0f, 0.47f, -0.19f, 0.0f, 0.79f, -0.03f, 0.065f, 0.085f);
    box(0.0f, 0.78f, -0.03f, 0.27f, 0.07f, 0.09f);
    glColor3f(0.80f, 0.05f, 0.06f);
    box(0.0f, 0.63f, -0.11f, 0.15f, 0.05f, 0.09f);                                 // jacket stripe
    // arms: shoulder -> elbow -> handlebar grip
    for (int s = -1; s <= 1; s += 2)
    {
        glColor3f(0.14f, 0.14f, 0.20f);
        beam(s * 0.13f, 0.77f, -0.03f, s * 0.19f, 0.66f, 0.06f, 0.030f, 0.028f);
        beam(s * 0.19f, 0.66f, 0.06f, s * 0.16f, 0.565f, 0.22f, 0.028f, 0.024f);
        ball(s * 0.19f, 0.66f, 0.06f, 0.030f);
        glColor3f(0.05f, 0.05f, 0.06f);
        ball(s * 0.16f, 0.565f, 0.22f, 0.032f);                                    // glove
    }
    // neck, head, helmet, visor
    glColor3f(0.94f, 0.78f, 0.62f);
    beam(0.0f, 0.80f, -0.03f, 0.0f, 0.86f, 0.0f, 0.03f, 0.03f);
    ball(0.0f, 0.90f, 0.02f, 0.055f);
    glColor3f(0.96f, 0.96f, 0.96f);
    glPushMatrix();
        glTranslatef(0.0f, 0.905f, 0.02f);
        glScalef(1.0f, 0.95f, 1.1f);
        glutSolidSphere(0.078, 16, 14);
    glPopMatrix();
    glColor3f(0.80f, 0.05f, 0.06f);
    box(0.0f, 0.977f, 0.02f, 0.022f, 0.014f, 0.14f);
    glColor3f(0.04f, 0.04f, 0.06f);
    box(0.0f, 0.905f, 0.088f, 0.11f, 0.038f, 0.03f);
}

void gamerbike()
{
    glPushMatrix();
        glTranslatef(xp, bikePos, 0.0f);
        shadow(0.0f, 0.0f, 0.28f, 0.75f, 0.50f);
        glRotatef(bikeLean, 0.0f, 1.0f, 0.0f);       // lean into the turn / crash roll
        glRotatef(180.0f, 0.0f, 0.0f, 1.0f);         // model +Z (nose) -> world +Y (down the road)
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);          // model +Y (up)   -> world +Z (up)
        glScalef(BIKE_SCALE, BIKE_SCALE, BIKE_SCALE);
        lightOn();
        drawBikeModel(wheelAngle);
        lightOff();
    glPopMatrix();
}

// -------------------------------------------------------------------
//  Night-time light pools (headlight + lamp posts)
// -------------------------------------------------------------------
void nightEffects()
{
    if (!isNight) return;
    lightOff();
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    for (float z = -38.0f; z < WORLD_LENGTH; z += 9.0f)
    {
        if (!inView(z)) continue;
        glBegin(GL_TRIANGLE_FAN);
            glColor4f(1.0f, 0.90f, 0.55f, 0.32f);
            glVertex3f(-0.58f, z, 0.045f);
            glColor4f(1.0f, 0.90f, 0.55f, 0.0f);
            for (int a = 0; a <= 16; a++)
                glVertex3f(-0.58f + cosf(a * 2.0f * PI / 16.0f) * 1.3f, z + sinf(a * 2.0f * PI / 16.0f) * 2.2f, 0.045f);
        glEnd();
    }

    if (gameState != ST_CRASH)
    {
        glBegin(GL_QUADS);
            glColor4f(1.0f, 0.96f, 0.75f, 0.50f);
            glVertex3f(xp - 0.10f, bikePos + 0.9f, 0.05f);
            glVertex3f(xp + 0.10f, bikePos + 0.9f, 0.05f);
            glColor4f(1.0f, 0.96f, 0.75f, 0.0f);
            glVertex3f(xp + 0.85f, bikePos + 9.0f, 0.05f);
            glVertex3f(xp - 0.85f, bikePos + 9.0f, 0.05f);
        glEnd();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// -------------------------------------------------------------------
//  Lighting / projection
// -------------------------------------------------------------------
void updateLighting()
{
    GLfloat lightPos[4], diffuse[4], ambient[4], spec[4];
    if (isNight)
    {
        lightPos[0] = 0.35f; lightPos[1] = 0.55f; lightPos[2] = 0.30f; lightPos[3] = 0.0f;
        diffuse[0] = 0.40f; diffuse[1] = 0.44f; diffuse[2] = 0.66f; diffuse[3] = 1.0f;
        ambient[0] = 0.15f; ambient[1] = 0.17f; ambient[2] = 0.27f; ambient[3] = 1.0f;
        spec[0] = 0.25f; spec[1] = 0.27f; spec[2] = 0.35f; spec[3] = 1.0f;
    }
    else
    {
        lightPos[0] = -0.45f; lightPos[1] = 0.65f; lightPos[2] = 0.35f; lightPos[3] = 0.0f;
        diffuse[0] = 0.85f; diffuse[1] = 0.82f; diffuse[2] = 0.74f; diffuse[3] = 1.0f;
        ambient[0] = 0.42f; ambient[1] = 0.43f; ambient[2] = 0.46f; ambient[3] = 1.0f;
        spec[0] = 0.65f; spec[1] = 0.65f; spec[2] = 0.60f; spec[3] = 1.0f;
    }
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
}

void setProjection()
{
    float fov = 45.0f + 12.0f * clampf((boostSmooth - 1.0f) / (BOOST_MAX - 1.0f), 0.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov, (double)winW / (double)(winH < 1 ? 1 : winH), 2.0, 1500.0);
    glMatrixMode(GL_MODELVIEW);
}

void handleResize(int w, int h)
{
    winW = w; winH = h < 1 ? 1 : h;
    glViewport(0, 0, w, winH);
    setProjection();
}

void initRendering()
{
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    GLfloat globalAmb[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);
    GLfloat matSpec[4] = { 0.22f, 0.22f, 0.22f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 40.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_LIGHTING);
    treeQuad = gluNewQuadric();
    gluQuadricNormals(treeQuad, GLU_SMOOTH);
}

// -------------------------------------------------------------------
//  HUD (2-D overlay)
// -------------------------------------------------------------------
static void drawText(float x, float y, const char* s, void* font)
{
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(font, *s);
}

static int textWidth(const char* s, void* font)
{
    int w = 0;
    for (; *s; s++) w += glutBitmapWidth(font, *s);
    return w;
}

static void drawBigText(const char* s, float cx, float cy, float sc, float r, float g, float b)
{
    float w = 0;
    for (const char* p = s; *p; p++) w += glutStrokeWidth(GLUT_STROKE_ROMAN, *p) * sc;
    glPushMatrix();
        glTranslatef(cx - w / 2.0f, cy, 0.0f);
        glScalef(sc, sc, 1.0f);
        glColor3f(r, g, b);
        glLineWidth(4.0f);
        for (const char* p = s; *p; p++) glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    glPopMatrix();
    glLineWidth(1.0f);
}

static void panel(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void drawHUD()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();

    char buf[96];
    void* f18 = GLUT_BITMAP_HELVETICA_18;
    void* f12 = GLUT_BITMAP_HELVETICA_12;

    // speed lines while boosting
    float boostAmt = clampf((boostSmooth - 1.0f) / (BOOST_MAX - 1.0f), 0.0f, 1.0f);
    if (boostAmt > 0.05f && gameState == ST_PLAY)
    {
        float cx = winW * 0.5f, cy = winH * 0.5f;
        float m = (winW < winH ? winW : winH) * 0.5f;
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (int i = 0; i < 36; i++)
        {
            float ang = i * 0.1745f + sinf(i * 12.9f) * 0.08f;
            float ph = fmodf(animTime * 3.0f + i * 0.37f, 1.0f);
            float r0 = m * (0.95f + ph * 0.45f);
            float r1 = r0 + m * (0.12f + 0.25f * boostAmt);
            glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
            glVertex2f(cx + cosf(ang) * r0 * 1.5f, cy + sinf(ang) * r0);
            glColor4f(1.0f, 1.0f, 1.0f, 0.35f * boostAmt);
            glVertex2f(cx + cosf(ang) * r1 * 1.5f, cy + sinf(ang) * r1);
        }
        glEnd();
        glLineWidth(1.0f);
    }

    // stats panel
    panel(12, winH - 96, 232, 84, 0.0f, 0.0f, 0.0f, 0.50f);
    glColor3f(1.0f, 0.90f, 0.20f);
    sprintf(buf, "SCORE     %d", score);
    drawText(24, winH - 36, buf, f18);
    glColor3f(0.90f, 0.95f, 1.0f);
    sprintf(buf, "DISTANCE  %d m", (int)bikePos);
    drawText(24, winH - 60, buf, f18);
    glColor3f(0.55f, 1.0f, 0.65f);
    sprintf(buf, "SPEED     %d km/h", (int)(curSpeed * 12.0f));
    drawText(24, winH - 84, buf, f18);

    // progress bar to the finish line
    float bx = winW * 0.30f, bw = winW * 0.40f, by = winH - 34;
    float prog = clampf(bikePos / FINISH_DISTANCE, 0.0f, 1.0f);
    panel(bx - 3, by - 3, bw + 6, 20, 0.0f, 0.0f, 0.0f, 0.5f);
    panel(bx, by, bw * prog, 14, 0.25f, 0.80f, 0.35f, 0.95f);
    glColor3f(1, 1, 1);
    drawText(bx - 3, by - 18, "START", f12);
    drawText(bx + bw - textWidth("FINISH", f12) + 3, by - 18, "FINISH", f12);

    // boost / throttle
    if (boostAmt > 0.3f)
    {
        glColor3f(1.0f, 0.55f, 0.10f);
        drawText(winW * 0.5f - textWidth("BOOST!", f18) / 2, by - 44, "BOOST!", f18);
    }
    panel(winW - 172, 14, 158, 58, 0.0f, 0.0f, 0.0f, 0.45f);
    glColor3f(0.9f, 0.9f, 0.9f);
    drawText(winW - 162, 52, "THROTTLE", f12);
    panel(winW - 162, 36, 138, 8, 0.3f, 0.3f, 0.3f, 0.8f);
    panel(winW - 162, 36, 138 * (throttle - 0.5f) / 0.9f, 8, 0.25f, 0.75f, 1.0f, 0.95f);
    glColor3f(0.9f, 0.9f, 0.9f);
    drawText(winW - 162, 20, isNight ? "NIGHT" : "DAY", f12);

    glColor3f(1, 1, 1);
    drawText(14, 14, "ARROWS steer/throttle   SHIFT boost   P pause   N/D/T night-day   +/- zoom   R restart", f12);

    // overlays
    if (gameState == ST_CRASH && crashTimer > 0.6f)
    {
        panel(0, 0, (float)winW, (float)winH, 0.25f, 0.0f, 0.0f, 0.60f);
        drawBigText("GAME OVER", winW * 0.5f, winH * 0.56f, 0.9f * (winW / 800.0f), 1.0f, 0.25f, 0.20f);
        sprintf(buf, "You hit a car!   Score: %d   Distance: %d m", score, (int)bikePos);
        glColor3f(1, 1, 1);
        drawText(winW * 0.5f - textWidth(buf, f18) / 2, winH * 0.44f, buf, f18);
        drawText(winW * 0.5f - textWidth("Press R to try again  -  E to exit", f18) / 2, winH * 0.36f,
                 "Press R to try again  -  E to exit", f18);
    }
    else if (gameState == ST_CRASH)
    {
        float a = clampf(0.6f - crashTimer * 1.0f, 0.0f, 0.6f);
        panel(0, 0, (float)winW, (float)winH, 1.0f, 0.0f, 0.0f, a);
    }
    else if (gameState == ST_WIN && finishTimer > 0.4f)
    {
        panel(0, 0, (float)winW, (float)winH, 0.0f, 0.05f, 0.18f, 0.55f);
        drawBigText("YOU WIN!", winW * 0.5f, winH * 0.56f, 1.0f * (winW / 800.0f), 1.0f, 0.90f, 0.20f);
        sprintf(buf, "Finish line reached!   Score: %d", score);
        glColor3f(1, 1, 1);
        drawText(winW * 0.5f - textWidth(buf, f18) / 2, winH * 0.44f, buf, f18);
        drawText(winW * 0.5f - textWidth("Press R to race again  -  E to exit", f18) / 2, winH * 0.36f,
                 "Press R to race again  -  E to exit", f18);
    }
    else if (paused)
    {
        panel(0, 0, (float)winW, (float)winH, 0.0f, 0.0f, 0.0f, 0.35f);
        drawBigText("PAUSED", winW * 0.5f, winH * 0.52f, 0.7f * (winW / 800.0f), 1.0f, 1.0f, 1.0f);
        glColor3f(1, 1, 1);
        drawText(winW * 0.5f - textWidth("Press P to resume", f18) / 2, winH * 0.42f, "Press P to resume", f18);
    }

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// -------------------------------------------------------------------
//  Scene
// -------------------------------------------------------------------
void drawScene()
{
    glClearColor(sky_red, sky_green, sky_blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setProjection();

    glEnable(GL_FOG);
    GLfloat fogColor[4] = { sky_red, sky_green, sky_blue, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, fogNear);
    glFogf(GL_FOG_END, fogFar);
    glHint(GL_FOG_HINT, GL_NICEST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (gameState == ST_CRASH && crashTimer < 0.5f)          // little camera shake on impact
    {
        float amp = 0.20f * (0.5f - crashTimer);
        glTranslatef(sinf(crashTimer * 90.0f) * amp, cosf(crashTimer * 77.0f) * amp, 0.0f);
    }
    glTranslatef(0.0f, 0.0f, camDistance);

    updateLighting();
    glDisable(GL_LIGHTING);
    sky();

    glPushMatrix();
        glRotatef(80.0f, -1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, BIKE_WORLD_Y - bikePos, 0.0f);   // scroll the world; the bike sits at bikePos

        ground();
        road();
        mountains();
        tree();
        house();
        commercial();
        roadside();
        finishLine();
        objectcube();
        nightEffects();
        gamerbike();
    glPopMatrix();

    drawHUD();
    glutSwapBuffers();
}

// -------------------------------------------------------------------
//  Game logic
// -------------------------------------------------------------------
void resetGame()
{
    srand((unsigned)time(NULL));
    buildObstacles();
    gameState = ST_PLAY;
    bikePos = 0.0f; xp = LANE_LEFT_X; carpos = 0;
    bikeLean = 0.0f; wheelAngle = 0.0f; curSpeed = 0.0f;
    throttle = 1.0f; boostSmooth = 1.0f;
    score = 0; crashTimer = 0.0f; finishTimer = 0.0f;
    playTime = 0.0f; denseMode = false; paused = false;
}

static void stepGame(float dt)
{
    // smooth lane change + lean
    float targetX = (carpos == 0) ? LANE_LEFT_X : LANE_RIGHT_X;
    float dx = targetX - xp;
    xp += dx * clampf(dt * 12.0f, 0.0f, 1.0f);

    if (gameState == ST_PLAY)
    {
        float leanTarget = clampf(dx * 45.0f, -22.0f, 22.0f);
        bikeLean += (leanTarget - bikeLean) * clampf(dt * 10.0f, 0.0f, 1.0f);

        // speed: base ramps up with distance, throttle arrows, SHIFT boost
        float boostTarget = (shiftHeld || boostLatch) ? BOOST_MAX : 1.0f;
        boostSmooth += (boostTarget - boostSmooth) * clampf(dt * 3.5f, 0.0f, 1.0f);
        float base = 5.0f + 4.0f * clampf(bikePos / FINISH_DISTANCE, 0.0f, 1.0f);
        curSpeed = base * throttle * boostSmooth;

        bikePos += curSpeed * dt;
        wheelAngle = fmodf(wheelAngle + (curSpeed * dt) / (0.245f * BIKE_SCALE) * 57.29578f, 360.0f);
        playTime += dt;
        denseMode = (playTime > 30.0f);

        for (int i = 0; i < obstacleCount; i++)
        {
            float carX = (obstacles[i].lane == 0) ? LANE_LEFT_X : LANE_RIGHT_X;
            float d = bikePos - obstacles[i].pos;
            bool lateralHit = fabsf(xp - carX) < COLLISION_LATERAL;

            if (fabsf(d) < COLLISION_WINDOW && lateralHit)
            {
                gameState = ST_CRASH;
                crashTimer = 0.0f;
                crashDir = (xp > carX) ? 1.0f : -1.0f;
                break;
            }
            if (!obstacles[i].scored && d > COLLISION_WINDOW)
            {
                obstacles[i].scored = true;
                if (!lateralHit) score += 5;              // dodged that car cleanly
            }
        }

        if (gameState == ST_PLAY && bikePos >= FINISH_DISTANCE)
        {
            gameState = ST_WIN;
            finishTimer = 0.0f;
        }
    }
    else if (gameState == ST_CRASH)
    {
        crashTimer += dt;
        curSpeed *= expf(-4.0f * dt);
        bikePos += curSpeed * dt;
        boostSmooth += (1.0f - boostSmooth) * clampf(dt * 5.0f, 0.0f, 1.0f);
        bikeLean = crashDir * clampf(crashTimer * 260.0f, 0.0f, 84.0f);
    }
    else // ST_WIN: coast to a stop past the line
    {
        finishTimer += dt;
        curSpeed *= expf(-1.6f * dt);
        bikePos += curSpeed * dt;
        boostSmooth += (1.0f - boostSmooth) * clampf(dt * 3.0f, 0.0f, 1.0f);
        bikeLean += (0.0f - bikeLean) * clampf(dt * 6.0f, 0.0f, 1.0f);
        wheelAngle = fmodf(wheelAngle + (curSpeed * dt) / (0.245f * BIKE_SCALE) * 57.29578f, 360.0f);
    }
}

void update(int value)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTimeMs) / 1000.0f;
    lastTimeMs = now;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

#ifdef _WIN32
    shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
    shiftHeld = (now - shiftSeenMs) < 300;
#endif

    if (!paused)
    {
        animTime += dt;
        cloudDrift += dt * 0.5f;
        stepGame(dt);
    }
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// -------------------------------------------------------------------
//  Input
// -------------------------------------------------------------------
static void noteShift()
{
    if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) shiftSeenMs = glutGet(GLUT_ELAPSED_TIME);
}

void keyboard(unsigned char key, int x, int y)
{
    noteShift();
    if (key >= 'A' && key <= 'Z') key = (unsigned char)(key - 'A' + 'a');

    if      (key == 'n') setNight();
    else if (key == 'd') setDay();
    else if (key == 't') { if (isNight) setDay(); else setNight(); }
    else if (key == 'e' || key == 27) exit(0);
    else if (key == 'p') paused = !paused;
    else if (key == 'r') resetGame();
    else if (key == 'b') boostLatch = !boostLatch;
    else if (key == '+' || key == '=')
    {
        camDistance += 0.6f;
        if (camDistance > CAM_DISTANCE_MAX) camDistance = CAM_DISTANCE_MAX;
    }
    else if (key == '-' || key == '_')
    {
        camDistance -= 0.6f;
        if (camDistance < CAM_DISTANCE_MIN) camDistance = CAM_DISTANCE_MIN;
    }
}

void specialKeys(int key, int x, int y)
{
    noteShift();
    switch (key)
    {
    case GLUT_KEY_RIGHT: carpos = 1; break;
    case GLUT_KEY_LEFT:  carpos = 0; break;
    case GLUT_KEY_UP:    throttle = clampf(throttle + 0.1f, 0.6f, 1.4f); break;
    case GLUT_KEY_DOWN:  throttle = clampf(throttle - 0.1f, 0.6f, 1.4f); break;
    default: break;
    }
}

// -------------------------------------------------------------------
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Moto Racer 3D");

    initRendering();
    resetGame();
    lastTimeMs = glutGet(GLUT_ELAPSED_TIME);

    glutDisplayFunc(drawScene);
    glutReshapeFunc(handleResize);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16, update, 0);
    glutMainLoop();
    return 0;
}
