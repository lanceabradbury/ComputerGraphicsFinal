#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#define Cos(th) cos(3.1415926/180*(th))
#define Sin(th) sin(3.1415926/180*(th))
#define Atan(th) (180/3.1415926)*atan(th)

int th = 0;
int ph = 0;
int axes = 0;
int fov = 55;
int light = 1;
int move = 1;
double asp = 1;
double dim = 3.0;

//light values
int distance  =   14;
int inc       =  10;
int smooth    =   1;
int local     =   0;
int emission  =   0;
int ambient   =  30;
int diffuse   = 100;
int specular  =   0;
int shininess =   0;
float shinyvec[1];
int zh        =  90;
float fastforward  =   0;
int mode = 1;


//character variables
float xpos = -0.2;
float zpos = -0.2;
float xlookat = 1;
float zlookat = 0;
int direction = 0;
int zone = 0;
float shieldx = .75;
int defending = 0;
double timePrev = 0;
int attacking = 1;
float swordx = 0;
float swordy = 0;
int upswing = 1;
int swung = 0;
float deathx = 0;
float deathy = 1;
int dieing = 0;

//enemy variables
float projectile1 = 0;
float crz = 1;
float crrot = 0;
int movingRight = 1;
int cralive = 1;

unsigned int texture[7];
unsigned int sky[8];

#define LEN 8192  //  Maximum length of text string
void Print(const char* format , ...)
{
   char    buf[LEN];
   char*   ch=buf;
   va_list args;
   //  Turn the parameters into a character string
   va_start(args,format);
   vsnprintf(buf,LEN,format,args);
   va_end(args);
   //  Display the characters one at a time at the current raster position
   while (*ch)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,*ch++);
}
void ErrCheck(char* where)
{
   int err = glGetError();
   if (err) fprintf(stderr,"ERROR: %s [%s]\n",gluErrorString(err),where);
}
void Fatal(const char* format , ...)
{
   va_list args;
   va_start(args,format);
   vfprintf(stderr,format,args);
   va_end(args);
   exit(1);
}
/*
 *  Reverse n bytes
 */
static void Reverse(void* x,const int n)
{
   int k;
   char* ch = (char*)x;
   for (k=0;k<n/2;k++)
   {
      char tmp = ch[k];
      ch[k] = ch[n-1-k];
      ch[n-1-k] = tmp;
   }
}

/*
 *  Load texture from BMP file
 */
unsigned int LoadTexBMP(char* file)
{
   unsigned int   texture;    // Texture name
   FILE*          f;          // File pointer
   unsigned short magic;      // Image magic
   unsigned int   dx,dy,size; // Image dimensions
   unsigned short nbp,bpp;    // Planes and bits per pixel
   unsigned char* image;      // Image data
   unsigned int   k;          // Counter

   //  Open file
   f = fopen(file,"rb");
   if (!f) Fatal("Cannot open file %s\n",file);
   //  Check image magic
   if (fread(&magic,2,1,f)!=1) Fatal("Cannot read magic from %s\n",file);
   if (magic!=0x4D42 && magic!=0x424D) Fatal("Image magic not BMP in %s\n",file);
   //  Seek to and read header
   if (fseek(f,16,SEEK_CUR) || fread(&dx ,4,1,f)!=1 || fread(&dy ,4,1,f)!=1 ||
       fread(&nbp,2,1,f)!=1 || fread(&bpp,2,1,f)!=1 || fread(&k,4,1,f)!=1)
     Fatal("Cannot read header from %s\n",file);
   //  Reverse bytes on big endian hardware (detected by backwards magic)
   if (magic==0x424D)
   {
      Reverse(&dx,4);
      Reverse(&dy,4);
      Reverse(&nbp,2);
      Reverse(&bpp,2);
      Reverse(&k,4);
   }
   //  Check image parameters
   if (dx<1 || dx>65536) Fatal("%s image width out of range: %d\n",file,dx);
   if (dy<1 || dy>65536) Fatal("%s image height out of range: %d\n",file,dy);
   if (nbp!=1)  Fatal("%s bit planes is not 1: %d\n",file,nbp);
   if (bpp!=24) Fatal("%s bits per pixel is not 24: %d\n",file,bpp);
   if (k!=0)    Fatal("%s compressed files not supported\n",file);
#ifndef GL_VERSION_2_0
   //  OpenGL 2.0 lifts the restriction that texture size must be a power of two
   for (k=1;k<dx;k*=2);
   if (k!=dx) Fatal("%s image width not a power of two: %d\n",file,dx);
   for (k=1;k<dy;k*=2);
   if (k!=dy) Fatal("%s image height not a power of two: %d\n",file,dy);
#endif

   //  Allocate image memory
   size = 3*dx*dy;
   image = (unsigned char*) malloc(size);
   if (!image) Fatal("Cannot allocate %d bytes of memory for image %s\n",size,file);
   //  Seek to and read image
   if (fseek(f,20,SEEK_CUR) || fread(image,size,1,f)!=1) Fatal("Error reading data from image %s\n",file);
   fclose(f);
   //  Reverse colors (BGR -> RGB)
   for (k=0;k<size;k+=3)
   {
      unsigned char temp = image[k];
      image[k]   = image[k+2];
      image[k+2] = temp;
   }

   //  Sanity check
   ErrCheck("LoadTexBMP");
   //  Generate 2D texture
   glGenTextures(1,&texture);
   glBindTexture(GL_TEXTURE_2D,texture);
   //  Copy image
   glTexImage2D(GL_TEXTURE_2D,0,3,dx,dy,0,GL_RGB,GL_UNSIGNED_BYTE,image);
   if (glGetError()) Fatal("Error in glTexImage2D %s %dx%d\n",file,dx,dy);
   //  Scale linearly when image size doesn't match
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

   //  Free image memory
   free(image);
   //  Return texture name
   return texture;
}

