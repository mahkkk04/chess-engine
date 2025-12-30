import pygame
import os
import copy
import random

# Initialize Pygame
pygame.init()

# Set up the display
WIDTH, HEIGHT = 640, 640
SCREEN = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Chess Board")

# Define colors
WHITE = (255, 255, 255)
BROWN = (139, 69, 19)
HIGHLIGHT = (255, 255, 0)
MOVE_HIGHLIGHT = (0, 255, 0, 128)  # Semi-transparent green

# Define board dimensions
BOARD_SIZE = 8
SQUARE_SIZE = WIDTH // BOARD_SIZE

# Load chess pieces
PIECES = {}
for color in ['white', 'black']:
    for piece in ['king', 'queen', 'bishop', 'knight', 'rook', 'pawn']:
        image = pygame.image.load(os.path.join('Images', f'{color}-{piece}.png'))
        PIECES[f'{color}_{piece}'] = pygame.transform.scale(image, (SQUARE_SIZE, SQUARE_SIZE))

# FEN to board conversion
def fen_to_board(fen):
    board = [[None for _ in range(8)] for _ in range(8)]
    fen_parts = fen.split()
    rows = fen_parts[0].split('/')
    for i, row in enumerate(rows):
        col = 0
        for char in row:
            if char.isdigit():
                col += int(char)
            else:
                color = 'white' if char.isupper() else 'black'
                piece_type = char.lower()
                piece_map = {'k': 'king', 'q': 'queen', 'r': 'rook', 'b': 'bishop', 'n': 'knight', 'p': 'pawn'}
                board[i][col] = f"{color}_{piece_map[piece_type]}"
                col += 1
    return board

# Initial FEN
INITIAL_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
board = fen_to_board(INITIAL_FEN)

# Castling rights
castling_rights = {'white': {'kingside': True, 'queenside': True},
                   'black': {'kingside': True, 'queenside': True}}

last_move = None

def highlight_king_in_check(color):
    king_pos = None
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            if board[row][col] == f'{color}_king':
                king_pos = (row, col)
                break
        if king_pos:
            break

    if king_pos:
        r, c = king_pos
        pygame.draw.rect(SCREEN, (255, 0, 0), (c * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE), 4)

def draw_board():
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            color = WHITE if (row + col) % 2 == 0 else BROWN
            pygame.draw.rect(SCREEN, color, (col * SQUARE_SIZE, row * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE))
            piece = board[row][col]
            if piece:
                SCREEN.blit(PIECES[piece], (col * SQUARE_SIZE, row * SQUARE_SIZE))

    # Highlight the king in red if in check
    if is_king_in_check('white', board):
        highlight_king_in_check('white')
    if is_king_in_check('black', board):
        highlight_king_in_check('black')


def get_square(pos):
    x, y = pos
    row = y // SQUARE_SIZE
    col = x // SQUARE_SIZE
    return row, col

def is_valid_square(row, col):
    return 0 <= row < BOARD_SIZE and 0 <= col < BOARD_SIZE

