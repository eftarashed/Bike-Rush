#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include<iostream>
#include<sstream>
#include <string>
#include<string.h>
#include<windows.h>
#include<mmsystem.h>

using namespace std;

static float xp=-.55;
int static crspeed = 60;
float crmove = 4;

const float LANE_LEFT_X  = -0.50f;
const float LANE_RIGHT_X =  0.50f;
const float COLLISION_WINDOW = 1.4f; // along-track half-width of a car's hit box
const int   MAX_OBSTACLES = 64;
struct Obstacle { float pos; int lane; }; // lane: 0 = left (matches carpos==0), 1 = right (carpos==1)
Obstacle obstacles[MAX_OBSTACLES];
bool obstacleScored[MAX_OBSTACLES];
int obstacleCount = 0;
float static tpx= .15;
int static carpos= 0;
float view = 10.0;
int static score= 0;
int static totalMeter = 0;
char quote[6][80];
int numberOfQuotes = 0, i;
int static carspeed= 45;
float static sky_red=0;
float static sky_green= .8;
float sky_blue= 1.0;
int roadlight = 50;

// ---------- Day / Night state ----------
bool isNight = false;
float zenith_r = 0.10f, zenith_g = 0.45f, zenith_b = 0.95f; // top-of-sky color, blends toward sky_red/green/blue at horizon

float fogNear = 250.0f, fogFar = 650.0f;
bool autoCycleEnabled = true;      // auto day/night cycling; a manual n/d/t press turns this off
const int DAYNIGHT_PERIOD_MS = 45000; // 45s day, 45s night

// ---------- Distance where the roadside scenery turns into jungle ----------
const float JUNGLE_START = 300.0f;

const float FINISH_DISTANCE = 520.0f;
const float WORLD_LENGTH = FINISH_DISTANCE + 80.0f; // everything (road, trees, hills..) stops shortly past the finish
bool raceFinished = false;

// ---------- Pause ----------
bool paused = false;

// ---------- Elapsed play time, used to ramp up density/height after 30s ----------
int gameStartTimeMs = 0;
bool denseMode = false; // true once 30s of play have elapsed: denser/taller trees & hills

// ---------- Speed ramps up with distance travelled ----------
float baseCrSpeed = 0.10f; // per-tick scroll speed at the start of the race

// ---------- Simple fixed starfield, generated once ----------
struct Star { float x, y; };
Star stars[70];
bool starsReady = false;
void initStars()
{
    if (starsReady) return;
    for (int s = 0; s < 70; s++)
    {
        stars[s].x = -34.0f + (rand() % 680) / 10.0f;
        stars[s].y = 7.0f + (rand() % 230) / 10.0f;
    }
    starsReady = true;
}

// Shared quadric object for tapered trunks (created lazily on first use)
GLUquadric* treeQuad = NULL;

// Builds the obstacle list once, at startup. Positions/lanes here are the
// ONLY thing collision() checks against, and objectcube() draws exactly
// this list — so a car is always on screen wherever it can hit you.
void buildObstacles()
{
    obstacleCount = 0;
    for (float zpp = -20.0f; zpp < WORLD_LENGTH && obstacleCount < MAX_OBSTACLES - 2; zpp += 70.0f)
    {
        obstacles[obstacleCount].pos = zpp;          obstacles[obstacleCount].lane = 0; obstacleScored[obstacleCount] = false; obstacleCount++;
        obstacles[obstacleCount].pos = zpp + 35.0f;  obstacles[obstacleCount].lane = 1; obstacleScored[obstacleCount] = false; obstacleCount++;
    }
}


void sprint( float x, float y, string st)
{
    int l,i;
//    l=strlen( st ); // see how many characters are in text string.
    glColor3f(0.0,0.0,0.0);
    //glDisable(GL_LIGHTING);
    glRasterPos2f( x, y); // location to start printing text
    for( i=0; i <  st.length(); i++) // loop until i is greater then l
    {
       glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, st[i]);

    }


}

void keyboardown(int key, int x, int y)
{
	switch (key){

	case GLUT_KEY_RIGHT:
	    xp=.55;
	    carpos=1;
      break;



	case GLUT_KEY_LEFT:
	    xp=-.55;
	    carpos=0;
	    break;


    case GLUT_KEY_UP:
        if (crspeed>5){
        crspeed-=5;
        carspeed+=5;
        }

        else
            crspeed=crspeed;
        break;


    case GLUT_KEY_DOWN:
       if (crspeed<60){
        crspeed+=5;
        carspeed-=5;
        }

        else
            crspeed=crspeed;
        break;


	default:
		break;

	}
}

//Extra Display Function
void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, 1.0, 1.0, 3200);
	glMatrixMode(GL_MODELVIEW);
}

GLfloat UpwardsScrollVelocity = -1.0;

void timeTick(void)
{
	if (UpwardsScrollVelocity< -1)
		view -= 0.0011;
	if (view < 0) { view = 2; UpwardsScrollVelocity = -1.0; }
	//  exit(0);
	UpwardsScrollVelocity -= 0.2;
	glutPostRedisplay();
}


void RenderToDisplay()
{
	int l, lenghOfQuote, i;

	glTranslatef(0.0, 0.0, 0.0);
	glRotatef(-20, 1.0, 0.0, 0.0);
	glScalef(0.05, 0.05, 0.05);

	for (l = 0; l<numberOfQuotes; l++)
	{
		lenghOfQuote = (int)strlen(quote[l]);
		glPushMatrix();
		glTranslatef(-(lenghOfQuote * 37), (l * 200), 0.0);
		for (i = 0; i < lenghOfQuote; i++)
		{
			glColor3f((UpwardsScrollVelocity / 10) + 300 + (l * 10), (UpwardsScrollVelocity / 10) + 300 + (l * 10), 0.0);
			glutStrokeCharacter(GLUT_STROKE_ROMAN, quote[l][i]);
		}
		glPopMatrix();
	}
}




void myDisplayFunction(void)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	gluLookAt(0.0, 30.0, 100.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	RenderToDisplay();
	glutSwapBuffers();
}

