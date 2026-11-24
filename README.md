# Swarm Evader — Swarm Intelligence Demonstrator & Report

**Author:** Prasad Desale  
**Guide:** Prof. K. K. Deshmukh  
**Repository:** https://github.com/Pras2005/swarm-evader

A fast, zero-dependency C++20 interactive swarm intelligence demonstrator and predator-evasion game, accompanied by a headless empirical benchmarking suite and a compiled LaTeX academic report.

## Features
- **Reynolds Boids + Predator Avoidance**: Real-time cohesion, alignment, inverse-distance separation, quadratic threat avoidance, shockwave pulse dispersion, and soft boundary containment.
- **Interactive Game / Sandbox (`./swarm_sim`)**:
  - **Mouse** or **WASD / Arrow Keys**: Move the predator/controller point.
  - **Left Click** or **Space**: Trigger an expanding repulsion shockwave pulse.
  - **Right Click** or **E**: Toggle between *Repel* (evasion) and *Attract* (orbital swarming) modes.
  - **`+` / `-`**: Adjust swarm population live (`10` to `10,000` boids).
  - **`1` / `2` / `3`**: Switch presets (*Calm Flock*, *Predator Evasion*, *Dense Murmuration*).
  - **`G`**: Toggle spatial hash grid & perception radius debug overlay.
  - **`F12`**: Save a `screenshot.ppm` snapshot of the live framebuffer.
  - **`P`** / **`R`** / **`Q` or `Esc`**: Pause / Reset / Quit.
- **Zero-Allocation Spatial Hash Grid**: Intrusive flat-array linked lists (`cellHeads` + `next`) accelerate neighbor queries by $>5\times$ at $N = 4{,}000$.
- **Live Collective Metrics HUD**: FPS, step time (ms), polarization $\Phi$, panic fraction, nearest-neighbor distance, and tag score.

## Build & Run

```bash
make all test          # Compile swarm_sim & run unit tests
./swarm_sim            # Launch interactive X11 window
./swarm_sim --headless 180              # Run 180 frames headless
./swarm_sim --benchmark report/data     # Regenerate CSV datasets
make report            # Compile report/report.pdf via pdflatex + bibtex
```

## Project Layout
- `src/vec2.hpp` — Lightweight 2D vector algebra
- `src/swarm.hpp`, `src/swarm.cpp` — Boids physics, predator avoidance, spatial grid, and order metrics
- `src/renderer.hpp`, `src/renderer.cpp` — Pure X11 `XImage` software framebuffer + 5x7 bitmap HUD
- `src/main.cpp` — Interactive loop, headless smoke test, and `--benchmark` CSV generator
- `tests/test_swarm.cpp` — Unit tests (spatial grid equivalence vs. naive $O(N^2)$, avoidance direction, stability)
- `report/report.tex`, `report/references.bib`, `report/report.pdf` — Academic report with TikZ diagrams and `pgfplots` figures
