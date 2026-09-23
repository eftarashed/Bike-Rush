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
// ---------- Obstacle cars: single source of truth for both drawing and
// collision, so what you SEE is the only thing that can hit you. ----------
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
float zenith_r = 0.10f, zenith_g = 0.45f, zenith_b = 0.95f;
float fogNear = 250.0f, fogFar = 650.0f;

// ---------- Distance where the roadside scenery turns into jungle ----------
const float JUNGLE_START = 300.0f;

// ---------- Finish line / world length ----------
const float FINISH_DISTANCE = 520.0f;
const float WORLD_LENGTH = FINISH_DISTANCE + 80.0f;
bool raceFinished = false;

// ---------- Pause ----------
bool paused = false;

// ---------- Camera zoom ----------
float camDistance = -7.0f;
const float CAM_DISTANCE_MIN = -14.0f;
const float CAM_DISTANCE_MAX = -3.0f;

// ---------- Elapsed play time ----------
int gameStartTimeMs = 0;
bool denseMode = false;

// ---------- Speed ramps up with distance travelled ----------
float baseCrSpeed = 0.10f;

// ---------- Simple fixed starfield ----------
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

GLUquadric* treeQuad = NULL;

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
    glColor3f(0.0,0.0,0.0);
    glRasterPos2f( x, y);
    for( i=0; i <  st.length(); i++)
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
	strcpy(quote[1], "Game Over");
	strcpy(quote[0], "OOPSS:-)!!!");
	numberOfQuotes = 5;
	glutDisplayFunc(myDisplayFunction);
	glutReshapeFunc(reshape);
	glutIdleFunc(timeTick);
	glutMainLoop();
	return 0;
}

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
    }
    else if(key=='d')
    {
        setDay();
    }
    else if(key=='t')
    {
        if (isNight) setDay(); else setNight();
    }
    else if(key=='e')
    {
        exit(1);
    }
    else if(key=='p' || key=='P')
    {
        paused = !paused;
    }
    else if(key=='+' || key=='=')
    {
        camDistance += 0.6f;
        if (camDistance > CAM_DISTANCE_MAX) camDistance = CAM_DISTANCE_MAX;
    }
    else if(key=='-' || key=='_')
    {
        camDistance -= 0.6f;
        if (camDistance < CAM_DISTANCE_MIN) camDistance = CAM_DISTANCE_MIN;
    }
}

bool collision()
{
    float distanceTravelled = -crmove;

    for (int idx = 0; idx < obstacleCount; idx++)
    {
        if (obstacles[idx].lane != carpos) continue;
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
            if (obstacles[idx].lane != carpos) score += 5;
        }
    }
}

void initRendering()
{
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glDisable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
}