int winner(char a)
{


	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 500);
	glutCreateWindow("GAME OVER");
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glLineWidth(3);
	//char c=char(score);
	strcpy(quote[1], "Game Over");
	strcpy(quote[0], "OOPSS:-)!!!");
	numberOfQuotes = 5;
	glutDisplayFunc(myDisplayFunction);
	glutReshapeFunc(reshape);
	glutIdleFunc(timeTick);
	glutMainLoop();

	return 0;
}

// Shown when the player reaches the end of the road (see FINISH_DISTANCE / finishLine()).
int finishRace(char a)
{
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 500);
	glutCreateWindow("FINISH");
	glClearColor(0.0, 0.05, 0.18, 1.0);
	glLineWidth(3);
	strcpy(quote[1], "You Win!");
	strcpy(quote[0], "YEYYYY.........");
	numberOfQuotes = 5;
	glutDisplayFunc(myDisplayFunction);
	glutReshapeFunc(reshape);
	glutIdleFunc(timeTick);
	glutMainLoop();

	return 0;
}

void setNight()
{
    sky_red=0.02f; sky_green=0.06f; sky_blue=0.14f;
    zenith_r=0.01f; zenith_g=0.01f; zenith_b=0.05f;
    roadlight = 255;
    isNight = true;
    fogNear = 140.0f;
    fogFar  = 420.0f;
}


void setDay()
{
    sky_red=0; sky_green=0.8; sky_blue=1.0;
    zenith_r=0.10f; zenith_g=0.45f; zenith_b=0.95f;
    roadlight = 50;
    isNight = false;
    fogNear = 250.0f;
    fogFar  = 650.0f;
}

  void keyboard(unsigned char key, int x, int y)
{
    if(key=='n')
    {
        setNight();
        autoCycleEnabled = false; // manual override
    }

    else if(key=='d')
    {
        setDay();
        autoCycleEnabled = false; // manual override
    }

    else if(key=='t')
    {
        // toggle day/night with a single key
        if (isNight) setDay(); else setNight();
        autoCycleEnabled = false; // manual override
    }

    else if(key=='e')
    {
        exit(1);
    }

    else if(key=='p' || key=='P')
    {
        paused = !paused;
    }

}




// Collision now checks the bike's actual travelled distance against the
// SAME list of car positions that objectcube() draws (built once in
// buildObstacles()). It only returns true when the bike is in the same
// lane as a car AND within that car's along-track footprint — i.e. only
// when there is really a car there to hit.
bool collision()
{
    float distanceTravelled = -crmove;

    for (int idx = 0; idx < obstacleCount; idx++)
    {
        if (obstacles[idx].lane != carpos) continue; // different lane: nothing to hit
        float d = distanceTravelled - obstacles[idx].pos;
        if (d > -COLLISION_WINDOW && d < COLLISION_WINDOW)
            return true;
    }
    return false;
}


void GameScore()
{
    float distanceTravelled = -crmove;
    totalMeter = (int)distanceTravelled;

    for (int idx = 0; idx < obstacleCount; idx++)
    {
        if (obstacleScored[idx]) continue;
        if (distanceTravelled > obstacles[idx].pos + COLLISION_WINDOW)
        {
            obstacleScored[idx] = true;
            if (obstacles[idx].lane != carpos) score += 5; // dodged that car cleanly
        }
    }
}
//Initializes 3D rendering
void initRendering()
{
    glEnable(GL_DEPTH_TEST);

    // Disable all OpenGL lighting
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_COLOR_MATERIAL);

    glEnable(GL_NORMALIZE);
}

// Sets GL_LIGHT0's position/color for the current day/night state. Called
// once per frame, in eye space (right after the camera transform), so it
// stays fixed for the rest of the frame no matter what other matrices get
// pushed while drawing the scene.
void updateLighting()
{
    GLfloat lightPos[4];
    GLfloat diffuse[4];
    GLfloat ambient[4];

    if (isNight)
    {
        // moonlight: dim, cool blue-white, low ambient so lit objects go dark but not pitch black
        lightPos[0]=0.35f; lightPos[1]=0.55f; lightPos[2]=0.30f; lightPos[3]=0.0f; // directional
        diffuse[0]=0.32f; diffuse[1]=0.35f; diffuse[2]=0.50f; diffuse[3]=1.0f;
        ambient[0]=0.10f; ambient[1]=0.11f; ambient[2]=0.16f; ambient[3]=1.0f;
    }
    else
    {
        // sunlight: bright and warm
        lightPos[0]=-0.45f; lightPos[1]=0.65f; lightPos[2]=0.35f; lightPos[3]=0.0f;
        diffuse[0]=1.0f; diffuse[1]=0.97f; diffuse[2]=0.88f; diffuse[3]=1.0f;
        ambient[0]=0.45f; ambient[1]=0.45f; ambient[2]=0.42f; ambient[3]=1.0f;
    }

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
}

//Called when the window is resized
void handleResize(int w, int h) {
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, (double)w / (double)h, 1.0, 1500.0);
}
float rtri =0;
float _angle = 0.0;
float _cameraAngle = 0.0;
float _ang_tri = 0.0;


float pedalAngle = 0;