void Project(double fov,double asp,double dim)
{
   //  Tell OpenGL we want to manipulate the projection matrix
   glMatrixMode(GL_PROJECTION);
   //  Undo previous transformations
   glLoadIdentity();
   //  Perspective transformation
   if (fov)
      gluPerspective(fov,asp,dim/16,16*dim);
   //  Orthogonal transformation
   else
      glOrtho(-asp*dim,asp*dim,-dim,+dim,-dim,+dim);
   //  Switch to manipulating the model matrix
   glMatrixMode(GL_MODELVIEW);
   //  Undo previous transformations
   glLoadIdentity();
}

void reshape(int width,int height)
{
   //  Ratio of the width to the height of the window
   asp = (height>0) ? (double)width/height : 1;
   //  Set the viewport to the entire window
   glViewport(0,0, width,height);
   //  Set projection
   Project(mode?fov:0,asp,dim);
}

void idle()
{
   //  Elapsed time in seconds
	double t = glutGet(GLUT_ELAPSED_TIME);
	double d;
	
	//death animation
	if(dieing)
	{
		double fall = fmod(t, .08);
		if(deathy >= 0 && deathx <= 1)
		{
			deathy -= fall;
			deathx += fall;
		}
	}
	
	//this is the part that handles the projectile from the turret
	if(zone == 1)
	{
		if(xpos - .3 < projectile1 && projectile1 < xpos + .3)
		{
			if(zpos > -.3 && zpos < .3)
			{
				if(!defending)
				{
					dieing = 1;
				}
			}
		}
		double shoot = fmod(t, .1);
		if(projectile1 < -15)
			projectile1 = 0;
		else
			projectile1 -= shoot;
	}
   
   //creeper movement
	if(zone == 2)
	{
		if(cralive)
		{
			double creepinc = fmod(t, .05);
			if(movingRight == 1)
			{
				crz += creepinc;
			}
			else if(movingRight == 0)
				crz -= creepinc;
			else if(movingRight == 2)
			{
				double creeprot = fmod(t, 5);
				crrot += creeprot;
			}
			else if(movingRight == 3)
			{
				double creeprot = fmod(t, 5);
				crrot -= creeprot;
			}
		
			if(crz >= 5)
			{
				movingRight = 2;
			}
			if(crz < -5)
			{
				movingRight = 3;
			}
			
			if(crrot >= 180 && movingRight == 2)
			{
				crrot = 180;
				movingRight = 0;
			}
			if(crrot <= 0 && movingRight == 3)
			{
				crrot = 0;
				movingRight = 1;
			}
			//creeper kills you
			if(!defending)
			{
				if(movingRight == 1 && xpos + .3 > 3 && xpos - .3 < 3)
				{
					if(crz > zpos -.3 && crz < zpos + -.1)
						dieing = 1;
				}
				if(movingRight == 0 && xpos + .3 > 3 && xpos - .3 < 3)
				{
					if(crz > zpos +.1 && crz < zpos + .3)
						dieing = 1;
				}
			}
		}
	}
   
	//swing sword animation
	if(!attacking)
	{
		double diff = fmod(t/1000.0, .08);
		if(upswing && swordy > 0)
		{
			if(swordx < .15)
				swordx += diff;
		}
		else
		{
			if(swordx > -.15)
			swordx -= diff;
		}
		if(upswing)
		{
			if(swordy > .2)
				upswing = 1 - upswing;
			swordy += diff;
			if(!swung && swordy > 0)
			{
				swung = 1 - swung;
				attacking = 1 - attacking;
			}
		}
		else
		{
			swordy -= diff;
			if(swordy < -.25)
			{
				upswing = 1 - upswing;
				swung = 1 - swung;
			}
		} 
		float dist = pow(3 - xpos, 2) + pow(crz-zpos, 2);
		if(sqrt(dist) < .5)
		{
			cralive = 0;
		}
	}
	//put your shield up to protect yourself
	else if(defending)
	{
		if(shieldx > 0)
		{
			double diff = fmod(t, 0.1);
			shieldx -= diff;
		}
	}
	else
	{
		if(shieldx < .75)
		{
			double diff = fmod(t, 0.1);
			shieldx += diff;
		}
	}
	if(fastforward)
	{
		d = 1000.0;
	}
	else
	{
		d = 10000.0;
		
	}
	t = t / d;
	zh = fmod(90*t, 360.0);
	//  Tell GLUT it is necessary to redisplay the scene
	glutPostRedisplay();
}

