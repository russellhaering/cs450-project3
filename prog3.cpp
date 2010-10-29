#include <stdlib.h> /* Must come first to avoid redef error */
#include <stdio.h>

#ifdef __APPLE__
#include <GLUI/glui.h>
#else
#include <GL/glui.h>
#endif

#include "obj.h"

/** Constants and enums **/
enum buttonTypes {OBJ_TEXTFIELD = 0, LOAD_BUTTON};
enum colors {RED, GREEN, BLUE};
enum projections {ORTHO, PERSP, FOV};
enum transforms {
  TRANSLATIONXY = 0,
  TRANSLATIONZ,
  ROTATION,
  SCALE
};
enum cameraOps {CAMROTATE = 0, TRACK, DOLLY};

#define WIN_WIDTH         1000
#define WIN_HEIGHT        500

#define ORTHO_DENOMINATOR 100

#define SBUF_SIZE         64

#define MAXD              0xFFFFFFFF

#define MAX_COLOR         255
#define WIREFRAME_OFFSET  -100.0

#define CLIPPING_NEAR     0.1
#define CLIPPING_FAR      20

#define PERSP_CAM_LOC     0, 0, .3
#define PERSP_CAM_TGT     0, .1, 0
#define PERSP_CAM_UP      0, 1, 0

#define ORTHO_CAM_LOC     0, 0, .3
#define ORTHO_CAM_TGT     0, .1, 0
#define ORTHO_CAM_UP      0, 1, 0


/** These are the live variables modified by the GUI ***/
int main_window;
int red = MAX_COLOR;
int green = MAX_COLOR;
int blue = MAX_COLOR;
int fov = 90;
int projType = ORTHO;
float rotMat[16];
float xyTrans[2] = {0.0,0.0};
float zTrans = 0.0;
float objScale = 1.0;
float camRotMat[16];
float camTrack[2] = {0.0, 0.0};
float camDolly = 0.0;

/** Globals **/
struct obj_data *data = NULL;
GLUI *glui;
GLUI_EditText *objFileNameTextField;
GLUI_Spinner *fovSpinner;
int selected = -1;

/**
 * update_projection - configure the specifed projection mode on the current
 * matrix (you probably want GL_PROJECTION).
 */
void update_projection()
{
  GLint viewport[4];
  float x_offset;
  float y_offset;

  // Load the viewport size
  glGetIntegerv(GL_VIEWPORT, viewport);

  switch (projType) {
  case ORTHO:
    // Configure orthographic projection
    x_offset = ((float) viewport[2] / ORTHO_DENOMINATOR);
    y_offset = ((float) viewport[3] / ORTHO_DENOMINATOR);
    glOrtho(-x_offset, x_offset, -y_offset, y_offset, CLIPPING_NEAR, CLIPPING_FAR);
    gluLookAt(ORTHO_CAM_LOC,
              ORTHO_CAM_TGT,
              ORTHO_CAM_UP);
    break;

  case PERSP:
    // Configure perspective projection
    gluPerspective(fov, ((float) viewport[2] / (float) viewport[3]), CLIPPING_NEAR, CLIPPING_FAR);
    gluLookAt(PERSP_CAM_LOC,
              PERSP_CAM_TGT,
              PERSP_CAM_UP);
    break;

  case FOV:
    // This is used once for something apparently unrelated to the enum its in

  default:
    printf("Uh Oh\n");
  }
}

/**
 * projection_cb - called when projection (or color, for soem reason)
 * parameters are modified, updates the projection mode and fires a redisplay
 */
void projection_cb(int id)
{
  // Set up the projection matrix
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  update_projection();

  // Set up the modelview matrix
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Redisplay
  glutPostRedisplay();
}

/**
 * text_cb - fired when the filename text is modified, not much reason
 * to do anything in here.
 */
void text_cb(int id)
{
}

/**
 * button_cb - fired when the load file button is pressed, loads the
 * currently specified file and fires a redisplay
 */
void button_cb(int control)
{
  struct obj_data *d, *curr;
  d = load_obj_file(objFileNameTextField->get_text());

  if (data == NULL) {
    data = d;
  }
  else {
    curr = data;
    while (curr->next != NULL) {
      curr = curr->next;
    }
    curr->next = d;
  }
  glutPostRedisplay();
}

/**
 * color_cb - fired when the color parameters change, but so is
 * the projection callback. This doesn't do anything to avoid a double
 * redisplay.
 */
void color_cb(int id)
{
}

/**
 * draw_object - draw the specified object in the current polygon mode
 */
