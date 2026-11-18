#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include "basic_transform.h"
#include "texture_atlas.h"
#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <cassert>

#define GL_SILENCE_DEPRECATION  // silences macOS OpenGL deprecation warnings

#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

using SpriteID = size_t;

class SpriteManager {
public:
    SpriteManager(const sf::Vector2u& atlasSize, unsigned padding)
        : mVertices(sf::PrimitiveType::Triangles),
          mNumSprites{0},
          mAtlas(atlasSize, padding),
          mShader(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"}),
          mTextureUniformLocation{-1},
          mTransformsUniformLocation{-1},
          mNumTransformsUniformLocation{-1},
          mPositionAttribLocation{-1},
          mTexCoordsAttribLocation{-1},
          mSpriteNumberAttribLocation{-1},
          mVerticesVBO{0},
          mSpriteNumberVBO{0},
          mTransformsTexture{0} {
        // TODO
        GLuint shader = mShader.getNativeHandle();
        mPositionAttribLocation = glGetAttribLocation(shader, "position");
        mTexCoordsAttribLocation = glGetAttribLocation(shader, "texCoords");
        mSpriteNumberAttribLocation = glGetAttribLocation(shader, "spriteNumber");

        mTextureUniformLocation = glGetUniformLocation(shader, "texture");
        mTransformsUniformLocation = glGetUniformLocation(shader, "transforms");
        mNumTransformsUniformLocation = glGetUniformLocation(shader, "numTransforms");

        glCheck(glGenBuffers(1, &mVerticesVBO));
        glCheck(glGenBuffers(1, &mSpriteNumberVBO));
        glCheck(glGenTextures(1, &mTransformsTexture));
    }

    ~SpriteManager() {
        glCheck(glDeleteBuffers(1, &mVerticesVBO));
        glCheck(glDeleteBuffers(1, &mSpriteNumberVBO));
        glCheck(glDeleteTextures(1, &mTransformsTexture));
    }

    // TODO
    TextureID registerTexture(const std::string& filepath) {
        return mAtlas.registerTexture(filepath);
    };

    // TODO
    SpriteID createSprite(TextureID textureID) {
        SpriteID id = mNumSprites;
        ++mNumSprites;
        mTransforms.emplace_back();

        for (size_t i = 0; i < 6; ++i) {
            mVertices.append(sf::Vertex{});
            mSpriteNumbers.push_back(static_cast<float>(id));
        }
        setTexture(id, textureID);
        return id;
    };

    // TODO
    void setTexture(SpriteID sprite, TextureID newTexture) {
        assert(sprite < mNumSprites);
        sf::Vector2f size{mAtlas.getSizePixels(newTexture)};
        sf::FloatRect uv = mAtlas.getBoundsUV(newTexture);

        const std::array<sf::Vector2f, 6> positions = {
            sf::Vector2f{0.f, 0.f}, sf::Vector2f{size.x, 0.f}, sf::Vector2f{size.x, size.y},
            sf::Vector2f{0.f, 0.f}, sf::Vector2f{size.x, size.y}, sf::Vector2f{0.f, size.y}};
        const std::array<sf::Vector2f, 6> texCoords = {
            sf::Vector2f{uv.position.x, uv.position.y},
            sf::Vector2f{uv.position.x + uv.size.x, uv.position.y},
            sf::Vector2f{uv.position.x + uv.size.x, uv.position.y + uv.size.y},
            sf::Vector2f{uv.position.x, uv.position.y},
            sf::Vector2f{uv.position.x + uv.size.x, uv.position.y + uv.size.y},
            sf::Vector2f{uv.position.x, uv.position.y + uv.size.y}};

        for (size_t i = 0; i < 6; ++i) {
            mVertices[sprite * 6 + i].position = positions[i];
            mVertices[sprite * 6 + i].texCoords = texCoords[i];
        }
    };

    // TODO
    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        sf::VertexArray transformedVertices = mVertices;
        for (size_t sprite = 0; sprite < mNumSprites; ++sprite) {
            for (size_t vertex = 0; vertex < 6; ++vertex) {
                size_t index = sprite * 6 + vertex;
                transformedVertices[index].position =
                    mTransforms[sprite].applyPoint(transformedVertices[index].position);
            }
        }

        states.coordinateType = sf::CoordinateType::Normalized;
        states.texture = &mAtlas.getTexture();
        states.shader = &mShader;
        target.draw(transformedVertices, states);
    };

