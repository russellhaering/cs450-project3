/************************************************************************/
/*                         CS 450 Assignment3                           */
/*                           Russell Haering                            */
/*           Based on Assignment 2 Solution by Christophe Torne         */
/*                            11/3/2010                                 */
/************************************************************************/

#include <stdlib.h>

#ifdef __APPLE__
#include <GLUI/glui.h>
#else
#include <GL/glui.h>
#endif

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#define BACKGROUND_COLOR 0.5, 0.5, 0.5, 1.0
#define LOCAL_AXIS_SIZE 0.5
#define ROTATION_RATE (180 / 0.5)

using namespace std;

/************************************************************************/
/*                          DATA STRUCTURES                             */
/************************************************************************/

typedef struct
{
	float r, g, b;
} Color;

typedef struct
{
	int v, n;
} Point;

typedef struct Object
{
	Color color;
	string name;
	float translate[3];
	float scale[3];
	float rotate[16];
	float (*normals)[3];
	float (*vertices)[3];
	Point (*faces)[3];
	int numVertices, numNormals, numFaces;
	GLuint displayList;

} Object;

typedef struct {
	float p1[3];
	float p2[3];
} Line;

enum buttonTypes {OBJ_TEXTFIELD = 0, LOAD_BUTTON};
enum colors	{RED = 0, GREEN, BLUE};
enum projections {ORTHO = 0, PERSP};
enum transformations {TRANS = 0, ROT, SCALE};
enum camTransforms {CAMROTATE = 0 , TRACK, DOLLY};
const int FOV = 2;

/************************************************************************/
/*                       FUNCTION PROTOTYPES                            */
/************************************************************************/

int		loadObj			(char *, Object &);
void	setupVV			(void);
void	projCB			(int);
void	textCB			(int);
void	buttonCB		(int);
void	colorCB			(int);
void	drawObjects		(GLenum);
void	myGlutDisplay	(void);
void	myGlutReshape	(int, int);
void	myGlutMouse		(int , int , int , int);
void	processHits		(GLint, GLuint[]);
void	initScene		();

/************************************************************************/
/*                   CONSTANTS & GLOBAL VARIABLES                       */
/************************************************************************/

vector<Object> Objects;

const int WIN_WIDTH = 500;
const int WIN_HEIGHT = 500;

int main_window;
int objSelected = -1;

float red = 1.f;
float green = 1.f;
float blue = 1.f;

int fov = 30;
const int FOVMIN = 0;
const int FOVMAX = 90;

projections projType = ORTHO;
transformations transType = TRANS;

int sizeX = WIN_WIDTH;
int sizeY = WIN_WIDTH;
float vl = -1.f;
float vr = 1.f;
float vb = -1.f;
float vt = 1.f;
const float vn = .5;
const float vf = 10;

float scaleFactor	= 1.f;
float camRotMat[16];
float camTrack[2] = {0.0, 0.0};
float camDolly = -2.0;

int sManip = -1;
float prevDrag[3] = {0.0, 0.0, 0.0};

GLUI *glui;
GLUI_EditText *objFileNameTextField;
GLUI_String fileName = GLUI_String("frog.obj");

float light0_pos[]	=	{0.f, 3.f, 2.f, 1.f};
float diffuse0[]	=	{1.f, 1.f, 1.f, .5f};
float ambient0[]	=	{.1f, .1f, .1f, 1.f};
float specular0[]	=	{1.f, 1.f, 1.f, .5f};

/************************************************************************/
/*                       FUNCTION DEFINITIONS                           */
/************************************************************************/

// Dot product of two 3d vectors
#define dot3(a, b)	((a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]))

// Subtract two 3d vectors into a third
void sub3(float *a, float *b, float *r) {
	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
	r[2] = a[2] - b[2];
}