void gamerbike()
    {
        pedalAngle = _ang_tri * 4.5f; // full crank rotation per pedal cycle
        float legLift = sin(pedalAngle * 3.14159f / 180.0f) * 0.05f; // simple up/down leg animation

        glPushMatrix();
            glTranslatef(xp, -1.0, 3.5);
            glRotatef(-10, -1.0, 0.0, 0.0);

            // ---------- WHEELS ----------
            // Rear wheel (tire + rim + spokes, for a more bike-like silhouette)
            glPushMatrix();
                glTranslatef(0.0, 0.0, -0.26);
                glRotatef(90, 0.0, 1.0, 0.0);
                glColor3f(0.05, 0.05, 0.05);
                glutSolidTorus(0.028, 0.18, 10, 22);
                glColor3f(0.75, 0.75, 0.78);
                glutWireTorus(0.014, 0.15, 6, 18);
                // spokes, fanned around the hub
                glColor3f(0.82, 0.82, 0.85);
                for (int s = 0; s < 8; s++)
                {
                    glPushMatrix();
                        glRotatef(s * 45.0f, 0.0, 0.0, 1.0);
                        glTranslatef(0.0, 0.09, 0.0);
                        glScalef(0.006f, 0.18f, 0.006f);
                        glutSolidCube(1.0f);
                    glPopMatrix();
                }
            glPopMatrix();

            // Front wheel (tire + rim + spokes)
            glPushMatrix();
                glTranslatef(0.0, 0.0, 0.30);
                glRotatef(90, 0.0, 1.0, 0.0);
                glColor3f(0.05, 0.05, 0.05);
                glutSolidTorus(0.028, 0.18, 10, 22);
                glColor3f(0.75, 0.75, 0.78);
                glutWireTorus(0.014, 0.15, 6, 18);
                glColor3f(0.82, 0.82, 0.85);
                for (int s = 0; s < 8; s++)
                {
                    glPushMatrix();
                        glRotatef(s * 45.0f + _ang_tri * 6.0f, 0.0, 0.0, 1.0); // spins with pedal motion
                        glTranslatef(0.0, 0.09, 0.0);
                        glScalef(0.006f, 0.18f, 0.006f);
                        glutSolidCube(1.0f);
                    glPopMatrix();
                }
            glPopMatrix();

            // ---------- FRAME ----------
            glColor3f(0.85, 0.08, 0.08); // frame red

            // Down tube: rear hub area up toward the head tube
            glPushMatrix();
                glTranslatef(0.0, 0.06, 0.02);
                glRotatef(58, 1.0, 0.0, 0.0);
                glScalef(0.045, 0.045, 0.62);
                glutSolidCube(0.5);
            glPopMatrix();

            // Top tube: seat area to handlebar area
            glPushMatrix();
                glTranslatef(0.0, 0.245, 0.04);
                glRotatef(8, 1.0, 0.0, 0.0);
                glScalef(0.04, 0.04, 0.5);
                glutSolidCube(0.5);
            glPopMatrix();

            // Seat tube: bottom bracket up to the seat
            glPushMatrix();
                glTranslatef(0.0, 0.15, -0.16);
                glRotatef(-16, 1.0, 0.0, 0.0);
                glScalef(0.04, 0.32, 0.04);
                glutSolidCube(0.5);
            glPopMatrix();

            // Chainstay: rear hub to bottom bracket
            glPushMatrix();
                glTranslatef(0.0, 0.06, -0.14);
                glRotatef(0, 1.0, 0.0, 0.0);
                glScalef(0.04, 0.04, 0.24);
                glutSolidCube(0.5);
            glPopMatrix();

            // Front fork: front hub up to handlebar stem
            glColor3f(0.6, 0.6, 0.65);
            glPushMatrix();
                glTranslatef(0.0, 0.13, 0.28);
                glRotatef(-16, 1.0, 0.0, 0.0);
                glScalef(0.035, 0.30, 0.035);
                glutSolidCube(0.5);
            glPopMatrix();

            // ---------- SEAT ----------
            glColor3f(0.08, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.26, -0.19);
                glRotatef(-6, 1.0, 0.0, 0.0);
                glScalef(0.09, 0.035, 0.20);
                glutSolidCube(0.5);
            glPopMatrix();

            // ---------- HANDLEBAR ----------
            glColor3f(0.15, 0.15, 0.17);
            glPushMatrix();
                glTranslatef(0.0, 0.27, 0.29);
                glScalef(0.32, 0.03, 0.03);
                glutSolidCube(0.5);
            glPopMatrix();
            // grips
            glPushMatrix();
                glColor3f(0.05, 0.05, 0.05);
                glTranslatef(0.10, 0.27, 0.29);
                glScalef(0.05, 0.045, 0.045);
                glutSolidCube(0.5);
            glPopMatrix();
            glPushMatrix();
                glColor3f(0.05, 0.05, 0.05);
                glTranslatef(-0.10, 0.27, 0.29);
                glScalef(0.05, 0.045, 0.045);
                glutSolidCube(0.5);
            glPopMatrix();

            // ---------- CHAIN (rear hub to crank, a thin dark loop) ----------
            glColor3f(0.08, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.06, -0.14);
                glScalef(0.012f, 0.012f, 0.24f);
                glutSolidCube(0.5);
            glPopMatrix();

            // ---------- CRANK / PEDALS ----------
            glColor3f(0.35, 0.35, 0.38);
            glPushMatrix();
                glTranslatef(0.0, 0.06, -0.02);
                glRotatef(pedalAngle, 1.0, 0.0, 0.0);
                // pedal arm 1 + pedal
                glPushMatrix();
                    glScalef(0.03, 0.13, 0.03);
                    glutSolidCube(0.5);
                glPopMatrix();
                glPushMatrix();
                    glTranslatef(0.10, 0.065, 0.0);
                    glColor3f(0.1, 0.1, 0.1);
                    glScalef(0.10, 0.02, 0.06);
                    glutSolidCube(0.5);
                glPopMatrix();
                // pedal arm 2 (opposite side, 180 degrees out of phase) + pedal
                glColor3f(0.35, 0.35, 0.38);
                glPushMatrix();
                    glRotatef(180, 1.0, 0.0, 0.0);
                    glScalef(0.03, 0.13, 0.03);
                    glutSolidCube(0.5);
                glPopMatrix();
                glPushMatrix();
                    glRotatef(180, 1.0, 0.0, 0.0);
                    glTranslatef(0.10, 0.065, 0.0);
                    glColor3f(0.1, 0.1, 0.1);
                    glScalef(0.10, 0.02, 0.06);
                    glutSolidCube(0.5);
                glPopMatrix();
            glPopMatrix();

            // ---------- RIDER ----------
            // Torso, leaning forward toward the handlebar
            glColor3f(0.15, 0.35, 0.85); // jersey
            glPushMatrix();
                glTranslatef(0.0, 0.42, -0.08);
                glRotatef(38, 1.0, 0.0, 0.0);
                glScalef(0.14, 0.30, 0.10);
                glutSolidCube(0.5);
            glPopMatrix();

            // Head
            glColor3f(0.94, 0.78, 0.62); // skin tone
            glPushMatrix();
                glTranslatef(0.0, 0.60, 0.12);
                glutSolidSphere(0.06, 14, 14);
            glPopMatrix();

            // Helmet
            glColor3f(0.95, 0.15, 0.15);
            glPushMatrix();
                glTranslatef(0.0, 0.635, 0.115);
                glScalef(1.0, 0.8, 1.05);
                glutSolidSphere(0.066, 14, 14);
            glPopMatrix();

            // Arms: shoulder down to the handlebar grips at (~0.10, 0.27, 0.29).
            // Lengthened and re-angled so they actually reach the grips instead
            // of stopping short in mid-air.
            glColor3f(0.94, 0.78, 0.62);
            glPushMatrix();
                glTranslatef(0.09, 0.385, 0.17);
                glRotatef(70, 1.0, 0.0, 0.0);
                glScalef(0.032, 0.46, 0.032);
                glutSolidCube(0.5);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(-0.09, 0.385, 0.17);
                glRotatef(70, 1.0, 0.0, 0.0);
                glScalef(0.032, 0.46, 0.032);
                glutSolidCube(0.5);
            glPopMatrix();

            // Legs: hip (just below the seat, ~y=0.30) down to the pedal
            // (~y=0.065, z=-0.02, offset by the crank's rotation). Lengthened
            // and centered on the hip-to-pedal midpoint so they connect the
            // rider to the crank instead of floating above it.
            glColor3f(0.10, 0.10, 0.12); // riding pants
            glPushMatrix();
                glTranslatef(0.06, 0.185 + legLift, -0.09);
                glRotatef(-12, 1.0, 0.0, 0.0);
                glScalef(0.035, 0.46, 0.035);
                glutSolidCube(0.5);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(-0.06, 0.185 - legLift, -0.09);
                glRotatef(-12, 1.0, 0.0, 0.0);
                glScalef(0.035, 0.46, 0.035);
                glutSolidCube(0.5);
            glPopMatrix();

            // Feet, at the bottom of each leg, resting on the pedals
            glColor3f(0.08, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.06, 0.07 + legLift, -0.02);
                glScalef(0.045, 0.03, 0.09);
                glutSolidCube(0.5);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(-0.06, 0.07 - legLift, -0.02);
                glScalef(0.045, 0.03, 0.09);
                glutSolidCube(0.5);
            glPopMatrix();

        glPopMatrix();

    }