void special(int key,int x,int y)
{
   //  Right arrow key - increase angle by 5 degrees
   if (key == GLUT_KEY_RIGHT)
      th += 5;
   //  Left arrow key - decrease angle by 5 degrees
   else if (key == GLUT_KEY_LEFT)
      th -= 5;
   //  Up arrow key - increase elevation by 5 degrees
   else if (key == GLUT_KEY_UP)
      ph += 5;
   //  Down arrow key - decrease elevation by 5 degrees
   else if (key == GLUT_KEY_DOWN)
      ph -= 5;
   //  Flip sign
   //  Keep angles to +/-360 degrees
   th %= 360;
   ph %= 360;
   //  Update projection
   Project(mode?fov:0,asp,dim);
   //  Tell GLUT it is necessary to redisplay the scene
   glutPostRedisplay();
}

/*
 *  GLUT calls this routine when a key is pressed
 */
void key(unsigned char ch,int x,int y)
{
   //  Exit on ESC
   if (ch == 27)
      exit(0);
	if(!dieing)//this is so you cant move while dead
	{
		//  Reset to the center of the zone
		if (ch == '0')
		{
			zpos = 0;
			xpos = 0;
		}
		//  Toggle axes
		else if (ch == 'x' || ch == 'X')
			axes = 1-axes;
		//increase the movement rate of the sun
		else if(ch == 't' || ch == 'T')
			fastforward = 1 - fastforward;
		//swing sword
		else if(ch == 'o' || ch == 'O')
		{
			if(!defending)
			{
				attacking = 1 - attacking;
			}
		}
		//move forward
		else if(ch == 'w' || ch == 'W')
		{
			if(xpos < 14.7 && xpos > -14.7)
				xpos += Cos(direction) * 0.2;
			if(zpos < 14.7 && zpos > -14.7)
				zpos += Sin(direction) * 0.2;
		}
		//move backward
		else if(ch == 's' || ch == 'S')
		{
			if(xpos < 14.7 && xpos > -14.7)
				xpos -= Cos(direction) * 0.2;
			if(zpos < 14.7 && zpos > -14.7)
				zpos -= Sin(direction) * 0.2;
		}
		//strafe left
		else if(ch == 'q' || ch == 'Q')
		{
			if(xpos < 14.7 && xpos > -14.7)
				xpos -= Cos(direction + 90) * 0.2;
			if(zpos < 14.7 && zpos > -14.7)
				zpos -= Sin(direction + 90) * 0.2;
		}
		//strafe right
		else if(ch == 'e' || ch == 'E')
		{
			if(xpos < 14.7 && xpos > -14.7)
				xpos += Cos(direction + 90) * 0.2;
			if(zpos < 14.7 && zpos > -14.7)
				zpos += Sin(direction + 90) * 0.2;
		}
		//turn left
		else if(ch == 'a' || ch == 'A')
		{
			direction -= 3;
		}
		//turn right
		else if(ch == 'd' || ch == 'D')
		{
			direction += 3;
		}
		//bring up shield
		else if( ch == 'p' || ch == 'P')
		{
			if(attacking)
				defending = 1 - defending;
		}
		else if(ch == 'u' || ch == 'U')
			dieing = 1;
    } 
	direction = direction % 360;
	glutIdleFunc(move?idle:NULL);
	//go through the door
	if(xpos > 14.6 && zpos > -1 && zpos < 1)
	{
		if(zone == 2)
			zone = 0;
		else
			zone = zone + 1;
		xpos = -14.6;
		projectile1 = 0;
		glutPostRedisplay();
	}
	//so you cant get stuck outside of the skybox
	else if(xpos > 14.6)
	{
		xpos = 14.4;
	}
	if(zpos > 14.6)
	{
		zpos = 14.4;
	}
	glutPostRedisplay();
}

