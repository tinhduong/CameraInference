#pragma once
#include <GLES3/gl3.h>

class GlShaderProgram {
public:
    GlShaderProgram();
    ~GlShaderProgram();

    bool init(const char* vertexSource, const char* fragmentSource);
    void release();
    void use() const;

    GLint getUniformLocation(const char* name) const;
    GLint getAttribLocation(const char* name) const;
    
    void setUniform1i(const char* name, int value) const;
    
    GLuint getProgramId() const { return programId_; }

private:
    GLuint compileShader(GLenum shaderType, const char* source);

    GLuint programId_ = 0;
};