// Identifies the closest point on line l1 to line l2
void closestApproach(Line l1, Line l2, float *point) {
	float u[3], v[3], w[3], a, b, c, d, e, D, sc;

	sub3(l1.p2, l1.p1, u);
	sub3(l2.p2, l2.p1, v);
	sub3(l1.p1, l2.p1, w);

	a = dot3(u, u);
	b = dot3(u, v);
	c = dot3(v, v);
	d = dot3(u, w);
	e = dot3(v, w);

	D = (a * c) - (b * b);

	if (D < .0001) {
		// The lines are nearly parallel
		sc = 0.0;
	}
	else {
		sc = ((b * e) - (c * d)) / D;
	}

	point[0] = l1.p1[0] + u[0] * sc;
	point[1] = l1.p1[1] + u[1] * sc;
	point[2] = l1.p1[2] + u[2] * sc;
}

/// Reads the contents of the obj file and appends the data at the end of
/// the vector of Objects. This time we allow the same object to be loaded
/// several times.
int loadObj(char *fileName, Object &obj)
{
	ifstream file;
	file.open(fileName);

	if (!file.is_open())
	{
		cerr << "Cannot open .obj file " << fileName << endl;
		return EXIT_FAILURE;
	}

	obj.name = string(fileName);
	obj.color.r = 1.f;
	obj.color.g = 1.f;
	obj.color.b = 1.f;

	/// First pass: count the vertices, normals and faces, allocate the arrays.
	int numVertices = 0, numNormals = 0, numFaces = 0;
	string buffer;
	while (getline(file, buffer), !buffer.empty())
	{
		if (buffer[0] == 'v')
		{
			if (buffer[1] == 'n')
				numNormals++;
			else
			{
				if (buffer[1] == ' ')
					numVertices++;
			}
		}
		else
		{
			if (buffer[0] == 'f')
				numFaces++;
		}
	};

	obj.vertices = new float [numVertices][3];
	obj.numVertices = numVertices;
	obj.normals = new float [numNormals][3];
	obj.numNormals = numNormals;
	obj.faces = new Point [numFaces][3];
	obj.numFaces = numFaces;

	// Initialize translations
	obj.translate[0] = 0.0;
	obj.translate[1] = 0.0;
	obj.translate[2] = 0.0;

	// Initialize the scales
	obj.scale[0] = 1.0;
	obj.scale[1] = 1.0;
	obj.scale[2] = 1.0;

	// Initialize the rotation matrix
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glGetFloatv(GL_MODELVIEW, obj.rotate);
	glPopMatrix();

	file.clear();
	file.seekg(ios::beg);

	/// Second pass: populate the arrays
	numFaces = numNormals = numVertices = 0;
	while (getline(file, buffer), !buffer.empty())
	{
		if (buffer[0] == 'v')
		{
			if (buffer[1] == 'n')
			{
				sscanf(	buffer.data() + 2*sizeof(char), " %f %f %f",
							&obj.normals[numNormals][0],
							&obj.normals[numNormals][1],
							&obj.normals[numNormals][2]);
				numNormals++;
			}
			else
			{
				if (buffer[1] == ' ')
				{
					sscanf(	buffer.data() + sizeof(char), " %f %f %f",
								&obj.vertices[numVertices][0],
								&obj.vertices[numVertices][1],
								&obj.vertices[numVertices][2]);
					numVertices++;
				}
			}
		}
		else
		{
			if (buffer[0] == 'f')
			{
				sscanf(	buffer.data() + sizeof(char), " %d//%d %d//%d %d//%d",
							&obj.faces[numFaces][0].v, &obj.faces[numFaces][0].n,
							&obj.faces[numFaces][1].v, &obj.faces[numFaces][1].n,
							&obj.faces[numFaces][2].v, &obj.faces[numFaces][2].n);
				numFaces++;
			}
		}
	};

	file.close();
	cout << "Finished loading " << fileName << endl;

	/// Now we generate the display list.
	obj.displayList = glGenLists(1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glNewList(obj.displayList, GL_COMPILE);
	{
		glBegin(GL_TRIANGLES);
		{
			for (int faceNum = 0 ; faceNum < obj.numFaces ; faceNum++)
			{
				glNormal3fv(obj.normals[obj.faces[faceNum][0].n - 1]);
				glVertex3fv(obj.vertices[obj.faces[faceNum][0].v - 1]);
				glNormal3fv(obj.normals[obj.faces[faceNum][1].n - 1]);
				glVertex3fv(obj.vertices[obj.faces[faceNum][1].v - 1]);
				glNormal3fv(obj.normals[obj.faces[faceNum][2].n - 1]);
				glVertex3fv(obj.vertices[obj.faces[faceNum][2].v - 1]);
			}
		}
		glEnd();
	}
	glEndList();

	return EXIT_SUCCESS;
}

void transCB(int id)
{
	switch(transType)
	{
		case TRANS:
			printf("Translation Mode\n");
			break;
		case ROT:
			printf("Rotation Mode\n");
			break;
		case SCALE:
			printf("Scale Mode\n");
			break;
		default:break;
	}
	glui->sync_live();
	glutPostRedisplay();
}

void projCB(int id)
{
		switch(projType)
		{
			case ORTHO:
				printf("Orthographic Projection\n");
				break;
			case PERSP:
				printf("Perspective Projection \n");
				break;
			default:
				break;
		}
	setupVV();

	glui->sync_live();
	glutPostRedisplay();
}

void fovCB(int id)
{
	setupVV();
	glutPostRedisplay();
}

void camRotationCB(int id) {
	printf("Rotating the scene/camera\n");
}

void dollyCB(int id) {
	glui->sync_live();
	printf("Dollying the scene/camera\n");
}

void trackXYCB(int id) {
	printf("Tracking the scene/camera\n");
}


void textCB(int id)
{
	glui->sync_live();
	glutPostRedisplay();
}

void buttonCB(int control)
{
	Objects.push_back(Object());
	printf("Loading %s\n", objFileNameTextField->get_text());
	loadObj((char *)objFileNameTextField->get_text(), Objects.back());
	glui->sync_live();
	glutPostRedisplay();
}

void colorCB(int id)
{
	glui->sync_live();
	if (objSelected != -1)
	{
		Objects.at(objSelected).color.r = red;
		Objects.at(objSelected).color.g = green;
		Objects.at(objSelected).color.b = blue;
	}
	glui->sync_live();
	glutPostRedisplay();
}

void drawObjects(GLenum mode)
{
	glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular0);

	for (int i = 0; i < (int) Objects.size(); i++)
	{
		glPushMatrix();

		Object *obj = &Objects.at(i);
		glTranslatef(obj->translate[0], obj->translate[1], obj->translate[2]);
		glScalef(obj->scale[0], obj->scale[1], obj->scale[2]);
		glMultMatrixf(obj->rotate);

		glLoadName(i + 3);
		glColor3f(obj->color.r, obj->color.g, obj->color.b);

		// Display the object normally
		if ((mode == GL_SELECT) || ((mode == GL_RENDER) && (objSelected !=i)))
			glCallList(obj->displayList);

		// Display selected objects in wireframe
		if ((mode == GL_RENDER) && (objSelected == i)) {
			glDisable(GL_LIGHTING);

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glColor3f(1.f, 0, 0);
			glCallList(obj->displayList);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			
			glEnable(GL_LIGHTING);
		}

		glPopMatrix();

		// Display object axes manipulators
		if (objSelected == i)
		{
			glPushMatrix();
			glTranslatef(obj->translate[0], obj->translate[1], obj->translate[2]);

			// Draw the object's axes in render mode only
			if (mode == GL_RENDER) {
				glDisable(GL_LIGHTING);

				glLineWidth(3);
				glBegin(GL_LINES);
				{
					// X Axis - Red
					glColor3f(1.0, 0.0, 0.0);
					glVertex3f(0.0, 0.0, 0.0);
					glVertex3f(obj->scale[0] * LOCAL_AXIS_SIZE, 0.0, 0.0);

					// Y Axis - Green
					glColor3f(0.0, 1.0, 0.0);
					glVertex3f(0.0, 0.0, 0.0);
					glVertex3f(0.0, obj->scale[1] * LOCAL_AXIS_SIZE, 0.0);

					// Z Axis - Blue
					glColor3f(0.0, 0.0, 1.0);
					glVertex3f(0.0, 0.0, 0.0);
					glVertex3f(0.0, 0.0, obj->scale[2] * LOCAL_AXIS_SIZE);
				}
				glEnd();
				glLineWidth(1);

				glEnable(GL_LIGHTING);
			}

			// Draw the object's manipulators regardless

			// X Axis - Red
			glColor3f(1.0, 0.0, 0.0);
			glPushMatrix();
			glTranslatef(obj->scale[0] * LOCAL_AXIS_SIZE, 0.0, 0.0);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			glLoadName(0);
			glutSolidCone(.04, .1, 10, 10);
			glPopMatrix();

			// Y Axis - Green
			glColor3f(0.0, 1.0, 0.0);
			glPushMatrix();
			glTranslatef(0.0, obj->scale[1] * LOCAL_AXIS_SIZE, 0.0);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			glLoadName(1);
			glutSolidCone(.04, .1, 10, 10);
			glPopMatrix();

			// Z Axis - Blue
			glColor3f(0.0, 0.0, 1.0);
			glPushMatrix();
			glTranslatef(0.0, 0.0, obj->scale[2] * LOCAL_AXIS_SIZE);
			glLoadName(2);
			glutSolidCone(.04, .1, 10, 10);
			glPopMatrix();

			glPopMatrix();
		}
	}
}

