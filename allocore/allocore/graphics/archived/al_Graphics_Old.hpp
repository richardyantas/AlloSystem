#ifndef INCLUDE_AL_GRAPHICS_HPP
#define INCLUDE_AL_GRAPHICS_HPP

/*
	Copyright (C) 2006-2008. The Regents of the University of California (REGENTS).
	All Rights Reserved.

	Permission to use, copy, modify, distribute, and distribute modified versions
	of this software and its documentation without fee and without a signed
	licensing agreement, is hereby granted, provided that the above copyright
	notice, the list of contributors, this paragraph and the following two paragraphs
	appear in all copies, modifications, and distributions.

	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
	OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
	BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
	PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
	HEREUNDER IS PROVIDED "AS IS". REGENTS HAS  NO OBLIGATION TO PROVIDE
	MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

/*
	Example code:

	Graphics gl();

	gl.begin(gl.POINTS);
	gl.vertex3d(0, 0, 0);
	gl.end();
*/

#include "allocore/math/al_Vec.hpp"
#include "allocore/math/al_Matrix4.hpp"
#include "allocore/types/al_Buffer.hpp"
#include "allocore/types/al_Color.hpp"

#include <stack>
using std::stack;

namespace al{

class Graphics;
class GraphicsBackend;
class GraphicsData;


/// Stores buffers related to rendering graphical objects
class GraphicsData {
public:

	typedef Vec3f			Vertex;
	typedef Vec3f			Normal;
	typedef Vec4f			Color;
	typedef Vec2f			TexCoord2;
	typedef Vec3f			TexCoord3;
	typedef unsigned int	Index;


	GraphicsData(): mPrimitive(0){}

	/// Reset all buffers
	void resetBuffers();
	void equalizeBuffers();
	void getBounds(Vec3f& min, Vec3f& max);
	Vec3f getCenter(); // center at 0,0,0

	// destructive edits to internal vertices:
	void unitize();	/// scale to -1..1
	void scale(double x, double y, double z);
	void scale(Vec3f v) { scale(v[0], v[1], v[2]); }
	void scale(double s) { scale(s, s, s); }
	void translate(double x, double y, double z);
	void translate(Vec3f v) { translate(v[0], v[1], v[2]); }

	// generates smoothed normals for a set of vertices
	// will replace any normals currently in use
	// angle - maximum angle (in degrees) to smooth across
	void generateNormals(float angle);

	int primitive() const { return mPrimitive; }
	const Buffer<Vertex>& vertices() const { return mVertices; }
	const Buffer<Normal>& normals() const { return mNormals; }
	const Buffer<Color>& colors() const { return mColors; }
	const Buffer<TexCoord2>& texCoord2s() const { return mTexCoord2s; }
	const Buffer<TexCoord3>& texCoord3s() const { return mTexCoord3s; }
	const Buffer<Index>& indices() const { return mIndices; }

	void addIndex(unsigned int i){ indices().append(i); }

	void addColor(float r, float g, float b, float a=1){ addColor(Color(r,g,b,a)); }
	void addColor(const Color& v) { colors().append(v); }
	void addColor(const al::Color& v) { addColor(v.r, v.g, v.b, v.a); }

	void addNormal(float x, float y, float z=0){ addNormal(Normal(x,y,z)); }
	void addNormal(const Normal& v) { normals().append(v); }

	void addTexCoord(float u, float v){ addTexCoord(TexCoord2(u,v)); }
	void addTexCoord(const TexCoord2& v){ texCoord2s().append(v); }
	
	void addTexCoord(float u, float v, float w){ addTexCoord(TexCoord3(u,v,w)); }
	void addTexCoord(const TexCoord3& v){ texCoord3s().append(v); }

	void addVertex(float x, float y, float z=0){ addVertex(Vertex(x,y,z)); }
	void addVertex(const Vertex& v){ vertices().append(v); }

	void primitive(int prim){ mPrimitive=prim; }

	Buffer<Vertex>& vertices(){ return mVertices; }
	Buffer<Normal>& normals(){ return mNormals; }
	Buffer<Color>& colors(){ return mColors; }
	Buffer<TexCoord2>& texCoord2s(){ return mTexCoord2s; }
	Buffer<TexCoord3>& texCoord3s(){ return mTexCoord3s; }
	Buffer<Index>& indices(){ return mIndices; }

protected:

	// Only populated (size>0) buffers will be used
	Buffer<Vertex> mVertices;
	Buffer<Normal> mNormals;
	Buffer<Color> mColors;
	Buffer<TexCoord2> mTexCoord2s;
	Buffer<TexCoord3> mTexCoord3s;
	Buffer<Index> mIndices;

	int mPrimitive;
};



/// Interface for setting graphics state and rendering GraphicsData

///	It also owns a GraphicsData, to simulate immediate mode (where it draws its own data)
///
class Graphics {
public:

	enum AntiAliasMode {
		DONT_CARE,
		FASTEST,
		NICEST
	};

	enum AttributeBit {
		COLOR_BUFFER_BIT	= 1<<0,		/**< Color-buffer bit */
		DEPTH_BUFFER_BIT	= 1<<1,		/**< Depth-buffer bit */
		ENABLE_BIT			= 1<<2,		/**< Enable bit */
		VIEWPORT_BIT		= 1<<3		/**< Viewport bit */
	};

	enum BlendFunc {
		SRC_ALPHA = 0,
		ONE_MINUS_SRC_ALPHA,
		SRC_COLOR,
		ONE_MINUS_SRC_COLOR,
		DST_ALPHA,
		ONE_MINUS_DST_ALPHA,
		DST_COLOR,
		ONE_MINUS_DST_COLOR,
		ZERO,
		ONE,
		SRC_ALPHA_SATURATE
	};

	enum Capability {
		BLEND,
		DEPTH_TEST,
		LIGHTING,
		SCISSOR_TEST
	};

	enum Face {
		FRONT = 0,
		BACK,
		FRONT_AND_BACK
	};

	enum MatrixMode {
		MODELVIEW = 0,
		PROJECTION
	};

	enum PolygonMode {
		POINT = 0,
		LINE,
		FILL
	};

	enum Primitive {
		POINTS = 0,
		LINES,
		LINE_STRIP,
		LINE_LOOP,
		TRIANGLES,
		TRIANGLE_STRIP,
		TRIANGLE_FAN,
		QUADS,
		QUAD_STRIP,
		POLYGON
	};

	struct State {
		State()
		:	blend_enable(false),
			blend_src(SRC_COLOR),
			blend_dst(DST_COLOR),
			lighting_enable(false),
			depth_enable(true),
			polygon_mode(FILL),
			antialias_mode(DONT_CARE)
		{}

		~State() {}

		// Blending
		bool blend_enable;
		BlendFunc blend_src;
		BlendFunc blend_dst;

		bool lighting_enable;			// Lighting
		bool depth_enable;				// Depth Testing
		PolygonMode polygon_mode;		// Polygon Mode
		AntiAliasMode antialias_mode;	// Anti-Aliasing
	};


	struct StateChange {
		StateChange()
		:	blending(true),
			lighting(true),
			depth_testing(true),
			polygon_mode(true),
			antialiasing(true)
		{}

		~StateChange(){}

		bool blending;
		bool lighting;
		bool depth_testing;
		bool polygon_mode;
		bool antialiasing;
	};

	/// @param[in] backend	The rendering backend
	Graphics(GraphicsBackend *backend);

	~Graphics();

	// Rendering State
	void blending(bool enable, BlendFunc src, BlendFunc dst);
	void depthTesting(bool enable);
	void depthMask(bool enable);
	void lighting(bool enable);
	void scissor(bool enable);
	void pushState(State &state);
	void popState();

	// Frame
	void clear(int attribMask);
	void clearColor(float r, float g, float b, float a);
	void clearColor(const Color& color) { clearColor(color.r, color.g, color.b, color.a); }