def get_pseudo_legal_moves(row, col, current_board):
    global last_move
    piece = current_board[row][col]
    if not piece:
        return []

    color, piece_type = piece.split('_')
    opponent_color = 'black' if color == 'white' else 'white'
    moves = []

    if piece_type == 'pawn':
        direction = -1 if color == 'white' else 1
        if is_valid_square(row + direction, col) and current_board[row + direction][col] is None:
            moves.append((row + direction, col))
            if (color == 'white' and row == 6) or (color == 'black' and row == 1):
                if is_valid_square(row + 2 * direction, col) and current_board[row + 2 * direction][col] is None:
                    moves.append((row + 2 * direction, col))
        for dc in [-1, 1]:
            if is_valid_square(row + direction, col + dc):
                target = current_board[row + direction][col + dc]
                if target and target.split('_')[0] != color:
                    moves.append((row + direction, col + dc))

        # En passant
        if last_move:
            (last_from, last_to) = last_move
            last_from_row, last_from_col = last_from
            last_to_row, last_to_col = last_to
            if current_board[last_to_row][last_to_col] == f'{opponent_color}_pawn':
                if abs(last_from_row - last_to_row) == 2 and last_to_row == row:
                    if last_to_col == col + 1 or last_to_col == col - 1:
                        moves.append((row + direction, last_to_col))

    elif piece_type == 'knight':
        for dr, dc in [(-2, -1), (-2, 1), (-1, -2), (-1, 2), (1, -2), (1, 2), (2, -1), (2, 1)]:
            r, c = row + dr, col + dc
            if is_valid_square(r, c) and (current_board[r][c] is None or current_board[r][c].split('_')[0] != color):
                moves.append((r, c))

    elif piece_type in ['bishop', 'rook', 'queen']:
        directions = []
        if piece_type in ['bishop', 'queen']:
            directions.extend([(-1, -1), (-1, 1), (1, -1), (1, 1)])
        if piece_type in ['rook', 'queen']:
            directions.extend([(-1, 0), (1, 0), (0, -1), (0, 1)])

        for dr, dc in directions:
            r, c = row + dr, col + dc
            while is_valid_square(r, c):
                if current_board[r][c] is None:
                    moves.append((r, c))
                elif current_board[r][c].split('_')[0] != color:
                    moves.append((r, c))
                    break
                else:
                    break
                r, c = r + dr, c + dc

    elif piece_type == 'king':
        for dr in [-1, 0, 1]:
            for dc in [-1, 0, 1]:
                if dr == 0 and dc == 0:
                    continue
                r, c = row + dr, col + dc
                if is_valid_square(r, c) and (current_board[r][c] is None or current_board[r][c].split('_')[0] != color):
                    moves.append((r, c))

        # Castling
        if color == 'white' and row == 7 and col == 4:
            if castling_rights['white']['kingside'] and all(current_board[7][c] is None for c in range(5, 7)) and current_board[7][7] == 'white_rook':
                moves.append((7, 6))
            if castling_rights['white']['queenside'] and all(current_board[7][c] is None for c in range(1, 4)) and current_board[7][0] == 'white_rook':
                moves.append((7, 2))
        elif color == 'black' and row == 0 and col == 4:
            if castling_rights['black']['kingside'] and all(current_board[0][c] is None for c in range(5, 7)) and current_board[0][7] == 'black_rook':
                moves.append((0, 6))
            if castling_rights['black']['queenside'] and all(current_board[0][c] is None for c in range(1, 4)) and current_board[0][0] == 'black_rook':
                moves.append((0, 2))

    return moves

def get_all_legal_moves(color, current_board):
    legal_moves = []
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            if current_board[row][col] and current_board[row][col].startswith(color):
                moves = get_legal_moves(row, col, current_board)
                for move in moves:
                    legal_moves.append(((row, col), move))
    return legal_moves

def make_ai_move(color, move_count):
    global board
    _, best_move = minimax(board, 2, float('-inf'), float('inf'), True, color, move_count)
    if best_move:
        board = move_piece(best_move[0], best_move[1], board)


def is_king_in_check(color, current_board):
    # Find the king's position
    king_pos = None
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            if current_board[row][col] == f'{color}_king':
                king_pos = (row, col)
                break
        if king_pos:
            break

    # Check if any opponent's piece can capture the king
    opponent_color = 'black' if color == 'white' else 'white'
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            piece = current_board[row][col]
            if piece and piece.startswith(opponent_color):
                moves = get_pseudo_legal_moves(row, col, current_board)
                if king_pos in moves:
                    return True
    return False

def get_all_legal_moves(color, current_board):
    legal_moves = []
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            if current_board[row][col] and current_board[row][col].startswith(color):
                moves = get_legal_moves(row, col, current_board)
                for move in moves:
                    legal_moves.append(((row, col), move))
    return legal_moves

def get_legal_moves(row, col, current_board):
    color = current_board[row][col].split('_')[0]
    pseudo_legal_moves = get_pseudo_legal_moves(row, col, current_board)
    legal_moves = []

    for move in pseudo_legal_moves:
        # Make a copy of the board and make the move
        temp_board = copy.deepcopy(current_board)
        from_row, from_col = row, col
        to_row, to_col = move

        # Handle en passant
        if current_board[row][col].endswith('_pawn') and from_col != to_col and current_board[to_row][to_col] is None:
            temp_board[from_row][to_col] = None

        temp_board[to_row][to_col] = temp_board[from_row][from_col]
        temp_board[from_row][from_col] = None

        # Check if the king is in check after the move
        if not is_king_in_check(color, temp_board):
            legal_moves.append(move)

    return legal_moves