void setupVV()
{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		if (projType == ORTHO)
			glOrtho(vl, vr, vb, vt, vn, vf);
		else
			glFrustum(vl * fov/FOVMAX, vr * fov/FOVMAX, vb * fov/FOVMAX, vt * fov/FOVMAX, vn, vf);
}



void myGlutDisplay(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* The next 3 lines are temporary to make sure the objects show up in the
	 * view volume when you load them. You will need to modify this
	 */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(camTrack[0], camTrack[1], camDolly);
	glMultMatrixf(camRotMat);

	glColor3f(0.0, 0.0, 0.0);
	glBegin(GL_LINES);
	{
		glVertex3f(0.0, 0.0, 20.0);
		glVertex3f(0.0, 0.0, -20.0);
		glVertex3f(0.0, 20.0, 0.0);
		glVertex3f(0.0, -20.0, 0.0);
		glVertex3f(20.0, 0.0, 0.0);
		glVertex3f(-20.0, 0.0, 0.0);
	}
	glEnd();

	drawObjects(GL_RENDER);
	glutSwapBuffers();
}


void myGlutReshape(int x, int y)
{
	sizeX = x;
	sizeY = y;
	glViewport(0, 0, sizeX, sizeY);

	if (sizeX <= sizeY)
	{
		vb = vl * sizeY / sizeX;
		vt = vr * sizeY / sizeX;
	}
	else
	{
		vl = vb * sizeX / sizeY;
		vr = vt * sizeX / sizeY;
	}

	setupVV();

	glutPostRedisplay();
}