void updateLighting()
{
    GLfloat lightPos[4];
    GLfloat diffuse[4];
    GLfloat ambient[4];

    if (isNight)
    {
        lightPos[0]=0.35f; lightPos[1]=0.55f; lightPos[2]=0.30f; lightPos[3]=0.0f;
        diffuse[0]=0.32f; diffuse[1]=0.35f; diffuse[2]=0.50f; diffuse[3]=1.0f;
        ambient[0]=0.10f; ambient[1]=0.11f; ambient[2]=0.16f; ambient[3]=1.0f;
    }
    else
    {
        lightPos[0]=-0.45f; lightPos[1]=0.65f; lightPos[2]=0.35f; lightPos[3]=0.0f;
        diffuse[0]=1.0f; diffuse[1]=0.97f; diffuse[2]=0.88f; diffuse[3]=1.0f;
        ambient[0]=0.45f; ambient[1]=0.45f; ambient[2]=0.42f; ambient[3]=1.0f;
    }

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
}

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
        float wheelSpin = _ang_tri * 6.0f;

        glPushMatrix();
            glTranslatef(xp, -1.0, 3.5);
            glRotatef(-6, -1.0, 0.0, 0.0);

            if (!treeQuad) treeQuad = gluNewQuadric();

            for (int wheel = 0; wheel < 2; wheel++)
            {
                float wz = (wheel == 0) ? -0.30f : 0.36f;
                glPushMatrix();
                    glTranslatef(0.0, 0.0, wz);
                    glRotatef(90, 0.0, 1.0, 0.0);
                    glColor3f(0.03, 0.03, 0.03);
                    glutSolidTorus(0.045, 0.20, 10, 22);
                    glColor3f(0.55, 0.55, 0.58);
                    glutSolidTorus(0.012, 0.15, 6, 18);
                    glColor3f(0.72, 0.10, 0.10);
                    glutSolidTorus(0.010, 0.12, 6, 14);
                    glColor3f(0.70, 0.70, 0.73);
                    for (int s = 0; s < 5; s++)
                    {
                        glPushMatrix();
                            glRotatef(s * 72.0f + wheelSpin, 0.0, 0.0, 1.0);
                            glTranslatef(0.0, 0.075, 0.0);
                            glScalef(0.030f, 0.15f, 0.012f);
                            glutSolidCube(1.0f);
                        glPopMatrix();
                    }
                glPopMatrix();
            }

            glColor3f(0.42, 0.24, 0.10);
            glPushMatrix();
                glTranslatef(0.0, 0.075, 0.02);
                glScalef(0.20, 0.16, 0.24);
                glutSolidCube(1.0);
            glPopMatrix();
            glColor3f(0.55, 0.55, 0.58);
            for (int f = 0; f < 5; f++)
            {
                glPushMatrix();
                    glTranslatef(0.0, 0.135f + f * 0.014f, 0.10);
                    glScalef(0.16, 0.01, 0.10);
                    glutSolidCube(1.0);
                glPopMatrix();
            }

            glColor3f(0.80, 0.80, 0.83);
            glPushMatrix();
                glTranslatef(0.11, 0.03, 0.08);
                glRotatef(180, 0.0, 1.0, 0.0);
                gluCylinder(treeQuad, 0.020, 0.020, 0.14, 10, 2);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(0.12, 0.03, -0.06);
                glRotatef(180, 0.0, 1.0, 0.0);
                gluCylinder(treeQuad, 0.040, 0.033, 0.15, 12, 2);
            glPopMatrix();

            glColor3f(0.85, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.20, 0.14);
                glRotatef(55, 1.0, 0.0, 0.0);
                glScalef(0.035, 0.035, 0.34);
                glutSolidCube(0.5);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(0.0, 0.20, -0.14);
                glRotatef(-30, 1.0, 0.0, 0.0);
                glScalef(0.03, 0.03, 0.26);
                glutSolidCube(0.5);
            glPopMatrix();

            glColor3f(0.85, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.245, 0.12);
                glScalef(0.16, 0.13, 0.24);
                glutSolidSphere(0.5, 14, 12);
            glPopMatrix();
            glColor3f(0.95, 0.95, 0.95);
            glPushMatrix();
                glTranslatef(0.0, 0.30, 0.12);
                glScalef(0.055, 0.05, 0.22);
                glutSolidCube(0.5);
            glPopMatrix();

            glColor3f(0.35, 0.19, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.245, -0.14);
                glScalef(0.15, 0.05, 0.26);
                glutSolidCube(0.5);
            glPopMatrix();

            glColor3f(0.85, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.22, -0.30);
                glRotatef(-18, 1.0, 0.0, 0.0);
                glScalef(0.13, 0.04, 0.14);
                glutSolidCube(0.5);
            glPopMatrix();
            glEnable(GL_BLEND);
            glColor4ub(230, 20, 20, 220);
            glPushMatrix();
                glTranslatef(0.0, 0.225, -0.37);
                glutSolidSphere(0.022, 8, 8);
            glPopMatrix();
            glDisable(GL_BLEND);

            glColor3f(0.78, 0.78, 0.80);
            for (int side = -1; side <= 1; side += 2)
            {
                glPushMatrix();
                    glTranslatef(side * 0.045f, 0.16, 0.32);
                    glRotatef(-14, 1.0, 0.0, 0.0);
                    gluCylinder(treeQuad, 0.018, 0.018, 0.28, 8, 2);
                glPopMatrix();
            }

            glColor3f(0.20, 0.20, 0.22);
            glPushMatrix();
                glTranslatef(0.0, 0.28, 0.33);
                glScalef(0.13, 0.035, 0.05);
                glutSolidCube(1.0);
            glPopMatrix();

            glColor3f(0.10, 0.10, 0.11);
            glPushMatrix();
                glTranslatef(0.0, 0.28, 0.40);
                glScalef(0.11, 0.11, 0.06);
                glutSolidSphere(0.5, 14, 12);
            glPopMatrix();
            glEnable(GL_BLEND);
            glColor4ub(255, 250, 220, isNight ? 255 : 200);
            glPushMatrix();
                glTranslatef(0.0, 0.28, 0.435);
                glutSolidSphere(0.05, 12, 12);
            glPopMatrix();
            glDisable(GL_BLEND);

            glColor3f(0.15, 0.15, 0.17);
            glPushMatrix();
                glTranslatef(0.0, 0.315, 0.335);
                glScalef(0.30, 0.028, 0.028);
                glutSolidCube(0.5);
            glPopMatrix();
            for (int side = -1; side <= 1; side += 2)
            {
                float sx = side * 0.145f;
                glColor3f(0.06, 0.06, 0.06);
                glPushMatrix();
                    glTranslatef(sx, 0.315, 0.335);
                    glScalef(0.045, 0.045, 0.06);
                    glutSolidCube(0.5);
                glPopMatrix();
                glColor3f(0.55, 0.55, 0.58);
                glPushMatrix();
                    glTranslatef(sx * 0.92f, 0.315, 0.36);
                    glRotatef(side * 20.0f, 0.0, 1.0, 0.0);
                    glScalef(0.010, 0.012, 0.06);
                    glutSolidCube(0.5);
                glPopMatrix();
                glColor3f(0.10, 0.10, 0.11);
                glPushMatrix();
                    glTranslatef(sx, 0.40, 0.30);
                    glScalef(0.012, 0.09, 0.012);
                    glutSolidCube(0.5);
                glPopMatrix();
                glPushMatrix();
                    glTranslatef(sx * 1.15f, 0.45, 0.29);
                    glScalef(0.06, 0.035, 0.015);
                    glutSolidCube(0.5);
                glPopMatrix();
            }

            glColor3f(0.20, 0.20, 0.22);
            for (int side = -1; side <= 1; side += 2)
            {
                glPushMatrix();
                    glTranslatef(side * 0.10f, 0.075, -0.04);
                    glScalef(0.10, 0.02, 0.03);
                    glutSolidCube(0.5);
                glPopMatrix();
            }

            glColor3f(0.38, 0.22, 0.10);
            glPushMatrix();
                glTranslatef(0.0, 0.46, -0.06);
                glRotatef(18, 1.0, 0.0, 0.0);
                glScalef(0.17, 0.32, 0.13);
                glutSolidCube(0.5);
            glPopMatrix();
            glColor3f(0.85, 0.08, 0.08);
            glPushMatrix();
                glTranslatef(0.0, 0.48, 0.0);
                glRotatef(18, 1.0, 0.0, 0.0);
                glScalef(0.175, 0.06, 0.135);
                glutSolidCube(0.5);
            glPopMatrix();

            glColor3f(0.94, 0.78, 0.62);
            glPushMatrix();
                glTranslatef(0.0, 0.615, 0.11);
                glScalef(0.045, 0.045, 0.06);
                glutSolidCube(0.5);
            glPopMatrix();

            glPushMatrix();
                glTranslatef(0.0, 0.65, 0.135);
                glutSolidSphere(0.06, 14, 14);
            glPopMatrix();

            glColor3f(0.95, 0.15, 0.15);
            glPushMatrix();
                glTranslatef(0.0, 0.685, 0.14);
                glScalef(1.0, 0.85, 1.05);
                glutSolidSphere(0.068, 14, 14);
            glPopMatrix();
            glColor3f(0.06, 0.06, 0.07);
            glPushMatrix();
                glTranslatef(0.0, 0.665, 0.20);
                glScalef(0.08, 0.03, 0.03);
                glutSolidCube(1.0);
            glPopMatrix();

            glColor3f(0.38, 0.22, 0.10);
            for (int side = -1; side <= 1; side += 2)
            {
                glPushMatrix();
                    glTranslatef(side * 0.12f, 0.44, 0.06);
                    glRotatef(48, 1.0, 0.0, 0.0);
                    glRotatef(side * 10.0f, 0.0, 1.0, 0.0);
                    glScalef(0.034, 0.40, 0.034);
                    glutSolidCube(0.5);
                glPopMatrix();
                glColor3f(0.08, 0.08, 0.09);
                glPushMatrix();
                    glTranslatef(side * 0.145f, 0.315, 0.335);
                    glutSolidSphere(0.034, 10, 10);
                glPopMatrix();
                glColor3f(0.38, 0.22, 0.10);
            }

            glColor3f(0.30, 0.17, 0.08);
            for (int side = -1; side <= 1; side += 2)
            {
                glPushMatrix();
                    glTranslatef(side * 0.09f, 0.24, -0.05);
                    glRotatef(70, 1.0, 0.0, 0.0);
                    glScalef(0.038, 0.26, 0.038);
                    glutSolidCube(0.5);
                glPopMatrix();
                glPushMatrix();
                    glTranslatef(side * 0.10f, 0.135, -0.05);
                    glRotatef(12, 1.0, 0.0, 0.0);
                    glScalef(0.034, 0.18, 0.034);
                    glutSolidCube(0.5);
                glPopMatrix();
            }

            glColor3f(0.28, 0.16, 0.07);
            for (int side = -1; side <= 1; side += 2)
            {
                glPushMatrix();
                    glTranslatef(side * 0.10f, 0.075, -0.06);
                    glScalef(0.05, 0.035, 0.11);
                    glutSolidCube(0.5);
                glPopMatrix();
            }

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
        glEnable(GL_BLEND);
        glTranslatef(0.0, 0.0, -55.0);

        glBegin(GL_QUADS);
            glColor3f(zenith_r, zenith_g, zenith_b);
            glVertex3f(-400.0, 90.0, 0);
            glVertex3f(400.0, 90.0, 0);
            glColor3f(sky_red, sky_green, sky_blue);
            glVertex3f(400.0, -6.0, 0);
            glVertex3f(-400.0, -6.0, 0);
        glEnd();

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

        drawClouds();

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
        glDisable(GL_BLEND);
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
            float h1 = (4.5f + 1.8f * sinf(z * 0.021f)) * heightMul;
            float h2 = (7.5f + 2.6f * cosf(z * 0.015f)) * heightMul;

            glPushMatrix();
                glColor3f(0.46f * dim, 0.53f * dim, 0.66f * dim);
                glTranslatef(-13.0, z, 0.0);
                glutSolidCone(3.4, h2, 4, 1);
            glPopMatrix();

            if (h2 > 8.0f)
            {
                glPushMatrix();
                    glColor3f(0.95f * dim, 0.95f * dim, 0.98f * dim);
                    glTranslatef(-13.0, z, h2 * 0.70f);
                    glutSolidCone(1.3, h2 * 0.32f, 4, 1);
                glPopMatrix();
            }

            glPushMatrix();
                glColor3f(0.30f * dim, 0.29f * dim, 0.25f * dim);
                glTranslatef(-9.0, z + 26, 0.0);
                glutSolidCone(2.5, h1, 4, 1);
            glPopMatrix();
        }
    }