def evaluate_board(board, color, move_count):
    material_score = 0
    positional_score = 0
    development_score = 0
    
    king_wt = 200
    queen_wt = 9
    rook_wt = 5
    knight_wt = 3
    bishop_wt = 3
    pawn_wt = 1
    mobility_wt = 0.1

    # Central squares
    central_squares = [(3,3), (3,4), (4,3), (4,4)]
    semi_central_squares = [(2,2), (2,3), (2,4), (2,5), (3,2), (3,5), (4,2), (4,5), (5,2), (5,3), (5,4), (5,5)]

    # Long diagonals
    long_diagonals = [(0,0), (1,1), (2,2), (3,3), (4,4), (5,5), (6,6), (7,7),
                      (0,7), (1,6), (2,5), (3,4), (4,3), (5,2), (6,1), (7,0)]

    piece_counts = {'white': {}, 'black': {}}
    for row in range(BOARD_SIZE):
        for col in range(BOARD_SIZE):
            piece = board[row][col]
            if piece:
                piece_color, piece_type = piece.split('_')
                piece_counts[piece_color][piece_type] = piece_counts[piece_color].get(piece_type, 0) + 1

                # Positional bonuses
                if piece_type == 'pawn':
                    if (row, col) in central_squares:
                        positional_score += 0.5 if piece_color == 'white' else -0.5
                    elif (row, col) in semi_central_squares:
                        positional_score += 0.3 if piece_color == 'white' else -0.3
                    # Bonus for advancing center pawns in the opening
                    if move_count < 10:
                        if piece_color == 'white' and row < 6 and (col == 3 or col == 4):
                            positional_score += 0.2 * (6 - row)
                        elif piece_color == 'black' and row > 1 and (col == 3 or col == 4):
                            positional_score -= 0.2 * (row - 1)

                elif piece_type == 'knight':
                    if (row, col) in central_squares:
                        positional_score += 0.7 if piece_color == 'white' else -0.7
                    elif (row, col) in semi_central_squares:
                        positional_score += 0.5 if piece_color == 'white' else -0.5

                elif piece_type == 'bishop':
                    if (row, col) in long_diagonals:
                        positional_score += 0.5 if piece_color == 'white' else -0.5

                elif piece_type == 'king':
                    # Encourage early castling
                    if piece_color == 'white':
                        if (row, col) in [(7,1), (7,2), (7,6), (7,7)]:
                            positional_score += 0.7
                    else:
                        if (row, col) in [(0,1), (0,2), (0,6), (0,7)]:
                            positional_score -= 0.7

                # Penalize undeveloped pieces in the opening and early middlegame
                if move_count < 20:
                    if piece_color == 'white':
                        if piece_type in ['knight', 'bishop'] and row == 7:
                            development_score -= 0.5
                        elif piece_type == 'rook' and col in [0, 7] and row == 7:
                            development_score -= 0.3
                    else:
                        if piece_type in ['knight', 'bishop'] and row == 0:
                            development_score += 0.5
                        elif piece_type == 'rook' and col in [0, 7] and row == 0:
                            development_score += 0.3

    material_score = (
        king_wt * (piece_counts['white'].get('king', 0) - piece_counts['black'].get('king', 0)) +
        queen_wt * (piece_counts['white'].get('queen', 0) - piece_counts['black'].get('queen', 0)) +
        rook_wt * (piece_counts['white'].get('rook', 0) - piece_counts['black'].get('rook', 0)) +
        knight_wt * (piece_counts['white'].get('knight', 0) - piece_counts['black'].get('knight', 0)) +
        bishop_wt * (piece_counts['white'].get('bishop', 0) - piece_counts['black'].get('bishop', 0)) +
        pawn_wt * (piece_counts['white'].get('pawn', 0) - piece_counts['black'].get('pawn', 0))
    )

    white_mobility = sum(len(get_legal_moves(row, col, board)) for row in range(BOARD_SIZE) for col in range(BOARD_SIZE) if board[row][col] and board[row][col].startswith('white'))
    black_mobility = sum(len(get_legal_moves(row, col, board)) for row in range(BOARD_SIZE) for col in range(BOARD_SIZE) if board[row][col] and board[row][col].startswith('black'))
    mobility_score = mobility_wt * (white_mobility - black_mobility)

    eval_score = material_score + positional_score + mobility_score + development_score
    return eval_score if color == 'white' else -eval_score

def minimax(board, depth, alpha, beta, maximizing_player, color, move_count):
    if depth == 0:
        return evaluate_board(board, color, move_count), None

    best_move = None
    if maximizing_player:
        max_eval = float('-inf')
        for move in get_all_legal_moves(color, board):
            temp_board = copy.deepcopy(board)
            temp_board = move_piece(move[0], move[1], temp_board)
            eval, _ = minimax(temp_board, depth - 1, alpha, beta, False, 'black' if color == 'white' else 'white', move_count + 1)
            if eval > max_eval:
                max_eval = eval
                best_move = move
            alpha = max(alpha, eval)
            if beta <= alpha:
                break
        return max_eval, best_move
    else:
        min_eval = float('inf')
        for move in get_all_legal_moves(color, board):
            temp_board = copy.deepcopy(board)
            temp_board = move_piece(move[0], move[1], temp_board)
            eval, _ = minimax(temp_board, depth - 1, alpha, beta, True, 'black' if color == 'white' else 'white', move_count + 1)
            if eval < min_eval:
                min_eval = eval
                best_move = move
            beta = min(beta, eval)
            if beta <= alpha:
                break
        return min_eval, best_move
    