void updateDrag(int x, int y, bool move) {
	GLint vp[4];
	GLdouble mv[16];
	GLdouble pj[16];
	GLdouble vX, vY, vZ;
	Line mRay, oAxis;
	float intersect[3];

	glGetIntegerv(GL_VIEWPORT, vp);
	glGetDoublev(GL_MODELVIEW_MATRIX, mv);
	glGetDoublev(GL_PROJECTION_MATRIX, pj);

	// Project a ray deep into the scene where the mouse clicked
	gluUnProject((float) x, (float) (vp[3] - y), 1.0, mv, pj, vp, &vX, &vY, &vZ);

	mRay.p2[0] = vX;
	mRay.p2[1] = vY;
	mRay.p2[2] = vZ;

	// Project a ray shallow into the scene where the mouse clicked
	gluUnProject((float) x, (float) (vp[3] - y), 0.0, mv, pj, vp, &vX, &vY, &vZ);

	mRay.p1[0] = vX;
	mRay.p1[1] = vY;
	mRay.p1[2] = vZ;

	oAxis.p1[0] = 0;
	oAxis.p1[1] = 0;
	oAxis.p1[2] = 0;

	oAxis.p2[0] = 0;
	oAxis.p2[1] = 0;
	oAxis.p2[2] = 0;

	oAxis.p2[sManip] = 1.0;

	closestApproach(oAxis, mRay, intersect);
	cout << "Closest To: (" << intersect[0] << ", " << intersect[1] << ", " << intersect[2] << ")" << endl;

	if (move) {
		Object *obj = &Objects.at(objSelected);
		float distance = intersect[sManip] - prevDrag[sManip];
		float vec[3] = {0.0, 0.0, 0.0};

		cout << "Moved By: " << distance << endl;

		switch (transType) {
			case TRANS:
				// Update the objects translation parameters
				obj->translate[sManip] += distance;
				break;
			case ROT:
				// Load the object's rotation matrix, apply rotations, then
				// store the matrix back
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
				glLoadMatrixf(obj->rotate);
				vec[sManip] = 1.0;
				glRotatef(distance * ROTATION_RATE, vec[0], vec[1], vec[2]);
				glGetFloatv(GL_MODELVIEW, obj->rotate);
				glPopMatrix();
				break;
			case SCALE:
				obj->scale[sManip] += (distance / LOCAL_AXIS_SIZE);
				if (obj->scale[sManip] < 0) obj->scale[sManip] = 0;
				break;
		}
	}

	prevDrag[0] = intersect[0];
	prevDrag[1] = intersect[1];
	prevDrag[2] = intersect[2];

}