//sits in one location and shoots a projectile bullet
void turret(double x,double y,double z, double dx,double dy,double dz, double th)
{
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
	float white[] = {1,1,1,1};
	float black[] = {0,0,0,1};
	glMaterialfv(GL_FRONT_AND_BACK,GL_SHININESS,shinyvec);
	glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,white);
	glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,black);
	
	glPushMatrix();
	glTranslated(x,y,z);
	glRotated(th,0,1,0);
	glScaled(dx,dy,dz);

	glBindTexture(GL_TEXTURE_2D, texture[6]);
	glBegin(GL_QUADS);
	//  Front
	glColor3f(1,1,1);
	glNormal3f( 0, 0, 1);
	glTexCoord2f(0,0); glVertex3f(-1.0,-1.5, 1.0);
	glTexCoord2f(1,0); glVertex3f(+1.0,-1.5, 1.0);
	glTexCoord2f(1,1); glVertex3f(+1.0,+1.5, 1.0);
	glTexCoord2f(0,1); glVertex3f(-1.0,+1.5, 1.0);
	glEnd();
	//  Back
	glBindTexture(GL_TEXTURE_2D, texture[6]);
	glBegin(GL_QUADS);
	glNormal3f( 0, 0,-1);
	glTexCoord2f(0,0); glVertex3f(+1.0,-1.5,-1.0);
	glTexCoord2f(1,0); glVertex3f(-1.0,-1.5,-1.0);
	glTexCoord2f(1,1); glVertex3f(-1.0,+1.5,-1.0);
	glTexCoord2f(0,1); glVertex3f(+1.0,+1.5,-1.0);
	glEnd();
	//  Right
	glBindTexture(GL_TEXTURE_2D, texture[6]);
	glBegin(GL_QUADS);
	glNormal3f(+1, 0, 0);
	glTexCoord2f(0,0); glVertex3f(+1.0,-1.5,+1.0);
	glTexCoord2f(1,0); glVertex3f(+1.0,-1.5,-1.0);
	glTexCoord2f(1,1); glVertex3f(+1.0,+1.5,-1.0);
	glTexCoord2f(0,1); glVertex3f(+1.0,+1.5,+1.0);
	glEnd();
	//  Left
	glBindTexture(GL_TEXTURE_2D, texture[6]);
	glBegin(GL_QUADS);
	glNormal3f(-1, 0, 0);
	glTexCoord2f(0,0); glVertex3f(-1.0,-1.5,-1.0);
	glTexCoord2f(1,0); glVertex3f(-1.0,-1.5,+1.0);
	glTexCoord2f(1,1); glVertex3f(-1.0,+1.5,+1.0);
	glTexCoord2f(0,1); glVertex3f(-1.0,+1.5,-1.0);
	glEnd();
	//  Top
	glBindTexture(GL_TEXTURE_2D, texture[6]);
	glBegin(GL_QUADS);
	glNormal3f( 0,+1, 0);
	glTexCoord2f(0,0); glVertex3f(-1.0,+1.5,+1.0);
	glTexCoord2f(1,0); glVertex3f(+1.0,+1.5,+1.0);
	glTexCoord2f(1,1); glVertex3f(+1.0,+1.5,-1.0);
	glTexCoord2f(0,1); glVertex3f(-1.0,+1.5,-1.0);

	glEnd();
	glDisable(GL_TEXTURE_2D);
	//barrel
	int d = 5;
	int theta;
	glColor3f(1,1,1);
	glBegin(GL_QUAD_STRIP);
	for (theta=0;theta<=360;theta+=d) {
		glNormal3d(Sin(theta), Cos(theta), 1);
		glVertex3d(-1, (Cos(theta) * .3333) + 1, Sin(theta) * .333);
		glVertex3d(-1.5, (Cos(theta) * .3333) + 1, Sin(theta) * .333);
	}
	glEnd();
	
	//cap on the barrel
	glBegin(GL_TRIANGLE_FAN);
	int ph;
	glColor3f(0, 0, 0);
	for(ph = 0; ph <= 360; ph += 5)
	{
		glVertex3d(-1.1, (Cos(ph) * .333) + 1, Sin(ph) * .333);
	}
	glEnd();
	
	glPopMatrix();
}

