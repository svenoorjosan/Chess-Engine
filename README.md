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
├── chess_engine/          # PyBind11 module
│   └── chess_wrapper.cpp  # Exposes Game to Python
├── gui.py                 # Pygame front-end
├── images/                # Piece sprites (PNG)
│   ├── wP.png wR.png …     # White pieces
│   └── bP.png bR.png …     # Black pieces
└── requirements.txt       # Python dependencies
