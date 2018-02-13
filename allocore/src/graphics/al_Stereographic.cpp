#include "allocore/graphics/al_Stereographic.hpp"
#include "allocore/graphics/al_Graphics.hpp"	/* << need the OpenGL headers */

namespace al{

Stereographic::Stereographic()
:	mMode(ANAGLYPH), mAnaglyphMode(RED_CYAN), mClearColor(Color(0)),
	mSlices(24), mOmniFov(360),
	mEyeNumber(0),
	mStereo(false), mOmni(false)
{}

/// Convert a normalized screen space position to world space
 // each component of input vector should be normalized from -1. to 1.
// template<class T>
Vec3d Stereographic::unproject(Vec3d screenPos){
	Matrix4d invprojview = Matrix4d::inverse(this->modelViewProjection());
	Vec4d worldPos4 = invprojview.transform(screenPos);
	return worldPos4.sub<3>(0) / worldPos4.w;
}

void Stereographic::pushDrawPop(Graphics& g, Drawable& draw){
	g.pushMatrix(g.PROJECTION);
	g.loadMatrix(projection());
	g.pushMatrix(g.MODELVIEW);
	g.loadMatrix(modelView());
		draw.onDraw(g);
	g.popMatrix(g.PROJECTION);
	g.popMatrix(g.MODELVIEW);
}

void Stereographic::sendViewport(Graphics& g, const Viewport& vp){
	glScissor(vp.l, vp.b, vp.w, vp.h);
	g.viewport(vp.l, vp.b, vp.w, vp.h);
	mVP = vp;
}

void Stereographic::sendClear(Graphics& g){
	g.depthMask(true); // ensure writing to depth buffer is enabled
	g.clearColor(mClearColor);
	g.clear(g.COLOR_BUFFER_BIT | g.DEPTH_BUFFER_BIT);
}

void Stereographic :: draw(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect) {
	//printf("%d, %d\n", mStereo, mMode);
	if(mStereo){
		switch(mMode){
			case ANAGLYPH:	drawAnaglyph(g, lens, pose, vp, draw, clear, pixelaspect); return;
			case ACTIVE:	drawActive	(g, lens, pose, vp, draw, clear, pixelaspect); return;
			case DUAL:		drawDual	(g, lens, pose, vp, draw, clear, pixelaspect); return;
			case LEFT_EYE:	drawLeft	(g, lens, pose, vp, draw, clear, pixelaspect); return;
			case RIGHT_EYE:	drawRight	(g, lens, pose, vp, draw, clear, pixelaspect); return;
			default:;
		}
	} else {
		drawMono(g, lens, pose, vp, draw, clear, pixelaspect);
	}
}

void Stereographic :: drawMono(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect)
{
	const Vec3d& pos = pose.pos();

	// We must configure scissoring BEFORE clearing buffers
	g.scissorTest(true);
	sendViewport(g, vp);

	// g.drawBuffer(Graphics::BACK);	// << breaks usage under FBO
	if(clear) sendClear(g);

	mEye = pos;

	if (omni()) {
		int wx = vp.l;
		double fovx = mOmniFov;
		for (unsigned i=0; i<mSlices; i++) {
			// angle at center of slice:
			double angle = fovx * (0.5-((i+0.5)/(double)(mSlices)));

			int wx1 = vp.l + vp.w * (i+1)/(double)mSlices;
			Viewport vp1(wx, vp.b, wx1-wx, vp.h);
			double aspect = vp1.aspect() * pixelaspect;
			double fovy = Lens::getFovyForFovX(fovx * (vp1.w)/(double)vp.w, aspect);
			mProjection = Matrix4d::perspective(fovy, aspect, lens.near(), lens.far());

			// TODO: lerp quat instead of computing anew each iteration
			mModelView = (pose * Quatd().fromAxisAngle(M_DEG2RAD * angle, 0, 1, 0)).matrix();
			invertRigid(mModelView);

			sendViewport(g, vp1);
			if(clear) sendClear(g);

			pushDrawPop(g,draw);

			wx = wx1;
		}

	} else {
		double fovy = lens.fovy();
		double aspect = vp.aspect() * pixelaspect;
		mProjection = Matrix4d::perspective(fovy, aspect, lens.near(), lens.far());
		
		mModelView = pose.matrix();
		invertRigid(mModelView);

		pushDrawPop(g,draw);
	}

	g.scissorTest(false);
}


/* This is used by the various stereo draw methods.
Before calling this function, you must enable scissoring and set the
appropriate draw buffer. Thus, to draw the right eye:

	g.scissorTest(true);
	g.drawBuffer(Graphics::BACK_RIGHT);

	drawOffAxis(RIGHT_EYE, g, lens, pose, vp, draw, clear, pixelaspect);

	g.drawBuffer(Graphics::BACK);
	g.scissorTest(false);
*/
void Stereographic::drawEye(StereoMode eye, Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect){

	double near = lens.near();
	double far = lens.far();
	double focal = lens.focalLength();
	double iod = lens.eyeSep();
	const Vec3d& pos = pose.pos();

	if(RIGHT_EYE == eye){
		iod = -iod; // eyes only differ in sign in interocular distance
		mEyeNumber = 0;
	}
	else{
		mEyeNumber = 1;
	}

	sendViewport(g, vp);		// set scissor/viewport regions
	if(clear) sendClear(g);	// clear color/depth buffers

	// FIXME: geometry is not continuous at slice boundaries
	if (omni()) {
		// Position of left edge of slice
		int wx = vp.l;
		double fovx = mOmniFov;

		// Render slices starting on left of viewport
		for (unsigned i=0; i<mSlices; i++) {
			// angle at center of slice:
			double angle = fovx * (0.5-((i+0.5)/(double)(mSlices)));

			// Position of right edge of slice (exclusive)
			int wx1 = vp.l + vp.w * (i+1)/(double)mSlices;
			Viewport vp1(wx, vp.b, wx1-wx, vp.h);
			double aspect = vp1.aspect() * pixelaspect;
			double fovy = Lens::getFovyForFovX(fovx * (vp1.w)/(double)vp.w, aspect);

			Quatd q = pose.quat() * Quatd().fromAxisAngle(M_DEG2RAD * angle, 0, 1, 0);
			Vec3d ux = q.toVectorX();
			Vec3d uy = q.toVectorY();
			Vec3d uz = q.toVectorZ();

			/* LEFT
			mEye = pos + (ux * iod);
			mProjection = Matrix4d::perspectiveLeft(fovy, aspect, near, far, iod, focal);
			*/
			/* RIGHT
			mEye = pos + (ux * -iod);	// right
			mProjection = Matrix4d::perspectiveRight(fovy, aspect, near, far, iod, focal);
			*/

			mEye = pos + (ux * iod);

			// Compute projection and modelview matrices
			mProjection = Matrix4d::perspectiveOffAxis(fovy, aspect, near, far, 0.5*iod, focal);
			mModelView = Matrix4d::lookAt(ux, uy, uz, mEye);

			// Do the rendering
			sendViewport(g, vp1);		// set scissor/viewport regions
			pushDrawPop(g,draw);		// onDraw wrapped in push/pop calls

			wx = wx1;
		}

	} else {
		double fovy = lens.fovy();
		double aspect = vp.aspect() * pixelaspect;

		// Get orientation reference frame of viewer
		Vec3d ux, uy, uz;
		pose.unitVectors(ux, uy, uz);

		/* LEFT
		mEye = pos + (ux * iod);
		mProjection = Matrix4d::perspectiveLeft(fovy, aspect, near, far, iod, focal);
		mModelView = Matrix4d::lookAtLeft(ux, uy, uz, mEye, iod);
					(= lookAtOffAxis(ux,uy,uz, pos,-0.5*iod))
		*/

		/* RIGHT
				mEye = pos + (ux * -iod);	// right
		mProjection = Matrix4d::perspectiveRight(fovy, aspect, near, far, iod, focal);
		mModelView = Matrix4d::lookAtRight(ux, uy, uz, mEye, iod);
					(= lookAtOffAxis(ux,uy,uz, pos, 0.5*iod))
		*/

		mEye = pos + (ux * iod);

		// Compute projection and modelview matrices
		mProjection = Matrix4d::perspectiveOffAxis(fovy, aspect, near, far, 0.5*iod, focal);
		mModelView = Matrix4d::lookAtOffAxis(ux, uy, uz, mEye, -0.5*iod);

		// Do the rendering
		pushDrawPop(g,draw);		// onDraw wrapped in push/pop
	}
}


void Stereographic :: drawAnaglyph(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect)
{
	g.scissorTest(true);

	// We must clear here since color masks affect glClear
	if(clear){
		sendViewport(g, vp);
		sendClear(g);
	}

	switch(mAnaglyphMode){
		case RED_BLUE:
		case RED_GREEN:
		case RED_CYAN:	g.colorMask(1,0,0,1); break;
		case BLUE_RED:	g.colorMask(0,0,1,1); break;
		case GREEN_RED:	g.colorMask(0,1,0,1); break;
		case CYAN_RED:	g.colorMask(0,1,1,1); break;
		default:		g.colorMask(1);
	}

	drawEye(RIGHT_EYE, g, lens, pose, vp, draw, /*clear*/false, pixelaspect);

	// Clear only depth buffer for second eye pass
	// Note: This must be cleared regardless of the 'clear' argument since we
	// only have one depth buffer and eye must have its own depth buffer.
	g.viewport(vp.l, vp.b, vp.w, vp.h);
	g.depthMask(true);
	g.clear(g.DEPTH_BUFFER_BIT);

	switch(mAnaglyphMode){
		case RED_BLUE:	g.colorMask(0,0,1,1); break;
		case RED_GREEN:	g.colorMask(0,1,0,1); break;
		case RED_CYAN:	g.colorMask(0,1,1,1); break;
		case BLUE_RED:
		case GREEN_RED:
		case CYAN_RED:	g.colorMask(1,0,0,1); break;
		default:		g.colorMask(1);
	}

	drawEye(LEFT_EYE, g, lens, pose, vp, draw, /*clear*/false, pixelaspect);

	g.colorMask(1);
	g.scissorTest(false);
}



void Stereographic :: drawActive(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect)
{
	g.scissorTest(true);

	g.drawBuffer(Graphics::BACK_RIGHT);
	drawEye(RIGHT_EYE, g, lens, pose, vp, draw, clear, pixelaspect);

	g.drawBuffer(Graphics::BACK_LEFT);
	drawEye(LEFT_EYE, g, lens, pose, vp, draw, clear, pixelaspect);

	g.drawBuffer(Graphics::BACK); // set back (?) to default
	g.scissorTest(false);
}


void Stereographic :: drawLeft(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect)
{
	g.scissorTest(true);
	drawEye(LEFT_EYE, g, lens, pose, vp, draw, clear, pixelaspect);
	g.scissorTest(false);
}

void Stereographic :: drawRight(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect)
{
	g.scissorTest(true);
	drawEye(RIGHT_EYE, g, lens, pose, vp, draw, clear, pixelaspect);
	g.scissorTest(false);
}


void Stereographic :: drawDual(Graphics& g, const Lens& lens, const Pose& pose, const Viewport& vp, Drawable& draw, bool clear, double pixelaspect)
{
	g.scissorTest(true);

	// Clear the whole viewport in one go (not once for each half)
	if(clear){
		sendViewport(g, vp);
		sendClear(g);
	}

	Viewport vpright(vp.l + vp.w*0.5, vp.b, vp.w*0.5, vp.h);
	Viewport vpleft(vp.l, vp.b, vp.w*0.5, vp.h);

	drawEye(RIGHT_EYE, g, lens, pose, vpright, draw, /*clear*/false, pixelaspect);

	drawEye(LEFT_EYE, g, lens, pose, vpleft, draw, /*clear*/false, pixelaspect);

	g.scissorTest(false);
}



/// blue line sync for active stereo
/// @see http://local.wasp.uwa.edu.au/~pbourke/miscellaneous/stereographics/stereorender/GLUTStereo/glutStereo.cpp
void Stereographic :: drawBlueLine(double window_width, double window_height){
// FIXME: This will not compile with OpenGL ES
#if defined(AL_GRAPHICS_USE_OPENGL)
	GLint i;
	unsigned long buffer;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	for(i = 0; i < 6; i++) glDisable(GL_CLIP_PLANE0 + i);
	glDisable(GL_COLOR_LOGIC_OP);
	glDisable(GL_COLOR_MATERIAL);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glDisable(GL_FOG);
	glDisable(GL_LIGHTING);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_LINE_STIPPLE);
	glDisable(GL_SCISSOR_TEST);
	//glDisable(GL_SHARED_TEXTURE_PALETTE_EXT); /* not in 10.5 sdk */
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_CUBE_MAP);
	glDisable(GL_TEXTURE_RECTANGLE_EXT);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	for(buffer = GL_BACK_LEFT; buffer <= GL_BACK_RIGHT; buffer++) {
		GLint matrixMode;
		GLint vp[4];

		glDrawBuffer(buffer);

		glGetIntegerv(GL_VIEWPORT, vp);
		glViewport(0, 0, window_width, window_height);

		glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glScalef(2.0f / window_width, -2.0f / window_height, 1.0f);
		glTranslatef(-window_width / 2.0f, -window_height / 2.0f, 0.0f);

		// draw sync lines
		glColor3d(0.0f, 0.0f, 0.0f);
		glBegin(GL_LINES); // Draw a background line
			glVertex3f(0.0f, window_height - 0.5f, 0.0f);
			glVertex3f(window_width, window_height - 0.5f, 0.0f);
		glEnd();
		glColor3d(0.0f, 0.0f, 1.0f);
		glBegin(GL_LINES); // Draw a line of the correct length (the cross over is about 40% across the screen from the left
			glVertex3f(0.0f, window_height - 0.5f, 0.0f);
			if(buffer == GL_BACK_LEFT)
				glVertex3f(window_width * 0.30f, window_height - 0.5f, 0.0f);
			else
				glVertex3f(window_width * 0.80f, window_height - 0.5f, 0.0f);
		glEnd();

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(matrixMode);

		glViewport(vp[0], vp[1], vp[2], vp[3]);
	}
	glPopAttrib();

	glDrawBuffer(GL_BACK);
#endif
}

} // al::