void myGlutMouse(int button, int button_state, int x, int y)
{
	GLuint selectBuffer[512];
	GLint viewport[4];

	if (button == GLUT_LEFT_BUTTON) {
		if (button_state == GLUT_DOWN) {
			glGetIntegerv(GL_VIEWPORT, viewport);

			glSelectBuffer(512, selectBuffer);
			glRenderMode(GL_SELECT);

			glInitNames();
			glPushName(-1);

			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();

			gluPickMatrix((GLdouble) x, (GLdouble) (viewport[3]-y), 2.0, 2.0, viewport);

			if (projType == ORTHO)
				glOrtho(vl, vr, vb, vt, vn, vf);
			else
				glFrustum(vl * fov/FOVMAX, vr * fov/FOVMAX, vb * fov/FOVMAX, vt * fov/FOVMAX, vn, vf);

			glMatrixMode(GL_MODELVIEW);
			drawObjects(GL_SELECT);

			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glFlush();

			processHits(glRenderMode(GL_RENDER), selectBuffer);

			if (sManip >= 0) {
				updateDrag(x, y, false);
			}
		}
		else if (button_state == GLUT_UP) {
			if (sManip >= 0) {
				updateDrag(x, y, true);
				cout << "Released Manipulator: " << sManip << endl;
			}
			sManip = -1;
		}
	}

	glutPostRedisplay();
}

void glutMotionCB(int x, int y) {
	if (sManip >= 0) {
		updateDrag(x, y, true);
	}

	glutPostRedisplay();
}

void processHits(GLint hits, GLuint buffer[])
{
	int pick = -1;
	unsigned int min_depth = 4294967295;
	GLuint *ptr = (GLuint *) buffer;

	cout << "Number of hits = " << hits << endl;
	for(int i = 0 ; i < hits ; i++, ptr+=4)
	{
		if (*(ptr+1) < min_depth)
		{
			min_depth = *(ptr+1);
			pick = *(ptr+3);
		}
	}

	if (pick >= 3) {
		objSelected = pick - 3;
		cout << "We got ourselves a winner: #" << objSelected << endl;
		red = Objects.at(objSelected).color.r;
		green = Objects.at(objSelected).color.g;
		blue = Objects.at(objSelected).color.b;
	}
	else if (pick >= 0) {
		sManip = pick;
		cout << "Manipulator Selected: " << sManip << endl;
	}
	else {
		objSelected = -1;
		red = 1.f;
		green = 1.f;
		blue = 1.f;
	}
	glui->sync_live();
}