	// Coordinate Transforms
	void viewport(int x, int y, int width, int height);
	void matrixMode(MatrixMode mode);
	MatrixMode matrixMode();
	void pushMatrix();
	void pushMatrix(MatrixMode v){ matrixMode(v); pushMatrix(); }
	void popMatrix();
	void popMatrix(MatrixMode v){ matrixMode(v); popMatrix(); }
	void loadIdentity();
	void loadMatrix(const Matrix4d &m);
	void multMatrix(const Matrix4d &m);
	void translate(double x, double y, double z);
	void translate(const Vec3d& v) { translate(v[0], v[1], v[2]); }
	void translate(const Vec3f& v) { translate(v[0], v[1], v[2]); }
	void rotate(double angle, double x, double y, double z);
	void rotate(double angle, const Vec3d& v) { rotate(angle, v[0], v[1], v[2]); }
	void scale(double x, double y, double z);
	void scale(double s) { scale(s, s, s); }
	void scale(const Vec3d& v) { scale(v[0], v[1], v[2]); }
	void scale(const Vec3f& v) { scale(v[0], v[1], v[2]); }

	// Immediate Mode

	/// Begin "immediate" mode drawing
	void begin(Primitive mode);
	
	/// End "immediate" mode
	void end();

	void vertex(double x, double y, double z=0.);
	void vertex(const Vec2d& v) { vertex(v[0], v[1], 0); }
	void vertex(const Vec2f& v) { vertex(v[0], v[1], 0); }
	void vertex(const Vec3d& v) { vertex(v[0], v[1], v[2]); }
	void vertex(const Vec3f& v) { vertex(v[0], v[1], v[2]); }
	void texcoord(double u, double v);
	void normal(double x, double y, double z=0.);
	void normal(const Vec3d& v) { normal(v[0], v[1], v[2]); }
	void normal(const Vec3f& v) { normal(v[0], v[1], v[2]); }
	void color(double r, double g, double b, double a=1.);
	void color(const Color& v){ color(v.r, v.g, v.b, v.a); }
	void color(const Vec3d& v, double a=1.) { color(v[0], v[1], v[2], a); }
	void color(const Vec3f& v, double a=1.) { color(v[0], v[1], v[2], a); }

	// Other state
	void antialiasing(AntiAliasMode v);
	void lineWidth(double v);
	void pointSize(double v);

	// Buffer drawing

	/// Render data in graphic buffers
	void draw(const GraphicsData& v);

	/// Render data in internal graphic buffers
	void draw();

	/// Get implementation backend
	GraphicsBackend * backend(){ return mBackend; }
	
	/// Get internal graphic buffers
	GraphicsData& data(){ return mGraphicsData; }

	void drawBegin();
	void drawEnd();

protected:
	GraphicsBackend	*	mBackend;			// graphics API implementation
	GraphicsData		mGraphicsData;		// used for immediate mode style rendering
	bool				mInImmediateMode;	// flag for whether or not in immediate mode
	MatrixMode			mMatrixMode;		// matrix stack to use
	stack<Matrix4d>		mProjectionMatrix;	// projection matrix stack
	stack<Matrix4d>		mModelViewMatrix;	// modelview matrix stack
	StateChange			mStateChange;		// state difference to mark changes
	stack<State>		mState;				// state stack

	void compareState(const State &prev_state, const State &state);
	void enableState();
	stack<Matrix4d>& matrixStackForMode(MatrixMode mode);
};



///	Abstract base class for any object that can be rendered via Graphics
class Drawable {
public:
	virtual void onDraw(Graphics& gl) = 0;
	virtual ~Drawable(){}
};

} // ::al