    // TODO
    void drawGL() {
        glCheck(glUseProgram(mShader.getNativeHandle()));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(mVertices.getVertexCount() * sizeof(sf::Vertex)),
                             &mVertices[0], GL_DYNAMIC_DRAW));
        glCheck(glVertexAttribPointer(static_cast<GLuint>(mPositionAttribLocation), 2, GL_FLOAT,
                                      GL_FALSE, sizeof(sf::Vertex),
                                      reinterpret_cast<void*>(offsetof(sf::Vertex, position))));
        glCheck(glVertexAttribPointer(static_cast<GLuint>(mTexCoordsAttribLocation), 2, GL_FLOAT,
                                      GL_FALSE, sizeof(sf::Vertex),
                                      reinterpret_cast<void*>(offsetof(sf::Vertex, texCoords))));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, mSpriteNumberVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(mSpriteNumbers.size() * sizeof(float)),
                             mSpriteNumbers.data(), GL_STATIC_DRAW));
        glCheck(glVertexAttribPointer(static_cast<GLuint>(mSpriteNumberAttribLocation), 1, GL_FLOAT,
                                      GL_FALSE, sizeof(float), nullptr));

        glCheck(glEnableVertexAttribArray(static_cast<GLuint>(mPositionAttribLocation)));
        glCheck(glEnableVertexAttribArray(static_cast<GLuint>(mTexCoordsAttribLocation)));
        glCheck(glEnableVertexAttribArray(static_cast<GLuint>(mSpriteNumberAttribLocation)));

        glCheck(glActiveTexture(GL_TEXTURE0));
        glCheck(glBindTexture(GL_TEXTURE_1D, mTransformsTexture));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        glCheck(glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F_ARB,
                             static_cast<GLsizei>(mTransforms.size()), 0, GL_RGBA, GL_FLOAT,
                             mTransforms.data()));

        glCheck(glActiveTexture(GL_TEXTURE1));
        glCheck(glBindTexture(GL_TEXTURE_2D, mAtlas.getTexture().getNativeHandle()));

        glCheck(glUniform1i(mTransformsUniformLocation, 0));
        glCheck(glUniform1i(mTextureUniformLocation, 1));
        glCheck(glUniform1i(mNumTransformsUniformLocation,
                            static_cast<GLint>(mTransforms.size())));

        glCheck(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mVertices.getVertexCount())));

        glCheck(glDisableVertexAttribArray(static_cast<GLuint>(mPositionAttribLocation)));
        glCheck(glDisableVertexAttribArray(static_cast<GLuint>(mTexCoordsAttribLocation)));
        glCheck(glDisableVertexAttribArray(static_cast<GLuint>(mSpriteNumberAttribLocation)));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glCheck(glUseProgram(0));
    };

    // TODO
    BasicTransform& getTransform(SpriteID id) {
        assert(id < mNumSprites);
        return mTransforms[id];
    }

    // TODO
    const TextureAtlas& getAtlas() const { return mAtlas; };

private:
    sf::VertexArray mVertices;
    std::vector<BasicTransform> mTransforms;
    size_t mNumSprites;
    TextureAtlas mAtlas;
    mutable sf::Shader mShader;

    // Use the below only in Batched rendering (part 2):
    std::vector<float> mSpriteNumbers;

    GLint mTextureUniformLocation;
    GLint mTransformsUniformLocation;
    GLint mNumTransformsUniformLocation;

    GLint mPositionAttribLocation;
    GLint mTexCoordsAttribLocation;
    GLint mSpriteNumberAttribLocation;

    GLuint mVerticesVBO;
    GLuint mSpriteNumberVBO;
    GLuint mTransformsTexture;
};

#endif  // SPRITE_MANAGER_H