void weapons()
{
	//Save transform attributes (Matrix Mode and Enabled Modes)
	glPushAttrib(GL_TRANSFORM_BIT|GL_ENABLE_BIT);
	//  Save projection matrix and set unit transform
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-asp,+asp,-1,1,-1,1);
	//  Save model view matrix and set to indentity
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	//  Draw instrument panel with texture
	glColor3f(1,1,1);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
	
	//shield
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(0.5,0.5);
	int ph;
	for(ph = 0; ph <= 360; ph += 5)
	{
		glTexCoord2f(0.5*Cos(ph)+0.5,0.5*Sin(ph)+0.5);
		glVertex2d((Cos(ph) * .6666) - shieldx, (Sin(ph) * .6666) - shieldx);
	}
	glEnd();
	
	//sword
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glRotated(5,0,0,1);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex2f(.7 + swordx, .9 + swordy);
	glTexCoord2f(1,0); glVertex2f(.8 + swordx, .9 + swordy);
	glTexCoord2f(1,1); glVertex2f(.8 + swordx, -0.5 + swordy);
	glTexCoord2f(0,1); glVertex2f(.7 + swordx, -0.5 + swordy);
	glEnd();
	//  Reset model view matrix
	glPopMatrix();
	//  Reset projection matrix
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	//  Pop transform attributes (Matrix Mode and Enabled Modes)
	glPopAttrib();
	glDisable(GL_TEXTURE_2D);

}

void skybox(double D)
{
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
	glColor3f(1,1,1);

	if(zone == 0)
		glBindTexture(GL_TEXTURE_2D, sky[3]);
	else if(zone == 1)
		glBindTexture(GL_TEXTURE_2D, sky[5]);
	else
		glBindTexture(GL_TEXTURE_2D, sky[7]);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(-D,-1,-D);
	glTexCoord2f(1,0); glVertex3f(+D,-1,-D);
	glTexCoord2f(1,1); glVertex3f(+D,+D,-D);
	glTexCoord2f(0,1); glVertex3f(-D,+D,-D);
	glEnd();
	
	if(zone == 0)
		glBindTexture(GL_TEXTURE_2D, sky[4]);
	else if(zone == 1)
		glBindTexture(GL_TEXTURE_2D, sky[5]);
	else
		glBindTexture(GL_TEXTURE_2D, sky[6]);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(+D,-1,-D);
	glTexCoord2f(1,0); glVertex3f(+D,-1,+D);
	glTexCoord2f(1,1); glVertex3f(+D,+D,+D);
	glTexCoord2f(0,1); glVertex3f(+D,+D,-D);
	glEnd();
	
	if(zone == 0)
		glBindTexture(GL_TEXTURE_2D, sky[1]);
	else if(zone == 1)
		glBindTexture(GL_TEXTURE_2D, sky[5]);
	else
		glBindTexture(GL_TEXTURE_2D, sky[6]);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(+D,-1,+D);
	glTexCoord2f(1,0); glVertex3f(-D,-1,+D);
	glTexCoord2f(1,1); glVertex3f(-D,+D,+D);
	glTexCoord2f(0,1); glVertex3f(+D,+D,+D);
	glEnd();
	
	if(zone == 0)
		glBindTexture(GL_TEXTURE_2D, sky[2]);
	else if(zone == 1)
		glBindTexture(GL_TEXTURE_2D, sky[5]);
	else
		glBindTexture(GL_TEXTURE_2D, sky[7]);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(-D,-1,+D);
	glTexCoord2f(1,0); glVertex3f(-D,-1,-D);
	glTexCoord2f(1,1); glVertex3f(-D,+D,-D);
	glTexCoord2f(0,1); glVertex3f(-D,+D,+D);
	glEnd();
	//  Top and bottom
	glBegin(GL_QUADS);
	glVertex3f(+D,+D,-D);
	glVertex3f(+D,+D,+D);
	glVertex3f(-D,+D,+D);
	glVertex3f(-D,+D,-D);
	glEnd();

	if(zone ==0)
		glBindTexture(GL_TEXTURE_2D, sky[0]);
	else if(zone == 1)
		glBindTexture(GL_TEXTURE_2D, texture[5]);
	else
		glBindTexture(GL_TEXTURE_2D, texture[4]);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0); glVertex3f(-D,-1,+D);
	glTexCoord2f(1,0); glVertex3f(+D,-1,+D);
	glTexCoord2f(1,1); glVertex3f(+D,-1,-D);
	glTexCoord2f(0,1); glVertex3f(-D,-1,-D);
	glEnd();
	glPopMatrix();
	glDisable(GL_TEXTURE_2D);
}