struct Cloud { float x, y, z, scale; };
Cloud clouds[14];
bool cloudsReady = false;
void initClouds()
{
    if (cloudsReady) return;
    for (int c = 0; c < 14; c++)
    {
        clouds[c].x = -32.0f + (rand() % 640) / 10.0f;
        clouds[c].y = 14.0f + (rand() % 150) / 10.0f;
        clouds[c].z = 1.5f + (rand() % 25) / 10.0f;
        clouds[c].scale = 1.1f + (rand() % 20) / 10.0f;
    }
    cloudsReady = true;
}

void drawCloudPuff(float x, float y, float z, float scale, float alpha)
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
        glPushMatrix(); glTranslatef( 0.0f, -0.15f, 0.0f); glutSolidSphere(0.55, 10, 8); glPopMatrix();
    glPopMatrix();
}

void drawClouds()
{
    initClouds();
    // Faint by night (moonlit silhouettes), bright and soft by day.
    float alpha = isNight ? 0.16f : 0.80f;
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    for (int c = 0; c < 14; c++)
        drawCloudPuff(clouds[c].x, clouds[c].y, clouds[c].z, clouds[c].scale, alpha);
}

 void sky()
        {
        initStars();

        glPushMatrix();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glTranslatef(0.0, 0.0, -55.0); // push the whole sky far behind the scene


        glBegin(GL_QUADS);
            glColor3f(zenith_r, zenith_g, zenith_b);
            glVertex3f(-40.0, 34.0, 0);
            glVertex3f(40.0, 34.0, 0);
            glColor3f(sky_red, sky_green, sky_blue);
            glVertex3f(40.0, -6.0, 0);
            glVertex3f(-40.0, -6.0, 0);
        glEnd();

        // Sun (day) or Moon (night)
        glPushMatrix();
            if (isNight)
            {
                glColor3f(0.92, 0.92, 0.88);
                glTranslatef(18.0, 20.0, 4.0);
                glutSolidSphere(1.5, 20, 20);
            }
            else
            {
                glColor3f(1.0, 0.85, 0.25);
                glTranslatef(-18.0, 22.0, 4.0);
                glutSolidSphere(2.0, 20, 20);
            }
        glPopMatrix();

        // Realistic soft clouds, drifting across the sky dome
        drawClouds();

        // Stars, visible only at night
        if (isNight)
        {
            glColor3f(1.0, 1.0, 1.0);
            glPointSize(2.0);
            glBegin(GL_POINTS);
            for (int s = 0; s < 70; s++)
                glVertex3f(stars[s].x, stars[s].y, 3.0);
            glEnd();
        }

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glPopMatrix();
        }


void mountains()
    {
        float dim = isNight ? 0.55f : 1.0f;

        glBegin(GL_QUADS);
            glColor3f(0.06f * dim, 0.42f * dim, 0.10f * dim);
            glVertex3f(-17.0f, -10, 0);
            glVertex3f(-1.0f, -10, 0);
            glVertex3f(-1.0f, WORLD_LENGTH, 0);
            glVertex3f(-17.0f, WORLD_LENGTH, 0);
        glEnd();


        float spacing = denseMode ? 24.0f : 40.0f;
        float heightMul = (denseMode ? 1.7f : 1.15f);

        for (float z = -60; z < WORLD_LENGTH + 40.0f; z += spacing)
        {
            float h1 = (4.5f + 1.8f * sinf(z * 0.021f)) * heightMul;   // front ridge height
            float h2 = (7.5f + 2.6f * cosf(z * 0.015f)) * heightMul;   // back ridge height, taller

            // Back ridge: farther away, hazier / bluer, big and tall
            glPushMatrix();
                glColor3f(0.46f * dim, 0.53f * dim, 0.66f * dim);
                glTranslatef(-13.0, z, 0.0);
                glutSolidCone(3.4, h2, 4, 1);
            glPopMatrix();

            // Snow cap on the taller back peaks
            if (h2 > 8.0f)
            {
                glPushMatrix();
                    glColor3f(0.95f * dim, 0.95f * dim, 0.98f * dim);
                    glTranslatef(-13.0, z, h2 * 0.70f);
                    glutSolidCone(1.3, h2 * 0.32f, 4, 1);
                glPopMatrix();
            }

            // Front ridge: closer to the road, darker earthy tone, gives parallax
            glPushMatrix();
                glColor3f(0.30f * dim, 0.29f * dim, 0.25f * dim);
                glTranslatef(-9.0, z + 26, 0.0);
                glutSolidCone(2.5, h1, 4, 1);
            glPopMatrix();
        }
    }