void draw_object(struct obj_data *curr)
{
    int i, j;
    struct face *f;
    struct vertex *v;
    struct vertex_normal *vn;

    // For now we are only supporting triangles
    glBegin(GL_TRIANGLES);

    for (i = 0; i < curr->faces->count; i++) {
      f = (struct face *) curr->faces->items[i];
      if (f->count != 3) {
        printf("Skipping non-triangle face at index %d\n", i);
        continue;
      }
      for (j = 0; j < f->count; j++) {
        // Set the normal if there is one
        if (f->vns != NULL) {
          vn = (struct vertex_normal *) f->vns[j];
          glNormal3f(vn->x, vn->y, vn->z);
        }

        // Set the vertex (there should always be one)
        v = (struct vertex *) f->vs[j];
        glVertex3f(v->x, v->y, v->z);
      }
    }

    printf("Rendered %d faces\n", i);
    glEnd();
}

/**
 * draw_objects - draw all loaded objects, with a wireframe and color on the
 * selected object.
 */
void draw_objects(void)
{
  int i;
  struct obj_data *curr;

  curr = data;
  i = 0;
  while (curr != NULL) {
    glPushName(i);

    // Set the color - white for non-selected, colored for selected
    if (i == selected) {
      glColor3f(((float) red / MAX_COLOR), ((float) green / MAX_COLOR), ((float) blue / MAX_COLOR));
    }
    else {
      glColor3f(1, 1, 1);
    }

    // Draw the object
    draw_object(curr);

    // Draw the wireframe on selected objects
    if (i == selected) {
      // Switch to line mode
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

      // Draw red lines
      glColor3f(1, 0, 0);
      draw_object(curr);

      // Switch back to fill mode
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glPopName();

    curr = curr->next;
    i++;
  }
  printf("Rendered %d objects\n", i);
}

/**
 * display_cb - the display callback, called to draw the scene. Clears
 * the scene, draws the objects then flushes and swaps the bufers.
 */
void display_cb()
{
  glClear(GL_COLOR_BUFFER_BIT  | GL_DEPTH_BUFFER_BIT);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  draw_objects();
  glFlush();
  glutSwapBuffers();
}

/**
 * reshape_cb - called when the window is reshaped, updates the viewport
 * and projection then redraws the scene.
 */
void reshape_cb (int x, int y)
{
  // Update the viewport
  glViewport(0, 0, x, y);

  // Set up the projection matrix
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  update_projection();

  // Set up the modelview matrix
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

/**
 * mouse_cb - called on mouse clicks, updates the global 'selected'
 * ID to reflect which object was clicked, then redraws the scene.
 */
void mouse_cb (int button, int state, int x, int y)
{
  int hits, i, names;
  GLuint selectBuf[SBUF_SIZE] = {0};
  GLuint min, *cur;
  GLint viewport[4];

  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
    // Get the viewport parameters
    glGetIntegerv(GL_VIEWPORT, viewport);
    glSelectBuffer(SBUF_SIZE, selectBuf);

    // Go into selection mode and set up the projection and modeliew matrices
    glRenderMode(GL_SELECT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    gluPickMatrix(x, (viewport[3] - y), 2, 2, viewport);
    update_projection();

    glMatrixMode(GL_MODELVIEW);

    // Set up the names stack then draw the scene
    glInitNames();
    draw_objects();

    // Restore render mode and associated matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glFlush();

    hits = glRenderMode(GL_RENDER);

    printf("%d hits\n", hits);

    min = MAXD;
    cur = selectBuf;

    // Find which object was hit if one was
    if (hits > 0) {
      for (i = 0; i < hits; i++) {
        names = *cur;

        if (*(cur + 1) < min) {
          min = *(cur + 1);
          if (names > 0) {
            selected = *(cur + 3);
          }
        }

        cur += (3 + names);
      }
    }
    // Otherwise no objects are selected
    else {
      selected = -1;
    }

    // Redraw the scene to update color/wireframe
    glutPostRedisplay();
  }
}

void rotation_cb(int id)
{
}

void translate_xy_cb(int id)
{
}

void translate_z_cb(int id)
{
}

void scale_cb(int id)
{
}

void cam_rotation_cb(int id)
{
}

void track_xy_cb(int id)
{
}

void dolly_cb(int id)
{
}

void init_scene()
{
  // This stuff is for lighting and materials. We'll learn more about
  // it later.
  float light0_pos[] = {0.0, 3.0, 0.0, 1.0};
  float diffuse0[] = {1.0, 1.0, 1.0, 0.5};
  float ambient0[] = {0.1, 0.1, 0.1, 1.0};
  float specular0[] = {1.0, 1.0, 1.0, 0.5};

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_COLOR_MATERIAL);
  glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse0);
  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient0);
  glLightfv(GL_LIGHT0, GL_SPECULAR, specular0);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_POLYGON_OFFSET_LINE);
  glPolygonOffset(0, WIREFRAME_OFFSET);
}