static void Vertex(double th,double ph)
{
   double x = Sin(th)*Cos(ph);
   double y = Cos(th)*Cos(ph);
   double z =         Sin(ph);
   //  For a sphere at the origin, the position
   //  and normal vectors are the same
   glNormal3d(x,y,z);
   glVertex3d(x,y,z);
}
//this is based off the creeper from minecraft
void creeper(double x,double y,double z,
                 double scale,
                 double th, double phi)
{
	float yellow[] = {1.0,1.0,0.0,1.0};
	float Emission[]  = {0.0,0.0,0.01*emission,1.0};

	glPushMatrix();
	glTranslated(x,y,z);
	glRotated(th,0,1,0);
	glRotated(phi,0,0,1);
	glScaled(scale,scale,scale);

	glMaterialfv(GL_FRONT,GL_SHININESS,shinyvec);
	glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
	glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
	
	glColor3f(1, 1, 1);
	glBegin(GL_QUADS);
	//front head
	glColor3f(0,0,0);
	glNormal3f( 0, 0, 1);
	glVertex3f(-1,4, 1);
	glVertex3f(+1,4, 1);
	glVertex3f(+1,+6, 1);
	glVertex3f(-1,+6, 1);
	//  Back head
	glColor3f(1,1,1);
	glNormal3f( 0, 0,-1);
	glVertex3f(+1,4,-1);
	glVertex3f(-1,4,-1);
	glVertex3f(-1,+6,-1);
	glVertex3f(+1,+6,-1);
	//  Right head
	glNormal3f(+1, 0, 0);
	glVertex3f(+1,4,+1);
	glVertex3f(+1,4,-1);
	glVertex3f(+1,+6,-1);
	glVertex3f(+1,+6,+1);
	//  Left head
	glNormal3f(-1, 0, 0);
	glVertex3f(-1,4,-1);
	glVertex3f(-1,4,+1);
	glVertex3f(-1,+6,+1);
	glVertex3f(-1,+6,-1);
	//  Top head
	glNormal3f( 0,+1, 0);
	glVertex3f(-1,+6,+1);
	glVertex3f(+1,+6,+1);
	glVertex3f(+1,+6,-1);
	glVertex3f(-1,+6,-1);
	//  Bottom head
	glNormal3f( 0,-1, 0);
	glVertex3f(-1,4,-1);
	glVertex3f(+1,4,-1);
	glVertex3f(+1,4,+1);
	glVertex3f(-1,4,+1);
	glEnd();
	
	glBegin(GL_QUADS);
	//front body
	glNormal3f( 0, 0, 1);
	glVertex3f(-1,0, .5);
	glVertex3f(+1,0, .5);
	glVertex3f(+1,+4, .5);
	glVertex3f(-1,+4, .5);
	//  Back body
	glNormal3f( 0, 0,-1);
	glVertex3f(+1,0,-.5);
	glVertex3f(-1,0,-.5);
	glVertex3f(-1,+4,-.5);
	glVertex3f(+1,+4,-.5);
	//  Right body
	glNormal3f(+1, 0, 0);
	glVertex3f(+1,0,+.5);
	glVertex3f(+1,0,-.5);
	glVertex3f(+1,+4,-.5);
	glVertex3f(+1,+4,+.5);
	//  Left body
	glNormal3f(-1, 0, 0);
	glVertex3f(-1,0,-.5);
	glVertex3f(-1,0,+.5);
	glVertex3f(-1,+4,+.5);
	glVertex3f(-1,+4,-.5);
	//  Top body
	glNormal3f( 0,+1, 0);
	glVertex3f(-1,+4,+.5);
	glVertex3f(+1,+4,+.5);
	glVertex3f(+1,+4,-.5);
	glVertex3f(-1,+4,-.5);
	//  Bottom body
	glNormal3f( 0,-1, 0);
	glVertex3f(-1,0,-.5);
	glVertex3f(+1,0,-.5);
	glVertex3f(+1,0,+.5);
	glVertex3f(-1,0,+.5);
	glEnd();
	
	glBegin(GL_QUADS);
	//front front leg
	glNormal3f( 0, 0, 1);
	glVertex3f(-1,-2, 1.5);
	glVertex3f(+1,-2, 1.5);
	glVertex3f(+1,0, 1.5);
	glVertex3f(-1,0, 1.5);
	//  Back front leg
	glNormal3f( 0, 0,-1);
	glVertex3f(+1,-2,.5);
	glVertex3f(-1,-2,.5);
	glVertex3f(-1,0,.5);
	glVertex3f(+1,0,.5);
	//  Right front leg
	glNormal3f(+1, 0, 0);
	glVertex3f(+1,-2,1.5);
	glVertex3f(+1,-2,.5);
	glVertex3f(+1,0,.5);
	glVertex3f(+1,0,1.5);
	//  Left front leg
	glNormal3f(-1, 0, 0);
	glVertex3f(-1,-2,.5);
	glVertex3f(-1,-2,1.5);
	glVertex3f(-1,0,1.5);
	glVertex3f(-1,0,.5);
	//  Top front leg
	glNormal3f( 0,+1, 0);
	glVertex3f(-1,0,1.5);
	glVertex3f(+1,0,1.5);
	glVertex3f(+1,0,.5);
	glVertex3f(-1,0,.5);
	//  Bottom front leg
	glNormal3f( 0,-1, 0);
	glVertex3f(-1,-2,.5);
	glVertex3f(+1,-2,.5);
	glVertex3f(+1,-2,1.5);
	glVertex3f(-1,-2,1.5);
	glEnd();
	
	glBegin(GL_QUADS);
	//front back leg
	glNormal3f( 0, 0, 1);
	glVertex3f(-1,-2, -.5);
	glVertex3f(+1,-2, -.5);
	glVertex3f(+1,0, -.5);
	glVertex3f(-1,0, -.5);
	//  Back back leg
	glNormal3f( 0, 0,-1);
	glVertex3f(+1,-2,-1.5);
	glVertex3f(-1,-2,-1.5);
	glVertex3f(-1,0,-1.5);
	glVertex3f(+1,0,-1.5);
	//  Right back leg
	glNormal3f(+1, 0, 0);
	glVertex3f(+1,-2,-.5);
	glVertex3f(+1,-2,-1.5);
	glVertex3f(+1,0,-1.5);
	glVertex3f(+1,0,-.5);
	//  Left back leg
	glNormal3f(-1, 0, 0);
	glVertex3f(-1,-2,-1.5);
	glVertex3f(-1,-2,-.5);
	glVertex3f(-1,0,-.5);
	glVertex3f(-1,0,-1.5);
	//  Top back leg
	glNormal3f( 0,+1, 0);
	glVertex3f(-1,0,-.5);
	glVertex3f(+1,0,-.5);
	glVertex3f(+1,0,-1.5);
	glVertex3f(-1,0,-1.5);
	//  Bottom back leg
	glNormal3f( 0,-1, 0);
	glVertex3f(-1,-2,-1.5);
	glVertex3f(+1,-2,-1.5);
	glVertex3f(+1,-2,-.5);
	glVertex3f(-1,-2,-.5);
	glEnd();
	glPopMatrix();
}