void lamppost(float z)
{
    // pole
    glPushMatrix();
        glColor3f(0.22f, 0.22f, 0.24f);
        glTranslatef(-1.05f, z, 0.62f);
        glScalef(0.045f, 0.045f, 1.24f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // base flare
    glPushMatrix();
        glColor3f(0.18f, 0.18f, 0.20f);
        glTranslatef(-1.05f, z, 0.02f);
        glScalef(0.09f, 0.09f, 0.05f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // arm reaching out over the road
    glPushMatrix();
        glColor3f(0.20f, 0.20f, 0.22f);
        glTranslatef(-0.82f, z, 1.22f);
        glScalef(0.40f, 0.04f, 0.04f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // lamp housing
    glPushMatrix();
        glColor3f(0.12f, 0.12f, 0.13f);
        glTranslatef(-0.58f, z, 1.15f);
        glScalef(0.13f, 0.13f, 0.07f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // glowing bulb
    glPushMatrix();
        glColor4ub(255, 245, 190, roadlight);
        glTranslatef(-0.58f, z, 1.06f);
        glutSolidSphere(0.05f, 10, 10);
    glPopMatrix();

    // soft downward light cone — barely visible by day, glows at night
    //glPushMatrix();
        //glColor4ub(255, 235, 150, isNight ? 80 : 12);
        //glTranslatef(-0.58f, z, 0.55f);
        //glRotatef(180, 1.0f, 0.0f, 0.0f);
       // glutSolidCone(0.26f, 0.5f, 14, 4);
    //glPopMatrix();
}

void roadside()
    {

	// realistic lampposts, evenly spaced down the left edge of the road
     for (float z = -38; z < WORLD_LENGTH; z += 9.0f)
	{
		lamppost(z);
	}



        glBegin(GL_QUADS);
            glColor3ub(0, 155, 20);
			glVertex3f(-5.0, -10, 0);
			glVertex3f(-1.0, -10, 0);
			glVertex3f(-1.0, WORLD_LENGTH, 0);
			glVertex3f(-5.0, WORLD_LENGTH, 0);
		glEnd();
		glBegin(GL_QUADS);
            glColor3ub(0, 155, 20);
			glVertex3f(1.0, -10, 0);
			glVertex3f(5.0, -10, 0);
			glVertex3f(5.0, WORLD_LENGTH, 0);
			glVertex3f(1.0, WORLD_LENGTH, 0);
		glEnd();

        // ---------- Grass "texture" ----------

        float gdim = isNight ? 0.55f : 1.0f;
        for (float z = -10; z < WORLD_LENGTH; z += 1.2f)
        {
            float jitter = sinf(z * 0.9f) * 0.20f;
            float shade  = 0.70f + 0.30f * sinf(z * 0.5f);

            glPushMatrix();
                glColor3f(0.0f, 0.50f * shade * gdim, 0.04f * gdim);
                glTranslatef(-3.0f + jitter, z, 0.02f);
                glScalef(0.09f, 0.09f, 0.11f);
                glutSolidCone(0.5f, 1.2f, 4, 1);
            glPopMatrix();

            glPushMatrix();
                glColor3f(0.0f, 0.55f * (1.05f - shade) * gdim + 0.10f, 0.04f * gdim);
                glTranslatef(3.0f - jitter, z + 0.6f, 0.02f);
                glScalef(0.09f, 0.09f, 0.11f);
                glutSolidCone(0.5f, 1.2f, 4, 1);
            glPopMatrix();
        }


    }

    // Two house designs — a taller two-storey house and a smaller single-storey
    // cottage — cycling through a small palette of wall colors so the
    // street doesn't look like one house copy-pasted down the road.
    void house(){
     int idx = 0;
     for (float z = -40; z < JUNGLE_START; z +=9.6)
	{
		bool twoFloor = (idx % 2 == 0);

		float palette[4][3] = {
		    {0.95f, 0.90f, 0.80f}, // cream
		    {0.82f, 0.50f, 0.40f}, // terracotta
		    {0.72f, 0.78f, 0.84f}, // pale blue-grey
		    {0.88f, 0.72f, 0.50f}, // tan
		};
		int pIdx = idx % 4;
		float wr = palette[pIdx][0], wg = palette[pIdx][1], wb = palette[pIdx][2];
		idx++;

		// windows glow warm yellow at night, plain white/black by day
		float winR = isNight ? 1.00f : 1.00f;
		float winG = isNight ? 0.82f : 1.00f;
		float winB = isNight ? 0.35f : 1.00f;

		glPushMatrix();
			glTranslatef(3.0, z, 0.0);

			if (twoFloor)
			{
				// ---- ground floor ----
				glColor3f(wr, wg, wb);
				glPushMatrix();
					glTranslatef(0.0, 0.0, .30);
					glutSolidCube(1);
				glPopMatrix();

				// ---- upper floor, narrower and a touch darker ----
				glColor3f(wr * 0.90f, wg * 0.90f, wb * 0.90f);
				glPushMatrix();
					glTranslatef(0.0, 0.0, 1.02);
					glScalef(0.82, 0.82, 0.70);
					glutSolidCube(1);
				glPopMatrix();

				// ground-floor windows
				glColor3f(winR, winG, winB);
				glPushMatrix(); glTranslatef(-0.45, 0.0, .40); glutSolidCube(.18); glPopMatrix();
				glPushMatrix(); glTranslatef( 0.45, 0.0, .40); glutSolidCube(.18); glPopMatrix();

				// upper-floor windows
				glPushMatrix(); glTranslatef(-0.32, 0.0, 1.08); glutSolidCube(.14); glPopMatrix();
				glPushMatrix(); glTranslatef( 0.32, 0.0, 1.08); glutSolidCube(.14); glPopMatrix();

				// door
				glColor3ub(60, 38, 20);
				glPushMatrix();
					glTranslatef(0.0, -.50, .18);
					glScalef(.22, .18, .55);
					glutSolidCube(1);
				glPopMatrix();

				// roof
				glColor3ub(120, 45, 35);
				glPushMatrix();
					glTranslatef(3.0-3.0, z-z, 1.30);
					glutSolidCone(0.9, 0.65, 4, 6);
				glPopMatrix();
			}
			else
			{
				// ---- single-storey cottage, lower and wider ----
				glColor3f(wr, wg, wb);
				glPushMatrix();
					glTranslatef(0.0, 0.0, .22);
					glScalef(1.05, 1.0, 0.55);
					glutSolidCube(1);
				glPopMatrix();

				glColor3f(winR, winG, winB);
				glPushMatrix(); glTranslatef(0.42, 0.0, .30); glutSolidCube(.20); glPopMatrix();

				glColor3ub(60, 38, 20);
				glPushMatrix();
					glTranslatef(-0.15, -.48, .16);
					glScalef(.22, .18, .42);
					glutSolidCube(1);
				glPopMatrix();

				glColor3ub(70, 61, 46);
				glPushMatrix();
					glTranslatef(0.0, 0.0, .50);
					glutSolidCone(1, .8, 4, 6);
				glPopMatrix();
			}

			// small green garden bush beside every house
			glColor3f(0.10f, 0.45f, 0.12f);
			glPushMatrix();
				glTranslatef(-0.85f, -0.85f, 0.12f);
				glutSolidSphere(0.14f, 8, 8);
			glPopMatrix();

		glPopMatrix();

	}
    }
    // A more realistic tree: a tapered trunk (thin gluCylinder, not a solid
    // block) topped with a small cluster of overlapping, differently-shaded
    // spheres so the canopy reads as irregular foliage instead of a plain cone.
    void realTree(float x, float zAlong, float trunkH, float canopyR,
                   float r, float g, float b, bool jungle)
    {
        float dim = isNight ? 0.6f : 1.0f;
        if (!treeQuad) treeQuad = gluNewQuadric();

        glPushMatrix();
            glTranslatef(x, zAlong, 0.0f);

            // tapered trunk, narrower at the top than at the base, with bark texture rings
            glColor3f(0.30f * dim, 0.19f * dim, 0.09f * dim);
            gluCylinder(treeQuad, trunkH * 0.09f, trunkH * 0.04f, trunkH, 7, 3);

            // canopy cluster, lifted to the top of the trunk
            glTranslatef(0.0f, 0.0f, trunkH);

            glColor3f(r * dim, g * dim, b * dim);
            glutSolidSphere(canopyR, 10, 8);

            glColor3f(r * 0.90f * dim, g * 0.95f * dim, b * 0.90f * dim);
            glPushMatrix();
                glTranslatef(canopyR * 0.55f, canopyR * 0.10f, canopyR * 0.30f);
                glutSolidSphere(canopyR * 0.68f, 8, 7);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(-canopyR * 0.50f, -canopyR * 0.25f, canopyR * 0.22f);
                glutSolidSphere(canopyR * 0.60f, 8, 7);
            glPopMatrix();

            glColor3f(r * 1.12f * dim, g * 1.06f * dim, b * 1.10f * dim);
            glPushMatrix();
                glTranslatef(canopyR * 0.05f, canopyR * 0.45f, canopyR * 0.42f);
                glutSolidSphere(canopyR * 0.58f, 8, 7);
            glPopMatrix();

            if (jungle)
            {
                // extra lifted tier so jungle trees read as taller and denser
                glColor3f(r * 1.2f * dim, g * 1.12f * dim, b * 1.18f * dim);
                glPushMatrix();
                    glTranslatef(0.0f, -canopyR * 0.1f, canopyR * 0.9f);
                    glutSolidSphere(canopyR * 0.5f, 8, 7);
                glPopMatrix();
            }
        glPopMatrix();
    }


    void middleTrees()
    {
        for (float z = -50; z < WORLD_LENGTH + 40.0f; z += 6.0f)
        {
            float heightMul = denseMode ? 1.3f : 1.0f;
            float h = (2.6f + 0.8f * sinf(z * 0.09f)) * heightMul;
            float xoff = -6.2f + 0.4f * sinf(z * 0.13f);
            realTree(xoff, z, h, 0.55f, 0.06f, 0.34f, 0.09f, true);
        }
    }

    void tree(){
        // Trees are taller overall than before, and after 30s of play
        // (denseMode) they grow taller still and pack in closer together.
        float heightMul = denseMode ? 1.55f : 1.15f;
        float step = denseMode ? 2.6f : 4.0f;

        for (float z = -40; z < WORLD_LENGTH + 40.0f; z += step)
	{
		if (z < JUNGLE_START)
		{
			// ---- ordinary park trees, one on each side of the road ----
			realTree(-1.20f, z,       0.62f * heightMul, 0.28f, 0.16f, 0.55f, 0.14f, false);
			realTree( 1.20f, z + 1.6f, 0.58f * heightMul, 0.26f, 0.18f, 0.60f, 0.15f, false);
		}
		else
		{
			// ---- dense jungle canopy (wilderness stretch) ----
			// slightly randomized size/offset per tree, using the position itself
			// as a cheap deterministic "random" seed so it looks organic, not tiled
			float jitter = sinf(z * 0.37f) * 0.15f;
			float scaleL = 0.65f + 0.20f * sinf(z * 0.21f);
			float scaleR = 0.65f + 0.20f * cosf(z * 0.19f);

			realTree(-1.15f + jitter, z,        1.40f * scaleL * heightMul, 0.44f * scaleL, 0.05f, 0.30f, 0.06f, true);
			realTree( 1.15f - jitter, z + 2.0f, 1.40f * scaleR * heightMul, 0.44f * scaleR, 0.06f, 0.34f, 0.07f, true);

			// occasional extra tree tucked further back for density
			if (fmod(z, 12.0f) < 4.0f)
			{
				realTree(-1.55f, z + 1.0f, 1.10f * heightMul, 0.36f, 0.05f, 0.27f, 0.06f, true);
				realTree( 1.55f, z + 3.0f, 1.15f * heightMul, 0.38f, 0.06f, 0.31f, 0.07f, true);
			}

			// extra density tier once denseMode kicks in
			if (denseMode && fmod(z, 12.0f) < 4.0f)
			{
				realTree(-1.85f, z + 2.0f, 1.0f * heightMul, 0.32f, 0.05f, 0.27f, 0.06f, true);
				realTree( 1.85f, z + 0.5f, 1.05f * heightMul, 0.34f, 0.06f, 0.31f, 0.07f, true);
			}
		}
	}

        // Middle-ground tall tree line, the whole length of the road
        middleTrees();

    }




void roadline(){
    for (float z = -4; z < 6; z +=1)
	{
		glPushMatrix();

			glColor3f(1, 1, 1);
				glBegin(GL_QUADS);
				glVertex3f(-.03, z, 0);
				glVertex3f(.03, z, 0);
				glVertex3f(.03, z+.5, 0);
				glVertex3f(-.03, z+.5, 0);
			glEnd();
		glPopMatrix();
	}
    }


    // A simple low-poly obstacle car: body, cabin, 4 wheels and headlights,
    // built the same way as gamerbike() (cubes + spheres). laneX is -.50 or
    // .50 (left/right lane), colorSeed just varies the paint color a bit.
    void car(float laneX, float zp, float colorSeed)
    {
        float r = 0.55f + 0.35f * fabs(sinf(colorSeed));
        float g = 0.15f + 0.20f * fabs(cosf(colorSeed * 1.7f));
        float b = 0.20f + 0.30f * fabs(sinf(colorSeed * 0.6f));

        glPushMatrix();
            glTranslatef(laneX, zp, -0.02f);

            // body
            glColor3f(r, g, b);
            glPushMatrix();
                glScalef(0.30f, 0.55f, 0.16f);
                glutSolidCube(0.7);
            glPopMatrix();

            // cabin / roof
            glColor3f(r * 0.8f, g * 0.8f, b * 0.8f);
            glPushMatrix();
                glTranslatef(0.0f, -0.03f, 0.14f);
                glScalef(0.22f, 0.30f, 0.10f);
                glutSolidCube(0.7);
            glPopMatrix();

            // wheels (4)
            glColor3f(0.05f, 0.05f, 0.05f);
            float wx = 0.12f, wy = 0.14f, wz = -0.04f;
            glPushMatrix(); glTranslatef( wx,  wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();
            glPushMatrix(); glTranslatef(-wx,  wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();
            glPushMatrix(); glTranslatef( wx, -wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();
            glPushMatrix(); glTranslatef(-wx, -wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();

            // headlights — bright, and especially visible at night
            glColor4ub(255, 250, 200, isNight ? 255 : 160);
            glPushMatrix(); glTranslatef( 0.08f, 0.27f, 0.02f); glutSolidSphere(0.025, 8, 8); glPopMatrix();
            glPushMatrix(); glTranslatef(-0.08f, 0.27f, 0.02f); glutSolidSphere(0.025, 8, 8); glPopMatrix();

        glPopMatrix();
    }

    void objectcube()
    {
        // Draws exactly the obstacles collision() checks against (see
        // buildObstacles()) — nothing here is out of sync with what can hit you.
        for (int idx = 0; idx < obstacleCount; idx++)
        {
            float laneX = (obstacles[idx].lane == 0) ? LANE_LEFT_X : LANE_RIGHT_X;
            car(laneX, obstacles[idx].pos, obstacles[idx].pos * 0.05f + obstacles[idx].lane * 2.0f);
        }
    }


    void road(){
        for (float z = -10; z < WORLD_LENGTH; z +=1)
	{
		glPushMatrix();
			glColor3f(1, 1, 1);
				glBegin(GL_QUADS);
				glVertex3f(-.03, z, 0);
				glVertex3f(.03, z, 0);
				glVertex3f(.03, z+.5, 0);
				glVertex3f(-.03, z+.5, 0);
			glEnd();
		glPopMatrix();
	}
	// The road surface itself is always plain black, day or night.
	glPushMatrix();
		glColor3ub(0, 0, 0);
		glTranslatef(0.0, 0.0, -.50);
		glBegin(GL_QUADS);
			glVertex3f(-1.3, -10, 0);
			glVertex3f(1.3, -10, 0);
			glVertex3f(1.3, WORLD_LENGTH, 0);
			glVertex3f(-1.3, WORLD_LENGTH, 0);
		glEnd();


	glPopMatrix();


    }

// A checkered finish line painted across the road, with two posts and a
// checkered banner overhead, marking FINISH_DISTANCE — the end of the road.
void finishLine()
    {
        float fz = FINISH_DISTANCE;

        // checkered stripe painted on the road surface
        glPushMatrix();
            glTranslatef(0.0, fz, 0.0);
            int cols = 8;
            float stripeW = 2.6f / cols;
            for (int c = 0; c < cols; c++)
            {
                float cx = -1.3f + c * stripeW;
                bool whiteBlock = (c % 2 == 0);
                glColor3f(whiteBlock ? 1.0f : 0.05f, whiteBlock ? 1.0f : 0.05f, whiteBlock ? 1.0f : 0.05f);
                glBegin(GL_QUADS);
                    glVertex3f(cx, -0.4f, 0.001f);
                    glVertex3f(cx + stripeW, -0.4f, 0.001f);
                    glVertex3f(cx + stripeW, 0.4f, 0.001f);
                    glVertex3f(cx, 0.4f, 0.001f);
                glEnd();
            }
        glPopMatrix();

        // posts on either side of the road
        glColor3ub(190, 190, 195);
        glPushMatrix();
            glTranslatef(-1.35f, fz, 0.9f);
            glScalef(.08f, .08f, 1.8f);
            glutSolidCube(1);
        glPopMatrix();
        glPushMatrix();
            glTranslatef(1.35f, fz, 0.9f);
            glScalef(.08f, .08f, 1.8f);
            glutSolidCube(1);
        glPopMatrix();

        // checkered banner spanning the top
        glPushMatrix();
            glTranslatef(0.0, fz, 1.9f);
            int bands = 10;
            float bandW = 2.9f / bands;
            for (int c = 0; c < bands; c++)
            {
                float cx = -1.45f + c * bandW;
                bool whiteBlock = (c % 2 == 0);
                glColor3f(whiteBlock ? 1.0f : 0.05f, whiteBlock ? 1.0f : 0.05f, whiteBlock ? 1.0f : 0.05f);
                glBegin(GL_QUADS);
                    glVertex3f(cx, -0.15f, 0);
                    glVertex3f(cx + bandW, -0.15f, 0);
                    glVertex3f(cx + bandW, 0.15f, 0);
                    glVertex3f(cx, 0.15f, 0);
                glEnd();
            }
        glPopMatrix();
    }

//Draws the 3D scene
void drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Fog fades the far-away road/scenery into the sky color instead of
	// clipping abruptly, which matters now that the road runs 1000 units out.
	// Range and color follow the current day/night state (see keyboard()).
	glEnable(GL_FOG);
	GLfloat fogColor[4] = { sky_red, sky_green, sky_blue, 1.0f };
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, fogNear);
	glFogf(GL_FOG_END, fogFar);
	glHint(GL_FOG_HINT, GL_NICEST);

	glMatrixMode(GL_MODELVIEW); //Switch to the drawing perspective
	glLoadIdentity(); //Reset the drawing perspective
	glRotatef(-_cameraAngle, 0.0, 1.0, 0.0); //Rotate the camera
	glTranslatef(0.0, 0.0, -7.0); //Move forward 5 units

	// Day/night lighting: position+color GL_LIGHT0 once, here, in eye space,
	// so it stays put for the rest of the frame. Lighting itself starts OFF
	// (flat road/sky/grass/text stay exactly as before); each 3D prop
	// function (gamerbike/house/tree/mountains/car) switches it on for
	// itself and back off when it's done.
	updateLighting();
	glDisable(GL_LIGHTING);

	sky(); // draw the sky/sun-or-moon/stars/clouds first, behind everything else
	gamerbike();
    glPushMatrix(); //Save the current state of transformations
	glTranslatef(0.0, 0.0, 0.0); //Move to the center of the triangle
	glRotatef(80 , -1.0, 0.0, 0.0);

	glPushMatrix();
	glTranslatef(0.0, crmove, 0.0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
    road();
    glPopMatrix();
    glPushMatrix();
	glTranslatef(0.0, crmove, 0.0);
    mountains();
    tree();
    house();
    roadside();
    objectcube();
    finishLine();
    GameScore();
    cout<<score<<endl;
    //cout<<crmove<<endl;
    //cout<<y11<<endl;
    glPopMatrix();

    glPushMatrix();
     glColor3ub(0,0,0);
     glTranslatef(5.52, 0.0, 2.0);

     ostringstream cnvrt;
     cnvrt << score;
     sprint(-4,-2.3,"Score: "+cnvrt.str());
     glPopMatrix();


      glPushMatrix();
     glColor3ub(0,0,0);
     glTranslatef(5.5, 0.0, 1.8);
     ostringstream cnvrt2;
     cnvrt2 << totalMeter;
     sprint(-4,-2.4,"Distance Travel: "+cnvrt2.str());
     glPopMatrix();

     glPushMatrix();
     glColor3ub(0,0,0);
     glTranslatef(5.5, 0.0, 1.6);
     ostringstream cnvrt3;
     cnvrt3 << carspeed;
     sprint(-4,-2.4,"Speed: "+cnvrt3.str());
     glPopMatrix();

     if (paused)
     {
         glPushMatrix();
         glColor3ub(0,0,0);
         glTranslatef(5.5, 0.0, 1.6);
         sprint(-4,-2.0,"PAUSED - press P to resume");
         glPopMatrix();
     }

    glPopMatrix();
    glClearColor(sky_red, sky_green, sky_blue, 1.0);

    // The road ends at FINISH_DISTANCE: once the player has traveled that far,
    // show the finish-line "You Win" screen instead of continuing to scroll.
    if (!raceFinished && -crmove >= FINISH_DISTANCE)
    {
        raceFinished = true;
        finishRace('a');
    }
    else if(collision())
    {
        winner('a');
    }
    //sky();
	glutSwapBuffers();
}

void update(int value) {

    int elapsed = glutGet(GLUT_ELAPSED_TIME) - gameStartTimeMs;


    denseMode = (elapsed > 30000);

    // Automatic day/night cycle, unless the player took manual control with
    // the n/d/t keys.
    if (autoCycleEnabled)
    {
        bool shouldBeNight = ((elapsed / DAYNIGHT_PERIOD_MS) % 2) == 1;
        if (shouldBeNight != isNight)
        {
            if (shouldBeNight) setNight(); else setDay();
        }
    }

    if (!paused)
    {
        // Speed ramps up the further the player has travelled, up to 2x
        // the starting speed by the time the finish line is reached.
        float distance = -crmove;
        float speedFactor = 1.0f + 1.0f * fminf(distance / FINISH_DISTANCE, 1.0f);
        crmove -= baseCrSpeed * speedFactor;
        carspeed = (int)(45 * speedFactor);

        _angle += 2.0f;
        if (_angle > 360) {
            _angle -= 360;

        }
        _ang_tri += 0.7f;
        if (_ang_tri > 80) {
                _ang_tri=0;
            //crmove= 40;
        }
    }

	glutPostRedisplay(); //Tell GLUT that the display has changed

	//Tell GLUT to call update again in 25 milliseconds
	glutTimerFunc(crspeed, update, 0);
}


int main(int argc, char** argv) {
	//Initialize GLUT

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 500);
	glutInitWindowPosition(100,100);
	glutCreateWindow("Transformations");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable( GL_BLEND );
	//glClearColor(1.0, 1.0, 1.0, 0.0);
	initRendering();
	buildObstacles();
	gameStartTimeMs = glutGet(GLUT_ELAPSED_TIME);
	//Set handler functions
	glutDisplayFunc(drawScene);
	glutReshapeFunc(handleResize);
	glutTimerFunc(25, update, 0); //Add a timer
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(keyboardown);
	glutMainLoop();
	return 0;
}
