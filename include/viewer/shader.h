#ifndef VIEWER_SHADER_H
#define VIEWER_SHADER_H

#include <string>
#include <map>

#include "viewer/include_opengl.h"

// geometry processing and shape analysis framework
namespace gproshan {


class shader
{
	protected:
		GLuint program {0};
		std::map<std::string, GLint> uniform;
		bool linked;

	public:
		shader() = default;
		~shader();
		const GLint & operator () (const std::string & name);
		operator GLuint () const;
		void load_vertex(const char * filename);
		void load_fragment(const char * filename);
		void load_geometry(const char * filename);
		void enable();
		void disable() const;

	protected:
		bool load(GLenum shader_type, const char * filename);
		bool read_source(const char * filename, std::string & source);
};


} // namespace gproshan

#endif // VIEWER_SHADER_H

