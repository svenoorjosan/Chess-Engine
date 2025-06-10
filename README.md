# Python × C++ Chess

A simple, high-performance chess engine written in C++ with a Python/Pygame front-end. Under the hood it uses alpha-beta search, MVV-LVA move ordering, and a basic transposition table. The Python GUI (wrapped via PyBind11) provides drag-and-drop play, clocks, and theme selection.

---

## Features

- **C++ Engine**  
  - Alpha-beta search with configurable depth  
  - MVV-LVA capture ordering  
  - Simple per-search transposition table  
  - Complete legal-move generation (checks, checkmates, stalemates)  
  - Three difficulty levels (including random blunders on Easy)

- **Python GUI**  
  - Pygame rendering of board and pieces  
  - Drag-and-drop movement with move highlighting  
  - Side clocks (White & Black)  
  - Difficulty selector and board theme picker  
  - Threaded AI thinking to keep UI responsive

---

## Repository Layout

```text
├── chess.cpp              # PyBind11 module
│── engine.cpp             # Exposes Game to Python
├── gui.py                 # Pygame front-end
├── images/                # Piece sprites (PNG)
│   ├── wP.png wR.png …    # White pieces
└── └── bP.png bR.png …    # Black pieces
```
## Prerequisites

- **C++ compiler** with C++11 support (e.g. g++, clang++)  
- **CMake** ≥ 3.10  
- **Python** ≥ 3.7  
- **PyBind11**  
- **Pygame**  

## Installation

1. **Clone the repo**
2. **Locate the repo**

## Compile
```bash
c++ -std=c++17 -O3 -shared -fPIC \
  -I/mingw64/include \
  -I/mingw64/include/python3.12 \
  -I/mingw64/include/pybind11 \
  engine.cpp \
  -L/mingw64/lib -lpython3.12 \
  -o chess_engine.pyd
```
## Run 
```bash
python gui.py
```
![Image](https://github.com/user-attachments/assets/bdb0cc5d-bebb-4a59-bc14-41416dfa04ab)

