# Okari Engine

Okari Engine is a custom 3D game engine written in C++ using OpenGL and GLFW.

It is built with a clear goal: provide a **clean, modular, and gameplay-oriented architecture** capable of supporting experiences similar to *The Legend of Zelda: Twilight Princess* — exploration, interaction, puzzles, and atmospheric world-building.

Rather than being a generic, all-purpose engine, Okari focuses on **clarity, control, and scalability**, allowing systems to evolve naturally alongside the needs of the game.

---

## Philosophy

The engine follows a simple principle:

> **Build only what the game needs, but build it properly.**

This project is not about recreating a massive engine like Unreal, but about building a **lean, understandable, and extensible foundation** where:

- gameplay systems remain flexible
- rendering stays decoupled
- data drives the world (not hardcoded logic)

---

## Architecture

The project is organized into three main layers:

```
Engine/ → Core systems (rendering, input, window, scene…)
Game/ → Gameplay logic (actors, world, behaviors)
Editor/ → Integrated level editor (ImGui-based tools)
```

This separation ensures:

- the engine remains reusable
- gameplay stays isolated
- tools evolve independently without polluting runtime logic

---

## Runtime Overview

The engine is built around a simple and explicit execution model:

```
Application
→ Layer (Game or Editor)
→ Update / Render loop
```

This allows different contexts to run on top of the same engine:

- a **game runtime** (Sandbox)
- an **editor runtime** (OkariEditor)

---

## Rendering

Okari uses a straightforward OpenGL-based pipeline:

- Shader abstraction (GLSL)
- Model / View / Projection workflow
- Texture sampling
- Depth testing

Rendering is intentionally minimal and controlled, allowing the pipeline to grow alongside the project without unnecessary complexity.

---

## World System

The world is **data-driven** and built around simple, composable objects.

Each object contains:

- a transform (position, rotation, scale)
- rendering data (e.g. texture)

Scenes are loaded and saved from JSON, making iteration fast and explicit.

---

## Integrated Editor

Okari Engine includes a built-in editor powered by Dear ImGui.

The editor is designed to directly use engine systems, not replicate them.

It provides a structured workspace with:

- a **3D viewport** rendering the scene through an offscreen framebuffer
- a **hierarchy panel** to navigate scene objects
- an **inspector panel** to view and edit object data
- an **asset browser** for future content management

The editor is fully dockable and follows a classic layout inspired by modern engines.

---

## Design Approach

The engine is developed with long-term maintainability in mind:

- no reliance on object names for logic
- clear separation between data and behavior
- minimal hidden systems
- explicit control over memory and flow

The goal is to avoid fragile abstractions and build systems that remain understandable even as complexity grows.

---

## Assets

Project assets are stored in a dedicated structure:

```
Assets/
Shaders/
Textures/
Levels/
```

They are automatically copied to the build directory after compilation.

---

## Build

### Requirements

- CMake
- C++17 compatible compiler
- OpenGL-compatible GPU

### Dependencies (included in `External/`)

- GLFW
- GLAD
- GLM
- stb_image
- nlohmann/json
- Dear ImGui (editor)

### Build

```bash
cmake -B build
cmake --build build
```

---

## Author

Developed with ❤️ by **TheObtey**, as part of a personal journey into game engine architecture, rendering systems, and interactive tools.