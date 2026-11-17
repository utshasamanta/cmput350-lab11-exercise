#include <chrono>
#include <iostream>
#include <numbers>
#include <vector>

#include "basic_transform.h"
#include "glad/glad.h"
#include "pcg.h"
#include "sprite_manager.h"
#include "texture_atlas.h"
#include "timer.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION

constexpr size_t WINDOW_WIDTH = 1280;
constexpr size_t WINDOW_HEIGHT = 720;
constexpr size_t BALL_RADIUS = std::min(WINDOW_WIDTH, WINDOW_HEIGHT) / 3;
constexpr size_t NUM_PARTICLES = 10000;

enum ParticleColor { RED = 0, YELLOW = 1, PURPLE = 2 };

// Given an rng, and an x-rotation-offset, generates a random point on the 3D unit sphere,
// together with a position-dependent color.
// You are not responsible for understanding the position generation code.
std::pair<sf::Vector3f, ParticleColor> randomColoredPointOnSphere(PCG& rng, float xRot,
                                                                  [[maybe_unused]] float yRot) {
    double u = (double)rng() / rng.max();
    double v = (double)rng() / rng.max();
    double theta = 2 * std::numbers::pi * (u) + xRot;
    double phi = std::acos(2 * v - 1);
    double cos_theta = std::cos(theta);
    double sin_theta = std::sin(theta);
    double cos_phi = std::cos(phi);
    double sin_phi = std::sin(phi);
    float colorRand = static_cast<float>(rng() / std::numeric_limits<uint32_t>::max());

    ParticleColor color = (colorRand < 0.2)    ? ParticleColor::YELLOW
                          : (colorRand > 0.92) ? ParticleColor::PURPLE
                                               : ParticleColor::RED;

    return {{static_cast<float>(cos_theta * sin_phi), static_cast<float>(sin_theta * sin_phi),
             (float)cos_phi},
            color};
}

int main() {
    // OpenGL settings we request for:
    sf::ContextSettings settings;
    settings.depthBits = 24;   // 24-bit depth buffer
    settings.stencilBits = 8;  // 8-bit stencil buffer
    // Request OpenGL 2.1 for compatibility with both SFML/Graphics AND macOS:
    //  - SFML's Graphics module implementation relies on legacy fixed-function pipeline features
    //     of OpenGL, which means only up to OpenGL 2.1. Any newer version requires Compatibility
    //     profile (i.e., maintaining legacy features).
    //  - macOS has deprecated OpenGL support, meaning for anything newer than 2.1 (i.e., 3.0 and
    //      newer), only Core profile is supported, i.e., REMOVING legacy fixed-function features.
    // As a result, if you want to both support macOS AND use SFML's Graphics features (e.g.,
    // sf::RenderWindow), you can use at latest OpenGL 2.1, which is a nightmare.
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    sf::RenderWindow window(sf::VideoMode({1280, 720}), "Shaders", sf::State::Windowed, settings);
    (void)window.setActive();  // set window as OpenGL target
    // window.setMouseCursorVisible(false); // hide cursor

    if (!gladLoadGLLoader((GLADloadproc)sf::Context::getFunction)) {
        // Handle GLAD initialization error
        return -1;
    }

    // Query the settings we actually received (may be different):
    const auto& actualSettings = window.getSettings();
    std::cout << "OpenGL version " << actualSettings.majorVersion << "."
              << actualSettings.minorVersion << " flags: " << actualSettings.attributeFlags << '\n';
    std::cout << "Maximum texture width (or height): " << sf::Texture::getMaximumSize() << '\n';

    sf::Texture tex1("assets/dragon_dagger.png");
    std::array<sf::Texture, 3> particleTextures = {
        sf::Texture("assets/particle_red.png"),
        sf::Texture("assets/particle_yellow.png"),
        sf::Texture("assets/particle_purple.png"),
    };

    sf::Vector2u particleTextureSize = particleTextures[0].getSize();

    sf::Sprite cursorSprite(tex1);
    cursorSprite.setScale({2.f, 2.f});

    // Particle generation
    PCG rng{};
    PCG rng2{};
    std::vector<sf::Sprite> particleSprites;
    particleSprites.reserve(NUM_PARTICLES);
    std::vector<ParticleColor> originalColors;
    originalColors.reserve(NUM_PARTICLES);
    std::vector<bool> flipped(NUM_PARTICLES, false);
    for (size_t i = 0; i < NUM_PARTICLES; ++i) {
        // Use rng to determine particle position
        auto [pos, color] = randomColoredPointOnSphere(rng, 0, 0);
        auto sprite = sf::Sprite(particleTextures[static_cast<size_t>(color)]);
        particleSprites.push_back(sprite);

        particleSprites.back().setPosition(
            sf::Vector2f{pos.y * BALL_RADIUS, pos.z * BALL_RADIUS} +
            sf::Vector2f(static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)) *
                0.5f -
            sf::Vector2f(particleTextureSize) * 0.05f);
        particleSprites.back().setScale({0.1f, 0.1f});
        originalColors.push_back(color);
    }

    glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
    glClearColor(0, 0, 0, 1);

    Timer timer;
    float frameTime = 0.1;  // EMA
    size_t frameNum = 0;
    while (window.isOpen()) {
        while (auto e = window.pollEvent()) {
            if (e->is<sf::Event::Closed>()) {
                return 0;
            }
        }
        auto mousePos = sf::Vector2f(sf::Mouse::getPosition(window));

        cursorSprite.setPosition(sf::Vector2f(sf::Mouse::getPosition(window)));
        // spriteManager.getTransform(cursorSprite).translation.y
        //     = static_cast<float>(window.getSize().y + 2)
        //     - spriteManager.getTransform(cursorSprite).translation.y;

        cursorSprite.setPosition(cursorSprite.getPosition() + sf::Vector2f{-26, 0});

        // reset rng to regenerate spherical positions
        rng.reset();
        for (size_t i = 0; i < NUM_PARTICLES; ++i) {
            // Use rng to determine particle position
            auto [pos, color] = randomColoredPointOnSphere(rng, mousePos.x / WINDOW_WIDTH, 0);

            particleSprites[i].setPosition(
                sf::Vector2f{pos.y * BALL_RADIUS, pos.z * BALL_RADIUS} +
                sf::Vector2f(static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)) *
                    0.5f -
                sf::Vector2f(particleTextureSize) * 0.05f);

            if ((mousePos -
                 (particleSprites[i].getPosition() + sf::Vector2f(particleTextureSize) * 0.05f))
                    .lengthSquared() < 400) {
                flipped[i] = true;
                particleSprites[i].setTexture(particleTextures[ParticleColor::PURPLE]);
            }
        }

        // use rng to determine resets
        for (size_t i = 0; i < NUM_PARTICLES; ++i) {
            if ((static_cast<double>(rng2()) / std::numeric_limits<uint32_t>::max()) < 0.005) {
                flipped[i] = false;
                particleSprites[i].setTexture(particleTextures[originalColors[i]]);
            }
        }

        frameTime += 0.01 * (timer.click<Timer::Millis>() - frameTime);
        ++frameNum;
        if (frameNum % 60 == 0) {
            std::cout << "Average fps: " << 1000 * 1.f / frameTime << '\n';
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        sf::RenderStates states{};
        states.blendMode = sf::BlendAdd;
        for (const auto& sprite : particleSprites) {
            window.draw(sprite, states);
        }
        window.draw(cursorSprite, states);
        window.display();
    }
    return 0;
}
