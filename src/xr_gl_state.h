#pragma once
#ifdef MCU_QUEST
#include <GLES3/gl32.h>
#include <utility>
namespace mcu {
struct GlState {
    GLint program, vao, arrayBuffer, elementBuffer, active, texture0, textureActive, draw, read,
        renderbuffer;
    GLint viewport[4], scissor[4], srcRGB, dstRGB, srcA, dstA, eqRGB, eqA, depthFunc, cullMode,
        frontFace, pack, unpack;
    GLfloat clear[4];
    GLboolean blend, depth, cull, scissorTest, depthMask, colorMask[4];
    bool pending = true;
    GlState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureActive);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcA);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &dstA);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqA);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
        glGetIntegerv(GL_FRONT_FACE, &frontFace);
        glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        scissorTest = glIsEnabled(GL_SCISSOR_TEST);
    }
    void restore() {
        if (!pending)
            return;
        pending = false;
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture0);
        glActiveTexture(active);
        glBindTexture(GL_TEXTURE_2D, textureActive);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA);
        glBlendEquationSeparate(eqRGB, eqA);
        glDepthFunc(depthFunc);
        glDepthMask(depthMask);
        glCullFace(cullMode);
        glFrontFace(frontFace);
        glPixelStorei(GL_PACK_ALIGNMENT, pack);
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        for (auto e : {std::pair<GLenum, GLboolean>{GL_BLEND, blend},
                       {GL_DEPTH_TEST, depth},
                       {GL_CULL_FACE, cull},
                       {GL_SCISSOR_TEST, scissorTest}})
            if (e.second)
                glEnable(e.first);
            else
                glDisable(e.first);
    }
    ~GlState() { restore(); }
};
} // namespace mcu
#endif
