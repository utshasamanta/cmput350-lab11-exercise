#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size)
        : mBinSize(size), mRects{}, mCurLineY{0}, mCurLineX{0}, mCurLineHeight{0} {}
    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        if (rectSize.x <= 0 || rectSize.y <= 0 || rectSize.x > mBinSize.x ||
            rectSize.y > mBinSize.y) {
            return {false, std::numeric_limits<size_t>::max()};
        }
        if (mCurLineX + rectSize.x > mBinSize.x) {
            // new line
            mCurLineX = 0.f;
            mCurLineY += mCurLineHeight;
            mCurLineHeight = rectSize.y;
        }
        if (rectSize.y > mBinSize.y - mCurLineY) {
            return {false, std::numeric_limits<size_t>::max()};
        }
        size_t ret = mRects.size();
        mRects.emplace_back(sf::Vector2f{mCurLineX, mCurLineY}, rectSize);
        mCurLineX += rectSize.x;
        mCurLineHeight = std::max(mCurLineHeight, rectSize.y);
        return {true, ret};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const {
        assert(rectID < mRects.size());
        return mRects[rectID];
    }

private:
    sf::Vector2f mBinSize;              // the size of the large rectangle in which to pack.
    std::vector<sf::FloatRect> mRects;  // the packed rectangle positions + sizes
    float mCurLineY;       // the current horizontal line along which to pack rectangle top-borders.
    float mCurLineX;       // the current offset into the horizontal line
    float mCurLineHeight;  // the tallest height we've seen so far in the line.
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, unsigned pixels) {
    sf::Image ret(img.getSize() + sf::Vector2u{2 * pixels, 2 * pixels});
    if (!ret.copy(img, {pixels, pixels})) {
        throw std::runtime_error("Failed to copy original image");
    }
    const std::array<sf::Vector2i, 4> offsets = {
        sf::Vector2i{0, 0},                                                             // top
        sf::Vector2i{0, static_cast<int>(pixels) + static_cast<int>(img.getSize().y)},  // bottom
        sf::Vector2i{0, static_cast<int>(pixels)},                                      // left
        sf::Vector2i{static_cast<int>(pixels) + static_cast<int>(img.getSize().x),
                     static_cast<int>(pixels)}  // right
    };
    const std::array<sf::Vector2i, 4> boundsSizes = {
        sf::Vector2i{static_cast<int>(ret.getSize().x), static_cast<int>(pixels)},  // top
        sf::Vector2i{static_cast<int>(ret.getSize().x), static_cast<int>(pixels)},  // bottom
        sf::Vector2i{static_cast<int>(pixels), static_cast<int>(img.getSize().y)},  // left
        sf::Vector2i{static_cast<int>(pixels), static_cast<int>(img.getSize().y)}   // right
    };
    for (int dir = 0; dir < 4; ++dir) {
        sf::Vector2i offset = offsets[dir];
        for (int dy = 0; dy < boundsSizes[dir].y; ++dy) {
            for (int dx = 0; dx < boundsSizes[dir].x; ++dx) {
                sf::Vector2i samplePos =
                    (offset + sf::Vector2i{dx, dy} -
                     sf::Vector2i{static_cast<int>(pixels), static_cast<int>(pixels)});
                samplePos.x = std::clamp(samplePos.x, 0, static_cast<int>(img.getSize().x) - 1);
                samplePos.y = std::clamp(samplePos.y, 0, static_cast<int>(img.getSize().y) - 1);
                ret.setPixel(
                    sf::Vector2u(offset) +
                        sf::Vector2u{static_cast<unsigned>(dx), static_cast<unsigned>(dy)},
                    img.getPixel({static_cast<unsigned>(samplePos.x), static_cast<unsigned>(samplePos.y)}));
            }
        }
    }
    return ret;
}

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, unsigned padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image img{std::filesystem::path(filepath)};
        img = padImage(img, mPadding);
        auto [success, retID] = mPacker.pack(sf::Vector2f(img.getSize()));
        if (!success) {
            throw std::runtime_error("Unable to pack " + filepath);
        }
        sf::FloatRect pixelRect = mPacker.getBounds(retID);
        // floats will have enough precision such that textures of width or height < 2^24
        // can be casted to int, which is sufficient for us (typically gpus might max at 2^15
        // for texture dimensions)
        mTexture.update(img, sf::Vector2u(pixelRect.position));
        return retID;
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        sf::FloatRect boundsPixels = mPacker.getBounds(id);
        // first, fix bounds using padding info:
        boundsPixels.size -= sf::Vector2f{2.f * mPadding, 2.f * mPadding};
        boundsPixels.position +=
            sf::Vector2f{static_cast<float>(mPadding), static_cast<float>(mPadding)};
        // Rescale to [0, 1]^2
        boundsPixels.size = boundsPixels.size / sf::Vector2f(mTexture.getSize());
        boundsPixels.position = boundsPixels.position / sf::Vector2f(mTexture.getSize());
        // finally, account for the fact that UV coords in OpenGL have origin at bottom-left:
        boundsPixels.position.y = 1.f - (boundsPixels.position.y + boundsPixels.size.y);

        return boundsPixels;
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        return sf::Vector2u(mPacker.getBounds(id).size) - sf::Vector2u{2 * mPadding, 2 * mPadding};
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    unsigned mPadding;
};

#endif  // TEXTURE_ATLAS_H