int main(int argc, char **argv)
{
  // setup glut
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitWindowPosition(50, 50);
  glutInitWindowSize(WIN_WIDTH, WIN_HEIGHT);

  // Create the GUI window
  main_window = glutCreateWindow("OBJ Loader");
  glutDisplayFunc(display_cb);
  glutReshapeFunc(reshape_cb);
  glutMouseFunc(mouse_cb);

  // Initialize OpenGL
  init_scene();

  glui = GLUI_Master.create_glui("OBJ Loader GUI", 0);

  // Object Loading Panel
  GLUI_Panel *objPanel = glui->add_panel("Obj Files");
  objFileNameTextField = glui->add_edittext_to_panel(objPanel, "Filename:", GLUI_EDITTEXT_TEXT, 0, OBJ_TEXTFIELD, text_cb);
  glui->add_button_to_panel(objPanel, "Load", LOAD_BUTTON, button_cb);

  glui->add_separator();

  // Projection Type Panel
  GLUI_Panel *projPanel = glui->add_panel("Projection");
  GLUI_RadioGroup *projGroup = glui->add_radiogroup_to_panel(projPanel, &projType, -1, projection_cb);
  glui->add_radiobutton_to_group(projGroup, "Orthographic");
  glui->add_radiobutton_to_group(projGroup, "Perspective");
  GLUI_Spinner *fovSpinner =glui->add_spinner_to_panel(projPanel, "FOV", GLUI_SPINNER_INT, &fov, FOV, projection_cb);
  fovSpinner->set_int_limits(0, 90, GLUI_LIMIT_CLAMP);

  // Object Transformations Panel
  GLUI_Panel *transformsPanel = glui->add_panel("Object Transformations");
  GLUI_Rotation *rotationManip = glui->add_rotation_to_panel(transformsPanel, "Rotation", rotMat, ROTATION,rotation_cb);
  rotationManip->reset();
  glui->add_column_to_panel(transformsPanel, true);
  GLUI_Translation *translateXYManip = glui->add_translation_to_panel(transformsPanel, "Translate XY", GLUI_TRANSLATION_XY, xyTrans, TRANSLATIONXY, translate_xy_cb);
  glui->add_column_to_panel(transformsPanel, true);
  GLUI_Translation *translateZManip = glui->add_translation_to_panel(transformsPanel, "Translate Z", GLUI_TRANSLATION_Z, &zTrans, TRANSLATIONZ, translate_z_cb);
  glui->add_column_to_panel(transformsPanel, true);
  GLUI_Spinner *scaleSpinner = glui->add_spinner_to_panel(transformsPanel, "Scale", GLUI_SPINNER_FLOAT, &objScale, SCALE, scale_cb);
  scaleSpinner->set_float_limits(0, 2.0, GLUI_LIMIT_CLAMP);

  // Camera Manipulation Panel
  GLUI_Panel *cameraPanel = glui->add_panel("Camera Manipulation Mode");
  GLUI_Rotation *camRotationManip = glui->add_rotation_to_panel(cameraPanel, "Camera Rotation", camRotMat, CAMROTATE,cam_rotation_cb);
  camRotationManip->reset();
  glui->add_column_to_panel(cameraPanel, true);
  GLUI_Translation *trackXYManip = glui->add_translation_to_panel(cameraPanel, "Track XY", GLUI_TRANSLATION_XY, camTrack, TRACK, track_xy_cb);
  glui->add_column_to_panel(cameraPanel, true);
  GLUI_Translation *dollyManip = glui->add_translation_to_panel(cameraPanel, "Dolly", GLUI_TRANSLATION_Z, &camDolly, DOLLY, dolly_cb);

  // Object Color Panel
  GLUI_Panel *colorPanel = glui->add_panel("Color");
  GLUI_Spinner *redValue = glui->add_spinner_to_panel(colorPanel, "Red", 2, &red, RED, color_cb);
  redValue->set_int_limits(0, MAX_COLOR);
  GLUI_Spinner *greenValue = glui->add_spinner_to_panel(colorPanel, "Green", 2, &green, GREEN, color_cb);
  greenValue->set_int_limits(0, MAX_COLOR);
  GLUI_Spinner *blueValue = glui->add_spinner_to_panel(colorPanel, "Blue", 2, &blue, BLUE, color_cb);
  blueValue->set_int_limits(0, MAX_COLOR);
  glui->set_main_gfx_window(main_window);

  GLUI_Master.set_glutIdleFunc(NULL);
  glui->sync_live();

  glutMainLoop();
  return EXIT_SUCCESS;
}
