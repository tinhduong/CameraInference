#include "GlShaderProgram.h"
#include <android/log.h>
#include <vector>

#define LOG_TAG "GlShaderProgram"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

GlShaderProgram::GlShaderProgram() {}

GlShaderProgram::~GlShaderProgram() {
    release();
}

bool GlShaderProgram::init(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader) return false;

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return false;
    }

    programId_ = glCreateProgram();
    if (!programId_) {
        LOGE("Could not create program");
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);
    glLinkProgram(programId_);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(programId_, GL_LINK_STATUS, &linkStatus);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (linkStatus != GL_TRUE) {
        GLint bufLength = 0;
        glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &bufLength);
        if (bufLength > 0) {
            std::vector<char> infoLog(bufLength);
            glGetProgramInfoLog(programId_, bufLength, nullptr, infoLog.data());
            LOGE("Could not link program:\n%s", infoLog.data());
        }
        release();
        return false;
    }

    return true;
}

void GlShaderProgram::release() {
    if (programId_ != 0) {
        glDeleteProgram(programId_);
        programId_ = 0;
    }
}

void GlShaderProgram::use() const {
    if (programId_ != 0) {
        glUseProgram(programId_);
    }
}

GLint GlShaderProgram::getUniformLocation(const char* name) const {
    return glGetUniformLocation(programId_, name);
}

GLint GlShaderProgram::getAttribLocation(const char* name) const {
    return glGetAttribLocation(programId_, name);
}

void GlShaderProgram::setUniform1i(const char* name, int value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

GLuint GlShaderProgram::compileShader(GLenum shaderType, const char* source) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        LOGE("Could not create shader type %d", shaderType);
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog.data());
            LOGE("Could not compile shader %d:\n%s", shaderType, infoLog.data());
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