// old stuff from al_Common.hpp that can be migrated at some point
//
//#include "allocore/graphics/al_Config.h"
//
//namespace al {
//namespace gfx{
//
//namespace AccessMode{
//	enum t{
//		ReadOnly		= GL_READ_ONLY,
//		WriteOnly		= GL_WRITE_ONLY, 
//		ReadWrite		= GL_READ_WRITE
//	};
//}
//
//namespace ArrayType{
//	enum t{
//		VertexArray			= GL_VERTEX_ARRAY,
//		NormalArray			= GL_NORMAL_ARRAY,
//		ColorArray			= GL_COLOR_ARRAY,
//		IndexArray			= GL_INDEX_ARRAY,
//		TextureCoordArray	= GL_TEXTURE_COORD_ARRAY,
//		EdgeFlagArray		= GL_EDGE_FLAG_ARRAY
//	};
//}
//
//// attribute masks
//namespace Attribute{
//	enum t{
//		ColorBufferBit	= GL_COLOR_BUFFER_BIT,
//		DepthBufferBit	= GL_DEPTH_BUFFER_BIT,
//		EnableBit		= GL_ENABLE_BIT,
//		ViewPortBit		= GL_VIEWPORT_BIT
//	};
//	inline t operator| (const t& a, const t& b){ return t(int(a) | int(b)); }
//	inline t operator& (const t& a, const t& b){ return t(int(a) & int(b)); }
//}
//
//namespace BufferType{
//	enum t{
//		ArrayBuffer		= GL_ARRAY_BUFFER,
//		ElementArray	= GL_ELEMENT_ARRAY_BUFFER,
//		PixelPack		= GL_PIXEL_PACK_BUFFER,			/**< Transfer to PBO */
//		PixelUnpack		= GL_PIXEL_UNPACK_BUFFER		/**< Transfer from PBO */
//	};
//}
//
//
///*
//"Static" means the data in VBO will not be changed, "Dynamic" means the data
//will be changed frequently, and "Stream" means the data will be changed every
//frame. "Draw" means the data will be sent to the GPU from the application,
//"Read" means the data will be read by the application from the GPU, and "Copy"
//means the data will be copied internally on the GPU.
//Read and Copy are used for PBOs and FBOs.
//*/
//namespace BufferUsage{
//	enum t{
//		StreamDraw		= GL_STREAM_DRAW,
//		StreamRead		= GL_STREAM_READ,
//		StreamCopy		= GL_STREAM_COPY,
//		StaticDraw		= GL_STATIC_DRAW,
//		StaticRead		= GL_STATIC_READ,
//		StaticCopy		= GL_STATIC_COPY,
//		DynamicDraw		= GL_DYNAMIC_DRAW,
//		DynamicRead		= GL_DYNAMIC_READ,
//		DynamicCopy		= GL_DYNAMIC_COPY
//	};
//}
//
////namespace Cap{
////	enum t{
////		AlphaTest		= GL_ALPHA_TEST,
////		Blend			= GL_BLEND,
////		ColorMaterial	= GL_COLOR_MATERIAL,
////		DepthTest		= GL_DEPTH_TEST,
////		Dither			= GL_DITHER,
////		Fog				= GL_FOG,
////		Lighting		= GL_LIGHTING,
////		LineSmooth		= GL_LINE_SMOOTH,
////		LineStipple		= GL_LINE_STIPPLE,
////		PointSprite		= GL_POINT_SPRITE,
////		PointSmooth		= GL_POINT_SMOOTH,
////		PolygonOffsetFill	=GL_POLYGON_OFFSET_FILL,
////		PolygonOffsetLine	=GL_POLYGON_OFFSET_LINE,
////		PolygonOffsetPoint	=GL_POLYGON_OFFSET_POINT,
////		PolygonSmooth	= GL_POLYGON_SMOOTH,
////		PolygonStipple	= GL_POLYGON_STIPPLE,
////		ScissorTest		= GL_SCISSOR_TEST,
////		StencilTest		= GL_STENCIL_TEST,
////		Texture1D		= GL_TEXTURE_1D,
////		Texture2D		= GL_TEXTURE_2D,
////		Texture3D		= GL_TEXTURE_3D,
////		TextureCubMap	= GL_TEXTURE_CUBE_MAP	
////	};
////}
//
//namespace ColorFormat{
//	enum t{
//		DepthComponent	= GL_DEPTH_COMPONENT,
//		Luminance		= GL_LUMINANCE,
//		LuminanceAlpha	= GL_LUMINANCE_ALPHA,
//		Red				= GL_RED,
//		Green			= GL_GREEN,
//		Blue			= GL_BLUE,
//		Alpha			= GL_ALPHA,
//		RGB				= GL_RGB,
//		RGBA			= GL_RGBA,
//		GBRA			= GL_BGRA
//	};
//}
//
////namespace Comparison{
////	enum t{
////		Never			= GL_NEVER,
////		Less			= GL_LESS,
////		Equal			= GL_EQUAL,
////		LEqual			= GL_LEQUAL,
////		Greater			= GL_GREATER,
////		NotEqual		= GL_NOTEQUAL,
////		GEqual			= GL_GEQUAL,
////		Always			= GL_ALWAYS
////	};
////}
//
//namespace DataType{
//	enum t{
//		Byte			= GL_BYTE,
//		UByte			= GL_UNSIGNED_BYTE,
//		Short			= GL_SHORT,
//		UShort			= GL_UNSIGNED_SHORT,
//		Int				= GL_INT,
//		UInt			= GL_UNSIGNED_INT,
//		Float			= GL_FLOAT,
//		Bytes2			= GL_2_BYTES,                  
//		Bytes3			= GL_3_BYTES,
//		Bytes4			= GL_4_BYTES,
//		Double			= GL_DOUBLE,
//		Unknown
//	};
//}
//
////namespace DrawBufferMode{
////	enum t{
////		None			= GL_NONE,
////		FrontLeft		= GL_FRONT_LEFT,
////		FrontRight		= GL_FRONT_RIGHT,
////		BackLeft		= GL_BACK_LEFT,
////		BackRight		= GL_BACK_RIGHT,
////		Front			= GL_FRONT,
////		Back			= GL_BACK,
////		Left			= GL_LEFT,
////		Right			= GL_RIGHT,
////		FrontAndBack	= GL_FRONT_AND_BACK
////	};
////}
//
////namespace Face{
////	enum t{
////		Front			= GL_FRONT,
////		Back			= GL_BACK,
////		FrontAndBack	= GL_FRONT_AND_BACK
////	};
////}
//
//namespace IpolMode{
//	enum t{
//		Nearest			= GL_NEAREST,
//		Linear			= GL_LINEAR
//	};
//}
//
////namespace MatrixMode{
////	enum t{
////		ModelView		= GL_MODELVIEW,
////		Projection		= GL_PROJECTION,
////		Texture			= GL_TEXTURE
////	};
////}
//
////namespace Prim{
////	enum t{
////		Points			= GL_POINTS,
////		Lines			= GL_LINES,
////		LineStrip		= GL_LINE_STRIP,
////		LineLoop		= GL_LINE_LOOP,
////		Triangles		= GL_TRIANGLES,
////		TriangleStrip	= GL_TRIANGLE_STRIP,
////		TriangleFan		= GL_TRIANGLE_FAN,
//////		Quads			= GL_QUADS,
//////		QuadStrip		= GL_QUAD_STRIP,
//////		Polygon			= GL_POLYGON
////	};
////}
//
//namespace ShaderType{
//	enum t{
//		Vertex			= GL_VERTEX_SHADER,
//		Fragment		= GL_FRAGMENT_SHADER
//	};
//}
//
//namespace WrapMode{
//	enum t{
//		Clamp			= GL_CLAMP,
//		ClampToBorder	= GL_CLAMP_TO_BORDER,
//		ClampToEdge		= GL_CLAMP_TO_EDGE,
//		MirroredRepeat	= GL_MIRRORED_REPEAT,
//		Repeat			= GL_REPEAT
//	};
//}
//
//// Use namespaces so that enum names can be accessed via glw::name. In case of
//// conflicts, the full namespace can be used.
//using namespace AccessMode;
////using namespace ArrayType;
////using namespace Attribute;
//using namespace BufferType;
//using namespace BufferUsage;
////using namespace Cap;
//using namespace ColorFormat;
//using namespace DataType;
////using namespace DrawBufferMode;
//
//using namespace IpolMode;
////using namespace MatrixMode;
////using namespace Prim;
//using namespace WrapMode;
//
//
//inline int numComponents(ColorFormat::t f){
//	switch(f){
//		case ColorFormat::RGB:				return 3;
//		case ColorFormat::RGBA:				return 4;
//		case ColorFormat::Luminance:	
//		case ColorFormat::Red:
//		case ColorFormat::Green:
//		case ColorFormat::Blue:
//		case ColorFormat::Alpha:			return 1;
//		case ColorFormat::LuminanceAlpha:	return 2;
//		default:							return 0;
//	};
//}
//
//inline int numBytes(DataType::t type){
//	#define CS(a,b) case a: return sizeof(b); 
//	switch(type){
//		CS(Byte, char)
//		CS(UByte, unsigned char)
//		CS(Short, short)
//		CS(UShort, unsigned short)
//		CS(Int, int)
//		CS(UInt, unsigned int)
//		CS(Float, float)
//		CS(Double, double)
//		default: return 0;
//	};
//	#undef CS
//}
//
//template<class T> inline DataType::t asType(){ return gfx::DataType::Unknown; }
//template<> inline DataType::t asType<char>(){ return gfx::Byte; }
//template<> inline DataType::t asType<unsigned char>(){ return gfx::UByte; }
//template<> inline DataType::t asType<short>(){ return gfx::Short; }
//template<> inline DataType::t asType<unsigned short>(){ return gfx::UShort; }
//template<> inline DataType::t asType<int>(){ return gfx::Int; }
//template<> inline DataType::t asType<unsigned int>(){ return gfx::UInt; }
//template<> inline DataType::t asType<float>(){ return gfx::Float; }
//template<> inline DataType::t asType<double>(){ return gfx::Double; }



