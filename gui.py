
import sys
import pygame
import concurrent.futures
import time
import traceback
import chess_engine

# ─── Config ────────────────────────────────────────────────────────────
TILE = 80
LIGHT_WOOD = (240, 217, 181)
DARK_WOOD  = (181, 136,  99)
HIGHLIGHT  = (118, 150,  86)

# ─── Pygame init ───────────────────────────────────────────────────────
pygame.init()
screen = pygame.display.set_mode((8 * TILE, 8 * TILE + 40))
pygame.display.set_caption("Python × C++ Chess")
font_big = pygame.font.SysFont(None, 48)
font_sm  = pygame.font.SysFont(None, 32)

# ─── Piece images ──────────────────────────────────────────────────────
pieces = {}
for color in ('w', 'b'):
    for piece in ('P', 'R', 'N', 'B', 'Q', 'K'):
        key = color + piece
        try:
            img = pygame.image.load(f"images/{key}.png")
        except pygame.error:
            print(f"Missing image: images/{key}.png")
            sys.exit(1)
        pieces[key] = pygame.transform.smoothscale(img, (TILE, TILE))

# ─── Helper: draw whole board ──────────────────────────────────────────
highlight_moves = []

def draw_board(board64: str, w_time: float, b_time: float) -> None:
    """Render the board + clocks onto the global 'screen'."""
    # Squares
    for r in range(8):
        for f in range(8):
            color = LIGHT_WOOD if (r + f) % 2 == 0 else DARK_WOOD
            pygame.draw.rect(screen, color, (f * TILE, r * TILE, TILE, TILE))

    # Move targets
    for sq in highlight_moves:
        cx = (ord(sq[0]) - 97) * TILE + TILE // 2
        cy = (8 - int(sq[1]))   * TILE + TILE // 2
        pygame.draw.circle(screen, HIGHLIGHT, (cx, cy), TILE // 6)

    # Pieces
    for r in range(8):
        for f in range(8):
            pc = board64[r * 8 + f]
            if pc == '\x00':
                continue
            key = ('w' if pc.isupper() else 'b') + pc.upper()
            screen.blit(pieces[key], (f * TILE, r * TILE))

    # Clock bar
    pygame.draw.rect(screen, (30, 30, 30), (0, 640, 640, 40))
    w_surf = font_sm.render(f"White: {int(w_time)}s", True, (255, 255, 255))
    b_surf = font_sm.render(f"Black: {int(b_time)}s", True, (255, 255, 255))
    screen.blit(w_surf, (10, 645))
    screen.blit(b_surf, (500, 645))
    pygame.display.flip()

# ─── Level picker ──────────────────────────────────────────────────────
def pick_level() -> int:
    imgs = [pygame.image.load(p) for p in ("easy.png", "medium.png", "hard.png")]
    labels = ["Easy", "Medium", "Hard"]
    img_size = 140
    spacing  = 60
    for i in range(3):
        imgs[i] = pygame.transform.smoothscale(imgs[i], (img_size, img_size))
    total_width = 3 * img_size + 2 * spacing
    start_x = (640 - total_width) // 2
    y_img  = 160
    y_text = y_img + img_size + 10
    while True:
        screen.fill((10, 20, 60))
        title = font_big.render("Select Difficulty", True, (255, 255, 255))
        screen.blit(title, (180, 50))
        for i in range(3):
            x = start_x + i * (img_size + spacing)
            screen.blit(imgs[i], (x, y_img))
            label = font_sm.render(labels[i], True, (255, 255, 255))
            screen.blit(label, label.get_rect(center=(x + img_size // 2, y_text)))
        pygame.display.flip()
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                pygame.quit(); sys.exit()
            if e.type == pygame.MOUSEBUTTONDOWN:
                x, y = e.pos
                for i in range(3):
                    bx = start_x + i * (img_size + spacing)
                    if bx <= x <= bx + img_size and y_img <= y <= y_img + img_size:
                        return i + 1

# ─── Theme picker ──────────────────────────────────────────────────────
def pick_theme() -> tuple:
    themes = [
        ("Classic", (240, 217, 181), (181, 136, 99)),
        ("Blue",    (180, 180, 255), ( 70,  70,180)),
        ("Teal",    (185, 245, 235), (  0, 155,135)),
    ]
    idx = 0
    while True:
        screen.fill((30, 30, 40))
        title = font_big.render("Select Theme", True, (255, 255, 255))
        screen.blit(title, (200, 40))
        # Mini boards
        for t in range(3):
            for r in range(4):
                for f in range(4):
                    col = themes[t][1] if (r + f) % 2 == 0 else themes[t][2]
                    pygame.draw.rect(screen, col, (60 + t * 180 + f * 20, 120 + r * 20, 20, 20))
            label = font_sm.render(themes[t][0], True, (255, 255, 255))
            screen.blit(label, (90 + t * 180, 220))
        # OK button
        pygame.draw.rect(screen, (255, 170, 0), (260, 300, 120, 40))
        screen.blit(font_sm.render("OK", True, (0, 0, 0)), (305, 310))
        pygame.display.flip()
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                pygame.quit(); sys.exit()
            if e.type == pygame.MOUSEBUTTONDOWN:
                x, y = e.pos
                for t in range(3):
                    if 60 + t*180 <= x <= 140 + t*180 and 120 <= y <= 200:
                        idx = t
                if 260 <= x <= 380 and 300 <= y <= 340:
                    return themes[idx][1], themes[idx][2]
                
BAR_TOP    = 640          # where we draw the bottom bar
BAR_HEIGHT = 40           # its thickness


# ─── Main game loop ────────────────────────────────────────────────────
def main():
    global LIGHT_WOOD, DARK_WOOD, highlight_moves
    try:
        level = pick_level()
        LIGHT_WOOD, DARK_WOOD = pick_theme()
        game = chess_engine.Game(level)

        executor    = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        ai_future   = None
        board_snap  = game.board64()   # stable board copy

        drag_from   = None
        status_msg  = ""
        game_over   = False

        # Clocks
        w_start = time.time()
        b_start = None
        w_time  = 0
        b_time  = 0
        player_turn = 'w'

        while True:
            # Clock update
            current_w = w_time + (time.time() - w_start if player_turn == 'w' and not game_over else 0)
            current_b = b_time + (time.time() - b_start if player_turn == 'b' and not game_over and b_start else 0)

            # Draw
            draw_board(board_snap if ai_future else game.board64(), current_w, current_b)

            # Event loop
            for e in pygame.event.get():
                if e.type == pygame.QUIT:
                    executor.shutdown(wait=False)
                    pygame.quit(); sys.exit()

                if game_over:   # Ignore inputs after game ends
                    continue

                # Mouse actions
                if e.type == pygame.MOUSEBUTTONDOWN and ai_future is None and player_turn == 'w':
                    # Start drag
                    if not drag_from:
                        drag_from = (e.pos[1] // TILE, e.pos[0] // TILE)
                        all_moves = game.legalMoves()
                        sq = f"{chr(drag_from[1] + 97)}{8 - drag_from[0]}"
                        highlight_moves = [m[2:4] for m in all_moves if m.startswith(sq)]
                    # End drag
                    else:
                        r0, c0 = drag_from
                        r1, c1 = e.pos[1] // TILE, e.pos[0] // TILE
                        mv = f"{chr(c0+97)}{8-r0}{chr(c1+97)}{8-r1}"
                        drag_from = None
                        highlight_moves = []
                        if game.playerMove(mv):
                            # Update clocks
                            w_time += time.time() - w_start
                            b_start = time.time()
                            player_turn = 'b'
                            board_snap = game.board64()  # freeze board
                            # Game state
                            if   game.isCheckmate(): status_msg, game_over = "You mated!", True
                            elif game.isStalemate(): status_msg, game_over = "Stalemate", True
                            elif game.inCheck():     status_msg = "Black in check!"
                            else:                    status_msg = ""
                            # Launch AI
                            if not game_over:
                                status_msg = "AI thinking…"
                                ai_future = executor.submit(game.aiMove)

            # Poll AI future
            if ai_future and ai_future.done():
                ai_future.result()              # apply move
                board_snap = game.board64()      # unfreeze
                b_time += time.time() - b_start
                w_start = time.time()
                player_turn = 'w'
                if   game.isCheckmate(): status_msg, game_over = "AI mated you", True
                elif game.isStalemate(): status_msg, game_over = "Stalemate", True
                elif game.inCheck():     status_msg = "White in check!"
                else:                    status_msg = ""
                ai_future = None

            # Status text
            if status_msg:
                surf = font_sm.render(status_msg, True, (255, 0, 0))
                rect = surf.get_rect(midleft=(320 - surf.get_width() // 2, 645))
                screen.blit(surf, rect)
                pygame.display.flip()


    except Exception:
        traceback.print_exc()
        input("\n[Crash] Press Enter to exit…")

if __name__ == "__main__":
    main()