void lamppost(float z)
{
    glPushMatrix();
        glColor3f(0.22f, 0.22f, 0.24f);
        glTranslatef(-1.05f, z, 0.62f);
        glScalef(0.045f, 0.045f, 1.24f);
        glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
        glColor3f(0.18f, 0.18f, 0.20f);
        glTranslatef(-1.05f, z, 0.02f);
        glScalef(0.09f, 0.09f, 0.05f);
        glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
        glColor3f(0.20f, 0.20f, 0.22f);
        glTranslatef(-0.82f, z, 1.22f);
        glScalef(0.40f, 0.04f, 0.04f);
        glutSolidCube(1.0f);
    glPopMatrix();

    glPushMatrix();
        glColor3f(0.12f, 0.12f, 0.13f);
        glTranslatef(-0.58f, z, 1.15f);
        glScalef(0.13f, 0.13f, 0.07f);
        glutSolidCube(1.0f);
    glPopMatrix();

    glEnable(GL_BLEND);
    glPushMatrix();
        glColor4ub(255, 245, 190, roadlight);
        glTranslatef(-0.58f, z, 1.06f);
        glutSolidSphere(0.05f, 10, 10);
    glPopMatrix();
    glDisable(GL_BLEND);
}

// ---------- NEW: driveway-zone check ----------
// Mirrors the driveway placements in commercial() (gas station, 4 shops,
// hospital) so roadside() can skip grass tufts that would otherwise
// render on top of the paved driveways.
bool isDrivewayZone(float z)
{
    const float margin = 0.3f;

    // gas station: driveway(20.0f, GAS_X, 1.3f)
    if (z > 20.0f - 1.3f - margin && z < 20.0f + 1.3f + margin) return true;

    // shops: 4 shops, spacing 14.0f starting at 120.0f, halfWidthZ 0.85f
    for (int s = 0; s < 4; s++)
    {
        float zc = 120.0f + s * 14.0f;
        if (z > zc - 0.85f - margin && z < zc + 0.85f + margin) return true;
    }

    // hospital: driveway(280.0f, HOSPITAL_X, 1.2f)
    if (z > 280.0f - 1.2f - margin && z < 280.0f + 1.2f + margin) return true;

    return false;
}