//
///// Disables rendering capabilities
//struct Disable{
//
//	Disable(){}
//	Disable(Cap::t v){ *this << v; }
//	Disable(Cap::t v1, Cap::t v2){ *this << v1 << v2; }
//
//	/// Disable a capability
//	const Disable& operator()(Cap::t v) const { return *this << v; }
//	const Disable& operator()(Cap::t v1, Cap::t v2) const { return *this << v1<<v2; }
//	const Disable& operator()(Cap::t v1, Cap::t v2, Cap::t v3) const { return *this << v1<<v2<<v3; }
//	
//	/// Disable a capability
//	const Disable& operator<< (Cap::t v) const { glDisable(v); return *this; }
//};
//
///// Global Disable function object
//static Disable disable;
//
//
//
///// Enables rendering capabilities
//struct Enable{
//
//	Enable(){}
//	Enable(Cap::t v){ *this << v; }
//	Enable(Cap::t v1, Cap::t v2){ *this << v1 << v2; }
//
//	/// Enable a capability
//	const Enable& operator()(Cap::t v) const { return *this << v; }
//	const Enable& operator()(Cap::t v1, Cap::t v2) const { return *this << v1<<v2; }
//	const Enable& operator()(Cap::t v1, Cap::t v2, Cap::t v3) const { return *this << v1<<v2<<v3; }
//	
//	/// Enable a capability
//	const Enable& operator<< (Cap::t v) const { glEnable(v); return *this; }
//};
//
///// Global Enable function object
//static Enable enable;