void initScene()
{
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glClearColor(BACKGROUND_COLOR);
	glPolygonOffset(0, -10);

	setupVV();
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowPosition(50, 50);
	glutInitWindowSize(WIN_WIDTH, WIN_HEIGHT);

	main_window = glutCreateWindow("OBJ Loader");
	glutDisplayFunc(myGlutDisplay);
	glutReshapeFunc(myGlutReshape);
	glutMouseFunc(myGlutMouse);
	glutMotionFunc(glutMotionCB);

	initScene();

	glui = GLUI_Master.create_glui("OBJ Loader GUI", 0, 600, 50);

	GLUI_Panel *objPanel = glui->add_panel("Obj Files");
	objFileNameTextField = glui->add_edittext_to_panel(objPanel, "Filename:", GLUI_EDITTEXT_TEXT, 0, OBJ_TEXTFIELD, textCB);

	glui->add_button_to_panel(objPanel, "Load", LOAD_BUTTON, buttonCB);

	glui->add_separator();

	GLUI_Panel *transformationsPanel = glui->add_panel("Object Transformation Mode");
	GLUI_RadioGroup *transGroup = glui->add_radiogroup_to_panel(transformationsPanel, (int *)&transType, -1, transCB);
	glui->add_radiobutton_to_group(transGroup, "Translation");
	glui->add_radiobutton_to_group(transGroup, "Rotation");
	glui->add_radiobutton_to_group(transGroup, "Scale");


	glui->add_separator();
	GLUI_Panel *cameraPanel = glui->add_panel("Camera Manipulation Mode");
	GLUI_Rotation *camRotationManip = glui->add_rotation_to_panel(cameraPanel, "Camera Rotation", camRotMat, CAMROTATE, camRotationCB);
	camRotationManip->reset();
	glui->add_column_to_panel(cameraPanel, true);
	GLUI_Translation *trackXYManip = glui->add_translation_to_panel(cameraPanel, "Track XY", GLUI_TRANSLATION_XY, camTrack, TRACK, trackXYCB);
	trackXYManip->set_speed(.005);
	glui->add_column_to_panel(cameraPanel, true);
	GLUI_Translation *dollyManip = glui->add_translation_to_panel(cameraPanel, "Dolly", GLUI_TRANSLATION_Z, &camDolly, DOLLY, dollyCB);
	dollyManip->set_speed(.005);
	glui->add_separator();


	GLUI_Panel *projPanel = glui->add_panel("Projection");
	GLUI_RadioGroup *projGroup = glui->add_radiogroup_to_panel(projPanel, (int *)&projType, -1, projCB);
	glui->add_radiobutton_to_group(projGroup, "Orthographic");
	glui->add_radiobutton_to_group(projGroup, "Perspective");
	GLUI_Spinner *fovSpinner = glui->add_spinner_to_panel(projPanel, "FOV", GLUI_SPINNER_INT, &fov, FOV, fovCB);
	fovSpinner->set_int_limits(FOVMIN, FOVMAX);

	GLUI_Panel *colorPanel = glui->add_panel("Color");
	GLUI_Spinner *redValue = glui->add_spinner_to_panel(colorPanel, "Red", GLUI_SPINNER_FLOAT, &red, RED, colorCB);
	redValue->set_float_limits(0.f, 1.f);

	GLUI_Spinner *greenValue = glui->add_spinner_to_panel(colorPanel, "Green", GLUI_SPINNER_FLOAT, &green, GREEN, colorCB);
	greenValue->set_float_limits(0.f, 1.f);

	GLUI_Spinner *blueValue = glui->add_spinner_to_panel(colorPanel, "Blue", GLUI_SPINNER_FLOAT, &blue, BLUE, colorCB);
	blueValue->set_float_limits(0.f, 1.f);

	glui->set_main_gfx_window(main_window);

	// We register the idle callback with GLUI, *not* with GLUT
	GLUI_Master.set_glutIdleFunc(NULL);
	glui->sync_live();
	glutMainLoop();
	return EXIT_SUCCESS;
}