static void ball(double x,double y,double z,double r)
{
   int th,ph;
   float yellow[] = {1.0,1.0,0.0,1.0};
   float Emission[]  = {0.0,0.0,0.01*emission,1.0};
   //  Save transformation
   glPushMatrix();
   //  Offset, scale and rotate
   glTranslated(x,y,z);
   glScaled(r,r,r);
   //  White ball
   glColor3f(1,1,1);
   glMaterialfv(GL_FRONT,GL_SHININESS,shinyvec);
   glMaterialfv(GL_FRONT,GL_SPECULAR,yellow);
   glMaterialfv(GL_FRONT,GL_EMISSION,Emission);
   //  Bands of latitude
   for (ph=-90;ph<90;ph+=inc)
   {
      glBegin(GL_QUAD_STRIP);
      for (th=0;th<=360;th+=2*inc)
      {
         Vertex(th,ph);
         Vertex(th,ph+inc);
      }
      glEnd();
   }
   //  Undo transofrmations
   glPopMatrix();
}



void display()
{
	const double len=1.5;  //  Length of axes
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glLoadIdentity();

	double dx = Cos(direction) * 100;
	double dz = Sin(direction) * 100;
	gluLookAt(xpos,0.3,zpos, dx,0,dz, 0,deathy,deathx);
	glColor3f(1,1,1);
	//this represents different areas.
	//zone 0 is outside
	//zone 1 is a cave with a turret trap
	//zone 2 is a deeper cave with a minecraft creeper
	switch(zone)
	{
		case 0:
			glColor3f(1,1,1);
			float Ambient[]   = {0.01*ambient ,0.01*ambient ,0.01*ambient ,1.0};
			float Diffuse[]   = {0.01*diffuse ,0.01*diffuse ,0.01*diffuse ,1.0};
			float Specular[]  = {0.01*specular,0.01*specular,0.01*specular,1.0};
			//  Light position
			float Position[]  = {0,distance*Sin(zh),distance*Cos(zh),1.0};
			//  Draw light position as ball (still no lighting here)
			ball(Position[0],Position[1],Position[2] , 0.1);
			//  OpenGL should normalize normal vectors
			glEnable(GL_NORMALIZE);
			//  Enable lighting
			glEnable(GL_LIGHTING);
			//  Location of viewer for specular calculations
			glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,local);
			//  glColor sets ambient and diffuse color materials
			glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
			glEnable(GL_COLOR_MATERIAL);
			//  Enable light 0
			glEnable(GL_LIGHT0);
			//  Set ambient, diffuse, specular components and position of light 0
			glLightfv(GL_LIGHT0,GL_AMBIENT ,Ambient);
			glLightfv(GL_LIGHT0,GL_DIFFUSE ,Diffuse);
			glLightfv(GL_LIGHT0,GL_SPECULAR,Specular);
			glLightfv(GL_LIGHT0,GL_POSITION,Position);
			break;
		case 1:
			turret(0,0,0, .4, .4, .4, 0);
			ball(projectile1, .4, 0, .1666);
			break;
		case 2:
			if(cralive)
				creeper(3, -.1, crz, .1, crrot, 0);
			else
				creeper(3, -.5, crz, .1, crrot, 90);
			break;
	}
	
	//draw sword and shield
	weapons();
	
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2,2);
	skybox(5 * dim);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glColor3f(1,1,1);
	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);

	//door to another zone
	if(zone == 0)
		glBindTexture(GL_TEXTURE_2D, texture[2]);
	else
		glBindTexture(GL_TEXTURE_2D, texture[3]);
	glBegin(GL_QUADS);
	glTexCoord2f(1,0); glVertex3f(15,-1.1,-1);
	glTexCoord2f(0,0); glVertex3f(15,-1.1,1);
	glTexCoord2f(0,1); glVertex3f(15,1,1);
	glTexCoord2f(1,1); glVertex3f(15,1,-1);
	glEnd();
	
	glDisable(GL_TEXTURE_2D);
	
	if(axes)
	{
		glColor3f(1,1,1);
		glBegin(GL_LINES);
		glVertex3d(0,0,0);
		glVertex3d(len,0,0);
		glVertex3d(0,0,0);
		glVertex3d(0,len,0);
		glVertex3d(0,0,0);
		glVertex3d(0,0,len);
		glEnd();
      
		glRasterPos3d(len,0.0,0.0);
		Print("X");
		glRasterPos3d(0.0,len,0.0);
		Print("Y");
		glRasterPos3d(0.0,0.0,len);
		Print("Z");

		glWindowPos2i(5,5);
	}
	glFlush();
	glutSwapBuffers();

}