def move_piece(from_pos, to_pos, current_board=None):
    global board, castling_rights, last_move
    if current_board is None:
        current_board = board

    from_row, from_col = from_pos
    to_row, to_col = to_pos
    piece = current_board[from_row][from_col]
    color, piece_type = piece.split('_')

    # Update castling rights
    if piece_type == 'king':
        castling_rights[color]['kingside'] = False
        castling_rights[color]['queenside'] = False
    elif piece_type == 'rook':
        if from_col == 0:  # Queenside rook
            castling_rights[color]['queenside'] = False
        elif from_col == 7:  # Kingside rook
            castling_rights[color]['kingside'] = False

    # Handle castling
    if piece_type == 'king' and abs(from_col - to_col) == 2:
        if to_col > from_col:  # Kingside
            current_board[to_row][to_col - 1] = current_board[to_row][7]
            current_board[to_row][7] = None
        else:  # Queenside
            current_board[to_row][to_col + 1] = current_board[to_row][0]
            current_board[to_row][0] = None

    # Handle en passant
    if piece_type == 'pawn' and from_col != to_col and current_board[to_row][to_col] is None:
        current_board[from_row][to_col] = None

    # Move the piece
    current_board[to_row][to_col] = piece
    current_board[from_row][from_col] = None

    # Handle pawn promotion
    if piece_type == 'pawn' and (to_row == 0 or to_row == 7):
        promote_pawn(to_row, to_col, current_board)

    # Only make AI move and update last_move if we're modifying the actual game board
    if current_board is board:
        last_move = (from_pos, to_pos)
        # if color == 'white':
        #     make_ai_move('black')

    return current_board

def promote_pawn(row, col, current_board):
    color = 'white' if row == 0 else 'black'
    options = ['queen', 'rook', 'bishop', 'knight']
    
    # If it's the AI's turn, always promote to queen
    if current_board is not board:
        current_board[row][col] = f'{color}_queen'
        return

    # Create a simple menu for promotion selection
    menu_height = 200
    menu = pygame.Surface((WIDTH, menu_height))
    menu.fill(WHITE)
    for i, piece in enumerate(options):
        piece_img = PIECES[f'{color}_{piece}']
        menu.blit(piece_img, (i * SQUARE_SIZE, (menu_height - SQUARE_SIZE) // 2))
    
    SCREEN.blit(menu, (0, (HEIGHT - menu_height) // 2))
    pygame.display.flip()
    
    waiting = True
    while waiting:
        for event in pygame.event.get():
            if event.type == pygame.MOUSEBUTTONDOWN:
                x, _ = pygame.mouse.get_pos()
                selected = x // SQUARE_SIZE
                if 0 <= selected < len(options):
                    current_board[row][col] = f'{color}_{options[selected]}'
                    waiting = False
    
    # Redraw the board after promotion
    SCREEN.fill(WHITE)
    draw_board()
    pygame.display.flip()

running = True
selected_piece = None
selected_pos = None
legal_moves = []
move_count = 0

clock = pygame.time.Clock()

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.MOUSEBUTTONDOWN:
            if event.button == 1:  # Left mouse button
                row, col = get_square(event.pos)
                if board[row][col]:
                    selected_piece = board[row][col]
                    selected_pos = (row, col)
                    legal_moves = get_legal_moves(row, col, board)
        elif event.type == pygame.MOUSEBUTTONUP:
            if event.button == 1 and selected_piece:
                new_row, new_col = get_square(event.pos)
                if (new_row, new_col) in legal_moves:
                    move_piece(selected_pos, (new_row, new_col))
                    move_count += 1
                    # AI move
                    make_ai_move('black', move_count)
                    move_count += 1
                selected_piece = None
                selected_pos = None
                legal_moves = []

    SCREEN.fill(WHITE)
    draw_board()
    
    # Highlight selected piece and legal moves
    if selected_pos:
        row, col = selected_pos
        pygame.draw.rect(SCREEN, HIGHLIGHT, (col * SQUARE_SIZE, row * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE), 4)
        for move in legal_moves:
            r, c = move
            s = pygame.Surface((SQUARE_SIZE, SQUARE_SIZE), pygame.SRCALPHA)
            s.fill(MOVE_HIGHLIGHT)
            SCREEN.blit(s, (c * SQUARE_SIZE, r * SQUARE_SIZE))

    pygame.display.flip()
    clock.tick(60)  # Limit to 60 FPS

pygame.quit()