void roadside()
    {

     for (float z = -38; z < WORLD_LENGTH; z += 9.0f)
	{
		lamppost(z);
	}

        glBegin(GL_QUADS);
            glColor3ub(0,155,20);
			glVertex3f(-5.0, -10, -0.1);
			glVertex3f(-1.0, -10, -0.1);
			glVertex3f(-1.0, WORLD_LENGTH, -0.1);
			glVertex3f(-5.0, WORLD_LENGTH, -.1);
		glEnd();
		glBegin(GL_QUADS);
            glColor3ub(0, 155, 20);
			glVertex3f(1.0, -10, -.1);
			glVertex3f(8.0, -10, -.1);
			glVertex3f(8.0, WORLD_LENGTH, -.1);
			glVertex3f(1.0, WORLD_LENGTH, -.1);
		glEnd();

        // ---------- Grass "texture" ----------
        float gdim = isNight ? 0.55f : 1.0f;
        for (float z = -10; z < WORLD_LENGTH; z += 1.2f)
        {
            float jitter = sinf(z * 0.9f) * 0.20f;
            float shade  = 0.70f + 0.30f * sinf(z * 0.5f);

            // left verge tuft — never on a driveway (driveways are on the right)
            glPushMatrix();
                glColor3f(0.0f, 0.50f * shade * gdim, 0.04f * gdim);
                glTranslatef(-3.0f + jitter, z, 0.02f);
                glScalef(0.09f, 0.09f, 0.11f);
                glutSolidCone(0.5f, 1.2f, 4, 1);
            glPopMatrix();

            // right verge tuft — skip if it lands on a driveway (asphalt)
            if (!isDrivewayZone(z + 0.6f))
            {
                glPushMatrix();
                    glColor3f(0.0f, 0.55f * (1.05f - shade) * gdim + 0.10f, 0.04f * gdim);
                    glTranslatef(1.65f - jitter * 0.5f, z + 0.6f, 0.02f);
                    glScalef(0.09f, 0.09f, 0.11f);
                    glutSolidCone(0.5f, 1.2f, 4, 1);
                glPopMatrix();
            }
        }

        // ---------- Extra "unordered" dark-green grass ----------
        for (float z = -8; z < WORLD_LENGTH; z += 1.7f)
        {
            float rndA = sinf(z * 2.3f) * cosf(z * 0.7f);
            float rndB = cosf(z * 1.9f) * sinf(z * 1.1f);

            // left verge
            glPushMatrix();
                glColor3f(0.02f * gdim, 0.30f * gdim, 0.05f * gdim);
                glTranslatef(-3.0f + rndA * 1.0f, z + rndB * 0.5f, 0.02f);
                glRotatef(rndA * 40.0f, 0.0f, 0.0f, 1.0f);
                glScalef(0.07f + fabs(rndB) * 0.05f, 0.07f + fabs(rndB) * 0.05f, 0.09f + fabs(rndA) * 0.06f);
                glutSolidCone(0.5f, 1.2f, 4, 1);
            glPopMatrix();

            // right verge — skip if it lands on a driveway
            float rz = z + rndA * 0.5f;
            if (!isDrivewayZone(rz))
            {
                glPushMatrix();
                    glColor3f(0.02f * gdim, 0.28f * gdim, 0.05f * gdim);
                    glTranslatef(1.70f - rndB * 0.30f, rz, 0.02f);
                    glRotatef(rndB * 40.0f, 0.0f, 0.0f, 1.0f);
                    glScalef(0.07f + fabs(rndA) * 0.05f, 0.07f + fabs(rndA) * 0.05f, 0.09f + fabs(rndB) * 0.06f);
                    glutSolidCone(0.5f, 1.2f, 4, 1);
                glPopMatrix();
            }
        }

    }

    void brickWallFace(float w, float d, float h, float baseR, float baseG, float baseB)
    {
        glColor3f(baseR, baseG, baseB);
        glPushMatrix();
            glScalef(w, d, h);
            glutSolidCube(1.0f);
        glPopMatrix();

        float mortarR = baseR * 0.42f, mortarG = baseG * 0.42f, mortarB = baseB * 0.40f;
        glColor3f(mortarR, mortarG, mortarB);

        int rows = 6;
        int cols = 5;
        float faceX = -w / 2.0f - 0.006f;
        float rowH  = h / rows;
        float colW  = d / cols;

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
                float fz0 = -h / 2.0f + rowH * r;
                float fz1 = fz0 + rowH;
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
    }

    void house(){
     int idx = 0;
     for (float z = -40; z < JUNGLE_START; z +=9.6)
	{
		bool twoFloor = (idx % 2 == 0);

		float palette[4][3] = {
		    {0.95f, 0.90f, 0.80f},
		    {0.82f, 0.50f, 0.40f},
		    {0.72f, 0.78f, 0.84f},
		    {0.88f, 0.72f, 0.50f},
		};
		int pIdx = idx % 4;
		float wr = palette[pIdx][0], wg = palette[pIdx][1], wb = palette[pIdx][2];
		idx++;

		float winR = isNight ? 1.00f : 1.00f;
		float winG = isNight ? 0.82f : 1.00f;
		float winB = isNight ? 0.35f : 1.00f;

		glPushMatrix();
			glTranslatef(3.0, z, 0.0);

			if (twoFloor)
			{
				glColor3f(wr, wg, wb);
				glPushMatrix();
					glTranslatef(0.0, 0.0, .30);
					glutSolidCube(1);
				glPopMatrix();

				glColor3f(wr * 0.90f, wg * 0.90f, wb * 0.90f);
				glPushMatrix();
					glTranslatef(0.0, 0.0, 1.02);
					glScalef(0.82, 0.82, 0.70);
					glutSolidCube(1);
				glPopMatrix();

				glColor3f(winR, winG, winB);
				glPushMatrix(); glTranslatef(-0.45, 0.0, .40); glutSolidCube(.18); glPopMatrix();
				glPushMatrix(); glTranslatef( 0.45, 0.0, .40); glutSolidCube(.18); glPopMatrix();

				glPushMatrix(); glTranslatef(-0.32, 0.0, 1.08); glutSolidCube(.14); glPopMatrix();
				glPushMatrix(); glTranslatef( 0.32, 0.0, 1.08); glutSolidCube(.14); glPopMatrix();

				glColor3ub(60, 38, 20);
				glPushMatrix();
					glTranslatef(0.0, -.50, .18);
					glScalef(.22, .18, .55);
					glutSolidCube(1);
				glPopMatrix();

				// ---- NEW: porch light — glows warm at night, faint by day ----
				glEnable(GL_BLEND);
				glColor4ub(255, 235, 150, isNight ? 235 : 30);
				glPushMatrix();
					glTranslatef(0.16, -0.50, 0.42);
					glutSolidSphere(0.035, 10, 10);
				glPopMatrix();
				glDisable(GL_BLEND);

				glColor3ub(120, 45, 35);
				glPushMatrix();
					glTranslatef(3.0-3.0, z-z, 1.30);
					glutSolidCone(0.9, 0.65, 4, 6);
				glPopMatrix();
			}
			else
			{
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

				// ---- NEW: porch light ----
				glEnable(GL_BLEND);
				glColor4ub(255, 235, 150, isNight ? 235 : 30);
				glPushMatrix();
					glTranslatef(0.05, -0.48, 0.36);
					glutSolidSphere(0.032, 10, 10);
				glPopMatrix();
				glDisable(GL_BLEND);

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

			// ---- NEW: grass tufts behind the house (the side away from the road) ----
			float bgdim = isNight ? 0.55f : 1.0f;
			for (int t = 0; t < 5; t++)
			{
				float tx = 0.70f + 0.30f * fabs(sinf((float)t * 1.7f + z * 0.11f));
				float tz = -0.55f + t * 0.30f;
				glPushMatrix();
					glColor3f(0.02f * bgdim, 0.40f * bgdim + 0.05f, 0.06f * bgdim);
					glTranslatef(tx, tz, 0.02f);
					glRotatef(t * 25.0f, 0.0f, 0.0f, 1.0f);
					glScalef(0.09f, 0.09f, 0.13f);
					glutSolidCone(0.5f, 1.2f, 4, 1);
				glPopMatrix();
			}

		glPopMatrix();

	}
    }

    void driveway(float z, float toX, float halfWidthZ)
    {
        glColor3ub(58, 58, 60);
        glBegin(GL_QUADS);
            glVertex3f(1.05f, z - halfWidthZ, 0.004f);
            glVertex3f(toX,   z - halfWidthZ, 0.004f);
            glVertex3f(toX,   z + halfWidthZ, 0.004f);
            glVertex3f(1.05f, z + halfWidthZ, 0.004f);
        glEnd();

        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
            glVertex3f(1.05f, z - halfWidthZ, 0.005f); glVertex3f(toX, z - halfWidthZ, 0.005f);
            glVertex3f(1.05f, z + halfWidthZ, 0.005f); glVertex3f(toX, z + halfWidthZ, 0.005f);
        glEnd();
    }

    void gasStation(float z, float xPos)
    {
        glPushMatrix();
            glTranslatef(xPos, z, 0.0f);
            glScalef(2.0f, 2.0f, 1.4f);

            glColor3f(0.85f, 0.15f, 0.10f);
            glPushMatrix();
                glTranslatef(-0.30f, 0.0f, 0.85f);
                glScalef(1.3f, 1.6f, 0.06f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.60f, 0.60f, 0.62f);
            for (int s = -1; s <= 1; s += 2)
                for (int t = -1; t <= 1; t += 2)
                {
                    glPushMatrix();
                        glTranslatef(-0.30f + s * 0.55f, t * 0.65f, 0.42f);
                        glScalef(0.04f, 0.04f, 0.85f);
                        glutSolidCube(1.0f);
                    glPopMatrix();
                }

            for (int p = -1; p <= 1; p += 2)
            {
                glColor3f(0.90f, 0.90f, 0.90f);
                glPushMatrix();
                    glTranslatef(-0.30f, p * 0.30f, 0.16f);
                    glScalef(0.10f, 0.10f, 0.32f);
                    glutSolidCube(1.0f);
                glPopMatrix();
                glColor3f(0.85f, 0.15f, 0.10f);
                glPushMatrix();
                    glTranslatef(-0.30f, p * 0.30f, 0.30f);
                    glScalef(0.11f, 0.11f, 0.05f);
                    glutSolidCube(1.0f);
                glPopMatrix();
                glColor3f(0.08f, 0.08f, 0.08f);
                glPushMatrix();
                    glTranslatef(-0.30f + 0.07f, p * 0.30f, 0.20f);
                    glScalef(0.015f, 0.10f, 0.015f);
                    glutSolidCube(1.0f);
                glPopMatrix();
            }

            glPushMatrix();
                glTranslatef(0.55f, 0.0f, 0.30f);
                brickWallFace(0.55f, 0.9f, 0.55f, 0.80f, 0.55f, 0.35f);
            glPopMatrix();
            glColor3f(0.30f, 0.30f, 0.32f);
            glPushMatrix();
                glTranslatef(0.55f, 0.0f, 0.60f);
                glScalef(0.60f, 0.95f, 0.05f);
                glutSolidCube(1.0f);
            glPopMatrix();
            glColor3f(0.55f, 0.80f, 0.85f);
            glPushMatrix();
                glTranslatef(0.26f, 0.0f, 0.30f);
                glScalef(0.03f, 0.55f, 0.30f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.0f, 0.608f, 0.078f);
            glPushMatrix();
                glTranslatef(0.10f, 0.0f, 0.03f);
                glScalef(1.55f, 1.15f, 0.05f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.20f, 0.20f, 0.22f);
            glPushMatrix();
                glTranslatef(-1.10f, 0.90f, 0.55f);
                glScalef(0.04f, 0.04f, 1.10f);
                glutSolidCube(1.0f);
            glPopMatrix();
            glEnable(GL_BLEND);
            glColor4ub(250, 220, 20, isNight ? 255 : 220);
            glPushMatrix();
                glTranslatef(-1.10f, 0.90f, 1.15f);
                glScalef(0.35f, 0.05f, 0.30f);
                glutSolidCube(1.0f);
            glPopMatrix();
            glDisable(GL_BLEND);

        glPopMatrix();
    }

    void shop(float z, int shopIdx, float xPos)
    {
        float wallPalette[3][3] = {
            {0.82f, 0.35f, 0.28f},
            {0.78f, 0.66f, 0.44f},
            {0.58f, 0.58f, 0.62f},
        };
        float awningPalette[3][3] = {
            {0.90f, 0.20f, 0.20f},
            {0.20f, 0.50f, 0.85f},
            {0.15f, 0.60f, 0.25f},
        };
        int p = shopIdx % 3;
        bool useBrick = (p != 2);

        glPushMatrix();
            glTranslatef(xPos, z, 0.0f);
            glScalef(2.0f, 2.0f, 1.5f);

            if (useBrick)
            {
                brickWallFace(0.9f, 0.75f, 0.55f, wallPalette[p][0], wallPalette[p][1], wallPalette[p][2]);
            }
            else
            {
                glColor3f(wallPalette[p][0], wallPalette[p][1], wallPalette[p][2]);
                glPushMatrix();
                    glScalef(0.9f, 0.75f, 0.55f);
                    glutSolidCube(1.0f);
                glPopMatrix();
            }

            glColor3f(awningPalette[p][0], awningPalette[p][1], awningPalette[p][2]);
            glPushMatrix();
                glTranslatef(-0.50f, 0.0f, 0.30f);
                glScalef(0.06f, 0.85f, 0.05f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.60f, 0.80f, 0.88f);
            glPushMatrix();
                glTranslatef(-0.46f, -0.14f, 0.18f);
                glScalef(0.03f, 0.30f, 0.24f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3ub(50, 35, 20);
            glPushMatrix();
                glTranslatef(-0.46f, 0.32f, 0.10f);
                glScalef(0.03f, 0.16f, 0.20f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.0f, 0.608f, 0.078f);
            glPushMatrix();
                glTranslatef(-0.50f, 0.0f, 0.02f);
                glScalef(0.05f, 0.90f, 0.05f);
                glutSolidCube(1.0f);
            glPopMatrix();

        glPopMatrix();
    }

    void hospital(float z, float xPos)
    {
        glPushMatrix();
            glTranslatef(xPos, z, 0.0f);

            glPushMatrix();
                glTranslatef(0.0f, 0.0f, 0.35f);
                brickWallFace(1.6f, 1.1f, 0.7f, 0.88f, 0.85f, 0.80f);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, 1.05f);
                brickWallFace(1.6f, 1.1f, 0.7f, 0.90f, 0.87f, 0.83f);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, 1.75f);
                brickWallFace(1.6f, 1.1f, 0.7f, 0.92f, 0.89f, 0.85f);
            glPopMatrix();

            glColor3f(0.35f, 0.35f, 0.37f);
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, 2.12f);
                glScalef(1.65f, 1.15f, 0.06f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.55f, 0.75f, 0.90f);
            for (int floor = 0; floor < 3; floor++)
            {
                float fz = 0.35f + floor * 0.70f;
                for (int w = -2; w <= 2; w++)
                {
                    glPushMatrix();
                        glTranslatef(-0.82f, w * 0.20f, fz + 0.05f);
                        glScalef(0.03f, 0.13f, 0.20f);
                        glutSolidCube(1.0f);
                    glPopMatrix();
                }
            }

            glEnable(GL_BLEND);
            glColor4ub(220, 15, 15, isNight ? 255 : 230);
            glPushMatrix();
                glTranslatef(-0.83f, 0.0f, 2.30f);
                glScalef(0.05f, 0.35f, 0.10f);
                glutSolidCube(1.0f);
            glPopMatrix();
            glPushMatrix();
                glTranslatef(-0.83f, 0.0f, 2.30f);
                glScalef(0.05f, 0.10f, 0.35f);
                glutSolidCube(1.0f);
            glPopMatrix();
            glDisable(GL_BLEND);

            glColor3ub(70, 130, 180);
            glPushMatrix();
                glTranslatef(-0.82f, 0.0f, 0.12f);
                glScalef(0.05f, 0.30f, 0.24f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.0f, 0.608f, 0.078f);
            glPushMatrix();
                glTranslatef(-0.95f, 0.0f, 0.03f);
                glScalef(0.75f, 1.30f, 0.05f);
                glutSolidCube(1.0f);
            glPopMatrix();

            glColor3f(0.75f, 0.75f, 0.78f);
            glPushMatrix();
                glTranslatef(-1.05f, 0.0f, 0.62f);
                glScalef(0.35f, 0.9f, 0.03f);
                glutSolidCube(1.0f);
            glPopMatrix();

        glPopMatrix();
    }

    void commercial()
    {
        const float GAS_X = 4.2f;
        const float SHOP_X = 3.0f;
        const float HOSPITAL_X = 5.4f;

        driveway(20.0f, GAS_X, 1.3f);
        gasStation(20.0f, GAS_X);

        for (int s = 0; s < 4; s++)
        {
            float zc = 120.0f + s * 14.0f;
            driveway(zc, SHOP_X, 0.85f);
            shop(zc, s, SHOP_X);
        }
        driveway(280.0f, HOSPITAL_X, 1.2f);
        hospital(280.0f, HOSPITAL_X);
    }

    void realTree(float x, float zAlong, float trunkH, float canopyR,
                   float r, float g, float b, bool jungle)
    {
        float dim = isNight ? 0.6f : 1.0f;
        if (!treeQuad) treeQuad = gluNewQuadric();

        glPushMatrix();
            glTranslatef(x, zAlong, 0.0f);

            glColor3f(0.30f * dim, 0.19f * dim, 0.09f * dim);
            gluCylinder(treeQuad, trunkH * 0.09f, trunkH * 0.04f, trunkH, 7, 3);

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
        float heightMul = denseMode ? 1.55f : 1.15f;
        float step = denseMode ? 2.6f : 4.0f;

        for (float z = -40; z < WORLD_LENGTH + 40.0f; z += step)
	{
		if (z < JUNGLE_START)
		{
			realTree(-1.20f, z,       0.62f * heightMul, 0.28f, 0.16f, 0.55f, 0.14f, false);
			realTree( 1.20f, z + 1.6f, 0.58f * heightMul, 0.26f, 0.18f, 0.60f, 0.15f, false);
		}
		else
		{
			float jitter = sinf(z * 0.37f) * 0.15f;
			float scaleL = 0.65f + 0.20f * sinf(z * 0.21f);
			float scaleR = 0.65f + 0.20f * cosf(z * 0.19f);

			realTree(-1.15f + jitter, z,        1.40f * scaleL * heightMul, 0.44f * scaleL, 0.05f, 0.30f, 0.06f, true);
			realTree( 1.15f - jitter, z + 2.0f, 1.40f * scaleR * heightMul, 0.44f * scaleR, 0.06f, 0.34f, 0.07f, true);

			if (fmod(z, 12.0f) < 4.0f)
			{
				realTree(-1.55f, z + 1.0f, 1.10f * heightMul, 0.36f, 0.05f, 0.27f, 0.06f, true);
				realTree( 1.55f, z + 3.0f, 1.15f * heightMul, 0.38f, 0.06f, 0.31f, 0.07f, true);
			}

			if (denseMode && fmod(z, 12.0f) < 4.0f)
			{
				realTree(-1.85f, z + 2.0f, 1.0f * heightMul, 0.32f, 0.05f, 0.27f, 0.06f, true);
				realTree( 1.85f, z + 0.5f, 1.05f * heightMul, 0.34f, 0.06f, 0.31f, 0.07f, true);
			}
		}
	}

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

    void car(float laneX, float zp, float colorSeed)
    {
        float r = 0.55f + 0.35f * fabs(sinf(colorSeed));
        float g = 0.15f + 0.20f * fabs(cosf(colorSeed * 1.7f));
        float b = 0.20f + 0.30f * fabs(sinf(colorSeed * 0.6f));

        glPushMatrix();
            glTranslatef(laneX, zp, -0.02f);

            glColor3f(r, g, b);
            glPushMatrix();
                glScalef(0.30f, 0.55f, 0.16f);
                glutSolidCube(0.7);
            glPopMatrix();

            glColor3f(r * 0.8f, g * 0.8f, b * 0.8f);
            glPushMatrix();
                glTranslatef(0.0f, -0.03f, 0.14f);
                glScalef(0.22f, 0.30f, 0.10f);
                glutSolidCube(0.7);
            glPopMatrix();

            glColor3f(0.05f, 0.05f, 0.05f);
            float wx = 0.12f, wy = 0.14f, wz = -0.04f;
            glPushMatrix(); glTranslatef( wx,  wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();
            glPushMatrix(); glTranslatef(-wx,  wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();
            glPushMatrix(); glTranslatef( wx, -wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();
            glPushMatrix(); glTranslatef(-wx, -wy, wz); glutSolidTorus(0.02, 0.05, 6, 12); glPopMatrix();

            glEnable(GL_BLEND);
            glColor4ub(255, 250, 200, isNight ? 255 : 160);
            glPushMatrix(); glTranslatef( 0.08f, 0.27f, 0.02f); glutSolidSphere(0.025, 8, 8); glPopMatrix();
            glPushMatrix(); glTranslatef(-0.08f, 0.27f, 0.02f); glutSolidSphere(0.025, 8, 8); glPopMatrix();
            glDisable(GL_BLEND);

        glPopMatrix();
    }

    void objectcube()
    {
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

void finishLine()
    {
        float fz = FINISH_DISTANCE;

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

void drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_FOG);
	GLfloat fogColor[4] = { sky_red, sky_green, sky_blue, 1.0f };
	glFogfv(GL_FOG_COLOR, fogColor);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, fogNear);
	glFogf(GL_FOG_END, fogFar);
	glHint(GL_FOG_HINT, GL_NICEST);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-_cameraAngle, 0.0, 1.0, 0.0);
	glTranslatef(0.0, 0.0, camDistance);

	updateLighting();
	glDisable(GL_LIGHTING);

	sky();
	gamerbike();
    glPushMatrix();
	glTranslatef(0.0, 0.0, 0.0);
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
    commercial();
    roadside();
    objectcube();
    finishLine();
    GameScore();
    cout<<score<<endl;
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

    if (!raceFinished && -crmove >= FINISH_DISTANCE)
    {
        raceFinished = true;
        finishRace('a');
    }
    else if(collision())
    {
        winner('a');
    }
	glutSwapBuffers();
}

void update(int value) {

    int elapsed = glutGet(GLUT_ELAPSED_TIME) - gameStartTimeMs;

    denseMode = (elapsed > 30000);

    if (!paused)
    {
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
        }
    }

	glutPostRedisplay();

	glutTimerFunc(crspeed, update, 0);
}

int main(int argc, char** argv) {

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 500);
	glutInitWindowPosition(100,100);
	glutCreateWindow("Transformations");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	initRendering();
	buildObstacles();
	gameStartTimeMs = glutGet(GLUT_ELAPSED_TIME);
	glutDisplayFunc(drawScene);
	glutReshapeFunc(handleResize);
	glutTimerFunc(25, update, 0);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(keyboardown);
	glutMainLoop();
	return 0;
}