//void alphaFunc(Comparison::t f, float v);			///< Set alpha test function
//void alphaFunc(const char * code, float v);			///< Set alpha test function: <, <=, =, >=, >, !=, !, *
//void begin(Prim::t v);								///< Begin vertex group delimitation
//void blendFunc(int sfactor, int dfactor);			///< Set blending function
//void blendTrans();									///< Set blending function to transparent
//void blendAdd();									///< Set blending function to additive
//void clear(Attribute::t mask);							///< Clear drawing buffers
//void clearColor(float r, float g, float b, float a=1);	///< Set clear color
//void clearColor(const glw::Color& v);				///< Set clear color
//void color(float gray, float a=1);					///< Set current draw color
//void color(float r, float g, float b, float a=1);	///< Set current draw color
//void color(const glw::Color& v);					///< Set current draw color
//void color(const glw::Color& c, float a);			///< Set current draw color, but override alpha component
//void colorMask(bool r, bool g, bool b, bool a);
//void drawBuffer(DrawBufferMode::t v);				///< Set drawing buffer
//void end();											///< End vertex group delimitation
//void fog(float end, float start, const glw::Color& c=glw::Color());	///< Set linear fog parameters
//void frustum(double l, double r, double b, double t, double near, double far); ///< Multiply the current matrix by a perspective matrix
//void identity();									///< Load identity transform matrix
//void lineStipple(char factor, short pattern);		///< Specify line stipple pattern
//void lineWidth(float val);							///< Set width of lines
//
///// Define a viewing transformation
//void lookAt(
//	double eyeX, double eyeY, double eyeZ,
//	double ctrX, double ctrY, double ctrZ,
//	double  upX, double  upY, double  upZ);
//
//void matrixMode(MatrixMode::t v);					///< Set current transform matrix
//
//template <class V3> void normal(const V3& v);		///< Send normal vertex given 3-element array accessible object
//void normal(float x, float y, float z);				///< Send normal vertex
//
//void ortho(float l, float r, float b, float t);		///< Set orthographic projection mode
//void perspective(float fovy, float aspect, float znear, float zfar);
//void pointSize(float val);							///< Set size of points
//void pointAtten(float c2=0, float c1=0, float c0=1); ///< Set distance attenuation of points. The scaling formula is clamp(size * sqrt(1/(c0 + c1*d + c2*d^2)))
//void pop();											///< Pop current transform matrix stack
//void pop(MatrixMode::t v);							///< Pop a transform matrix stack also setting as current matrix
//void pop2D();										///< Pop 2-D pixel space
//void pop3D();										///< Pop 3-D signed normalized cartesian space
//void popAttrib();									///< Pop last pushed attributes from stack
//void push();										///< Push current transform matrix stack 
//void push(MatrixMode::t m);							///< Push a transform matrix stack also setting as current matrix
//void pushAttrib(Attribute::t mask);					///< Push current attributes onto stack
//void rotate(float degx, float degy, float degz);
//void rotateX(float deg);
//void rotateY(float deg);
//void rotateZ(float deg);
//void scale(float v);								///< Scale all dimensions by amount
//void scale(float x, float y, float z=1.f);
//void scissor(float x, float y, float w, float h);
//void stroke(float w);								///< Sets width of lines and points
//void texCoord(float x, float y);
//void translate(float x, float y, float z=0.f);
//void translateX(float x);
//void translateY(float y);
//void translateZ(float z);
//void viewport(float x, float y, float w, float h);
//template <class V3> void vertex(const V3& v);		///< Send single vertex given 3-element array accessible object
//void vertex(float x, float y);						///< Send single xy vertex
//void vertex(float x, float y, float z);				///< Send single xyz vertex
//
//
//
//// Implementation
//
//// <, <=, =, >=, >, !=, !, *
//inline void alphaFunc(const char * f, float v){
//	using namespace glw::Comparison;
//	switch(f[0]){
//		case '<': alphaFunc(f[1] ? LEqual:Less, v); break;
//		case '>': alphaFunc(f[1] ? GEqual:Greater, v); break;
//		case '=': alphaFunc(Equal, v); break;
//		case '!': alphaFunc(f[1] ? NotEqual:Never, v); break;
//		case '*': alphaFunc(Always, v); break;
//	}
//}
//inline void clearColor(const glw::Color& v){ clearColor(v.r,v.g,v.b,v.a); }
//inline void color(float v, float a){ color(v,v,v,a); }
//inline void color(const glw::Color& v){ color(v.r, v.g, v.b, v.a); }
//inline void color(const glw::Color& v, float a){ color(v.r, v.g, v.b, a); }
//template <class V3> inline void normal(const V3& v){ normal(v[0],v[1],v[2]); }
//inline void pop(MatrixMode::t v){ matrixMode(v); pop(); }
//inline void push(MatrixMode::t v){ matrixMode(v); push(); }
//inline void rotate(float degx, float degy, float degz){	rotateX(degx); rotateY(degy); rotateZ(degz); }
//inline void scale(float v){ scale(v,v,v); }
//inline void stroke(float v){ pointSize(v); lineWidth(v); }
//inline void translateX(float v){ translate(v,0,0); }
//inline void translateY(float v){ translate(0,v,0); }
//inline void translateZ(float v){ translate(0,0,v); }
//template <class V3> inline void vertex(const V3& v){ vertex(v[0], v[1], v[2]); }
//
//
//// platform dependent
//
//inline void alphaFunc(Comparison::t f, float v){ glAlphaFunc(f,v); }
//inline void begin(Prim::t v){ glBegin(v); }
//inline void blendFunc(int sfactor, int dfactor){ glBlendFunc(sfactor, dfactor); }
//inline void blendAdd(){ glBlendEquation(GL_FUNC_ADD); blendFunc(GL_SRC_COLOR, GL_ONE); }
//inline void blendTrans(){ glBlendEquation(GL_FUNC_ADD); blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
//inline void clear(Attribute::t a){ glClear(a); }
//inline void clearColor(float r, float g, float b, float a){ glClearColor(r,g,b,a); }
//inline void color(float r, float g, float b, float a){ glColor4f(r,g,b,a); }
//inline void colorMask(bool r, bool g, bool b, bool a){ glColorMask(r, g, b, a); }
//inline void drawBuffer(DrawBufferMode::t v){ glDrawBuffer(v); }
//inline void end(){ glEnd(); }
//
//inline void fog(float end, float start, const glw::Color& c){
//	glFogi(GL_FOG_MODE, GL_LINEAR); 
//	glFogf(GL_FOG_START, start); glFogf(GL_FOG_END, end);
//	float fogColor[4] = {c.r, c.g, c.b, c.a};
//	glFogfv(GL_FOG_COLOR, fogColor);
//}
//
//inline void frustum(double l, double r, double b, double t, double near, double far){ glFrustum(l,r,b,t,near,far); }
//inline void identity(){ glLoadIdentity(); }
//inline void lineStipple(char factor, short pattern){ glLineStipple(factor, pattern); }
//inline void lineWidth(float v){ glLineWidth(v); }
//inline void lookAt(double ex, double ey, double ez, double cx, double cy, double cz, double ux, double uy, double uz){
//	gluLookAt(ex,ey,ez, cx,cy,cz, ux,uy,uz);
//}
//inline void matrixMode(MatrixMode::t v){ glMatrixMode(v); }
//inline void normal(float x, float y, float z){ glNormal3f(x,y,z); }
//inline void ortho(float l, float r, float b, float t){ gluOrtho2D(l,r,b,t); }
//inline void perspective(float fovy, float aspect, float znear, float zfar){
//	gluPerspective(fovy, aspect, znear, zfar);
//}
//inline void pointSize(float v){ glPointSize(v); }
//inline void pointAtten(float c2, float c1, float c0){
//	GLfloat att[3] = {c0, c1, c2};
//	glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, att);
//}
//
//inline void pop() { glPopMatrix(); }
//inline void popAttrib(){ glPopAttrib(); }
//inline void push(){ glPushMatrix(); }
//inline void pushAttrib(Attribute::t a){ glPushAttrib(a); }
//inline void rect(float l, float t, float r, float b){ glRectf(r, b, l, t); }
//inline void rotateX(float deg){ glRotatef(deg, 1.f, 0.f, 0.f); }
//inline void rotateY(float deg){ glRotatef(deg, 0.f, 1.f, 0.f); }
//inline void rotateZ(float deg){ glRotatef(deg, 0.f, 0.f, 1.f); }
//inline void scale(float x, float y, float z){ glScalef(x,y,z); }
//inline void scissor(float x, float y, float w, float h){ glScissor((GLint)x,(GLint)y,(GLsizei)w,(GLsizei)h); }
//inline void texCoord(float x, float y){ glTexCoord2f(x,y); }
//inline void translate(float x, float y, float z){ glTranslatef(x,y,z); }
//inline void viewport(float x, float y, float w, float h){ glViewport((GLint)x,(GLint)y,(GLsizei)w,(GLsizei)h); }
//inline void vertex(float x, float y){ glVertex2f(x,y); }
//inline void vertex(float x, float y, float z){ glVertex3f(x,y,z); }



/*

// capabilities (for disable() and enable())
enum Capability{
	Blend			= GL_BLEND,
	DepthTest		= GL_DEPTH_TEST,
	Fog				= GL_FOG,
	LineSmooth		= GL_LINE_SMOOTH,
	LineStipple		= GL_LINE_STIPPLE,
	PolygonSmooth	= GL_POLYGON_SMOOTH,
	PointSmooth		= GL_POINT_SMOOTH,
	ScissorTest		= GL_SCISSOR_TEST,
	Texture2D		= GL_TEXTURE_2D
};






/// Function object for disabling rendering capabilities
struct Disable{
	/// Disable a capability
	const Disable& operator() (Capability c) const { glDisable(c); return *this; }
	
	/// Disable a capability
	const Disable& operator<< (Capability c) const { return (*this)(c); }
};

/// Global Disable functor
static Disable disable;



/// Function object for enabling rendering capabilities
struct Enable{
	/// Enable a capability
	const Enable& operator() (Capability c) const { glEnable(c); return *this; }
	
	/// Enable a capability
	const Enable& operator<< (Capability c) const { return (*this)(c); }
};

/// Global Enable functor
static Enable enable;

*/

//} // ::al::gfx
//} // ::al


#endif