int main(int argc,char* argv[])
{
	glutInit(&argc,argv);
   	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
   	glutInitWindowSize(800,800);
   	glutCreateWindow("LanceBradburyFinal");
   	glutDisplayFunc(display);
   	glutReshapeFunc(reshape);
   	glutSpecialFunc(special);
   	glutKeyboardFunc(key);
   	glutIdleFunc(idle);
	sky[0] = LoadTexBMP("Grass.bmp");
	sky[1] = LoadTexBMP("Vista0.bmp");
	sky[2] = LoadTexBMP("Vista1.bmp");
	sky[3] = LoadTexBMP("Vista2.bmp");
	sky[4] = LoadTexBMP("Vista3.bmp");
	sky[5] = LoadTexBMP("cave1.bmp");
	sky[6] = LoadTexBMP("cave2.bmp");
	sky[7] = LoadTexBMP("cave3.bmp");

	texture[0] = LoadTexBMP("shield back1.bmp");
	texture[1] = LoadTexBMP("sword.bmp");
	texture[2] = LoadTexBMP("door1.bmp");
	texture[3] = LoadTexBMP("door2.bmp");
	texture[4] = LoadTexBMP("floor1.bmp");
	texture[5] = LoadTexBMP("floor2.bmp");
	texture[6] = LoadTexBMP("turret.bmp");

   	glutMainLoop();
   	return 0;
}
