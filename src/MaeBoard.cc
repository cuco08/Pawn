#include "MaeBoard.h"
#include "FileReader.h"
#include "Rook.h"
#include "Knight.h"
#include "Bishop.h"
#include "Queen.h"
#include "King.h"
#include "Pawn.h"

using std::string;

/*=============================================================================
  CONST STATIC INITIALIZATION BLOCK
  ===========================================================================*/
const Square
MaeBoard::EMPTY_SQUARE = { Piece::NULL_PLAYER, Piece::NULL_PIECE };

/*=============================================================================
  Build a new board as specified in the file initial.in
  ===========================================================================*/
MaeBoard::MaeBoard ()
{
   load_zobrist ();
   load_support_data ();
   reset ();
}

/*=============================================================================
 | Build a new board as specified in the file FILE_NAME (see initial.in for an|
 | example of the format used in load files)                                  |
 =============================================================================*/
MaeBoard::MaeBoard (const string& file)
{
   load_zobrist ();
   load_support_data ();
   clear ();

   if (!load_game (file))
      reset ();
}

MaeBoard::~MaeBoard ()
{
   for (Piece::Type piece = Piece::PAWN; piece <= Piece::KING; ++piece)
      delete this->chessmen[piece];

   this->position_counter.reset ();
   while (!this->game_history.empty ())
      this->game_history.pop ();
}

/*=============================================================================
 | Create random 64-bit integers to help maintain the hash key for THIS board |
 |                                                                            |
 | Things to consider include pieces, castling privileges, turn and en_passant|
 | capture possibility. A hash lock is also used to avoid collisions in the   |
 | hash table that serves as dictionary of board configurations in the Search |
 | module.                                                                    |
 =============================================================================*/
void
MaeBoard::load_zobrist ()
{
  srand (RANDOM_SEED);

  for (uint i = 0; i < PIECE_TYPES; ++i)
    for (uint j = 0; j < PLAYERS; ++j)
      for (uint k = 0; k < SQUARES; ++k)
         for (uint m = 0; m < N_HASH_KEYS; ++m)
            this->zobrist[i][j][k][m] = Util::rand64 ();

  this->turn_key = Util::rand64 ();
  this->castle_key[Piece::WHITE][KING_SIDE]  = Util::rand64 ();
  this->castle_key[Piece::WHITE][QUEEN_SIDE] = Util::rand64 ();
  this->castle_key[Piece::BLACK][KING_SIDE]  = Util::rand64 ();
  this->castle_key[Piece::BLACK][QUEEN_SIDE] = Util::rand64 ();

  // This is a waste of memory, since only 16 squares can be possible
  // en-passant capture squares, but this avoids dealing with awful offsets
  for (uint i = 0; i < SQUARES; ++i)
        this->en_passant_key[i] = Util::rand64 ();

  this->hash_key = 0;
  this->hash_lock = 0;
}

/*============================================================================
 | Remove all the pieces from the board and reset game status, en-passant    |
 | capture possibilities, current turn, castling privileges, etc.            |
 ============================================================================*/
void
MaeBoard::clear ()
{
   // Clear every bitboard
   this->all_pieces = 0;
   for (Piece::Player side = Piece::WHITE; side <= Piece::BLACK; ++side)
   {
      this->pieces[side] = 0;
      for (Piece::Type type = Piece::PAWN; type <= Piece::KING; ++type)
         this->piece[side][type] = 0;

      // Clear castle privileges
      this->can_do_castle[side][KING_SIDE] = true;
      this->can_do_castle[side][QUEEN_SIDE] = true;
      this->is_castled_[side][KING_SIDE] = false;
      this->is_castled_[side][QUEEN_SIDE] = false;
   }
   // Clear array of squares
   for (Squares square = a8; square <= h1; ++square)
      this->board[square] = EMPTY_SQUARE;

   // Clear turn information
   this->is_whites_turn = true;
   this->player = Piece::WHITE;
   this->opponent = Piece::BLACK;

   // Clear en-passant captures information
   this->en_passant_capture_square = 0;
   this->game_status = PENDING_GAME;

   // Clear the hash keys
   this->hash_key = this->hash_lock = 0;

   // Empty the hash table used to detect threefold repetition
   this->position_counter.reset ();

   // Empty the game history stack
   while (!this->game_history.empty ())
      this->game_history.pop ();

   this->fifty_move_counter = 0;
}

/*=============================================================================
  Set board to start a new game: put pieces in their initial positions, reset
  castling privileges, en-passant capture possibilities, and current turn.

  Precondition: there exists a file initial.in in the current directory with
  a valid initial chess configuration.
  ===========================================================================*/
void
MaeBoard::reset ()
{
   clear ();
   if (!load_game ("initial.in"))
      clear ();
}

/*=============================================================================
  Return TRUE if FILE_NAME could be opened and contained a valid chess game.

  Postcondition: The board contains now a valid configuration contained in the
  specified file, which may be either a pending or a finished game.
  ===========================================================================*/
bool
MaeBoard::load_game (const string& file)
{
   FileReader game_file (file, this);
   bool load_status = game_file.set_variables ();

   return load_status;
}

/*=============================================================================
  Return TRUE if the current game was successfully saved to FILE_NAME.
  ===========================================================================*/
bool
MaeBoard::save_game (const string& file)
{
   return file.size() > 0;
}

/*=============================================================================
  Return TRUE if the specified piece was added to the board in LOCATION
  ===========================================================================*/
bool
MaeBoard::add_piece (
    const string& location, Piece::Type type, Piece::Player player)
{
   Squares square;

   if (!Move::translate_to_square (location, square))
      return false;

   return add_piece (square, type, player);
}
/*=============================================================================
  Return TRUE if the specified piece was added to the board in LOCATION
  ===========================================================================*/
bool
MaeBoard::add_piece (Squares square, Piece::Type type, Piece::Player player)
{
   if (this->board[square] != EMPTY_SQUARE)
      return false;

   // Add piece in the bitboard representations
   this->piece[player][type] |= Util::to_bitboard[square];
   this->pieces[player] |= Util::to_bitboard[square];
   this->all_pieces |= Util::to_bitboard[square];

   this->board[square].player = player;
   this->board[square].piece = type;

   this->hash_key ^= zobrist[type][player][square][0];
   this->hash_lock ^= zobrist[type][player][square][1];

   return true;
}

/*=============================================================================
  Return TRUE if a piece was removed from square LOCATION
  ===========================================================================*/
bool
MaeBoard::remove_piece (const string& location)
{
   Squares square;

   if (!Move::translate_to_square (location, square))
      return false;

   return remove_piece (square);
}

/*=============================================================================
  Return TRUE if a piece was removed from SQUARE
  Precondition: Board::is_inside_board (square) = TRUE
  ===========================================================================*/
bool
MaeBoard::remove_piece (Squares square)
{
   if (this->board[square] == EMPTY_SQUARE)
      return false;

   // Remove piece in the bitboard representation
   this->pieces[board[square].player] ^= Util::to_bitboard[square];
   this->piece[board[square].player][board[square].piece] ^= Util::to_bitboard[square];
   this->all_pieces ^= Util::to_bitboard[square];

   Piece::Type piece = this->board[square].piece;
   Piece::Player player = this->board[square].player;

   this->board[square] = EMPTY_SQUARE;

   this->hash_key ^= this->zobrist[piece][player][square][0];
   this->hash_lock ^= this->zobrist[piece][player][square][1];

   return true;
}

/*=============================================================================
  Return NO_ERROR if MOVE was completely legal in THIS board; otherwise
  return the appropiate error code (KING_LEFT_IN_CHECK, OPPONENTS_TURN,
  WRONG_MOVEMENT)

  Precondition: MOVE.from () and MOVE.to () return values in range [0, SQUARES)
  Postcondition: The board has been updated to show the effect of MOVE.
  ===========================================================================*/
MaeBoard::Error
MaeBoard::make_move (Move& move, bool is_computer_move)
{
   Error move_error;

   // TEST IF THE MOVE IS CORRECT AND FILL IN INFORMATION IN MOVE
   if (this->board[move.from ()] == EMPTY_SQUARE)
      return NO_PIECE_IN_SQUARE;

   Squares start = move.from ();
   Squares end = move.to ();

   move.set_moving_piece (board[start].piece);
   if (board[end] != EMPTY_SQUARE)
      move.set_captured_piece (board[end].piece);

   // Assume that moves generated by the computer are pseudo-legal, so don't
   // bother making a verification.
   if (!is_computer_move)
      if ((move_error = can_move (move)) != NO_ERROR)
         return move_error;

   // Move has proven to be pseudo-legal so it may be tried.
   label_move (move);

   // Restoring information is kept in the stack GAME_HISTORY.
   save_restore_information (move);

   // MAKE THE ACTUAL MOVE
   Square initial = board[start];
   Square final = this->board[end];
   remove_piece (start);
   remove_piece (end);
   add_piece (end, initial.piece, initial.player);

   // Remove the pawn captured en-passant
   if (move.get_type () == Move::EN_PASSANT_CAPTURE)
   {
      uint offset = (this->is_whites_turn ? SIZE: -((int) SIZE));
      uint position = (Util::MSB_position (this->en_passant_capture_square) + offset);
      remove_piece (Squares (position));
   }

   // Check whether making the move leaves the king exposed to check
   int king_position = Util::MSB_position (this->piece[player][Piece::KING]);
   if (attacks_to (Squares (king_position), true /* include_king */))
   {
      remove_piece (end);
      add_piece (start, initial.piece, initial.player);
      if (final.piece != Piece::NULL_PIECE)
         add_piece (end, final.piece, final.player);

      // Restore the pawn captured en-passant
      if (move.get_type () == Move::EN_PASSANT_CAPTURE)
      {
         int offset = (this->is_whites_turn ? SIZE: -((int) SIZE));
         uint position = Util::MSB_position (en_passant_capture_square) + offset;
         add_piece (Squares (position), Piece::PAWN, opponent);
      }
      this->game_history.pop ();

      return KING_LEFT_IN_CHECK;
   }

   // Handle special moves and conditions
   handle_en_passant_move (move);
   handle_castling_privileges (move);
   handle_promotion_move (move);

   change_turn ();

   if (is_king_in_check ())
      move.set_type (Move::CHECK);

   // Record this board so we can detect threefold repetition conditions
   Record::board_key key = { this->hash_key, this->hash_lock };
   ushort times = 0;
   if (!this->position_counter.add_record (key, times) && times == 3)
      return DRAW_BY_REPETITION;

   // Apply 50-move rule here
   if (move.get_type () == Move::NORMAL_CAPTURE ||
       move.get_type () == Move::EN_PASSANT_CAPTURE ||
       move.get_moving_piece () == Piece::PAWN) {
      this->fifty_move_counter = 0;
   }
   else {
      this->fifty_move_counter++;
   }

   if (this->fifty_move_counter == 50)
      return DRAW_BY_REPETITION;

   return NO_ERROR;
}

/*=============================================================================
  Return TRUE if the last move was undone. An initial board should always
  return FALSE.

  Postcondition: Game status has been updated to reflect what it was before the
  last legal move made via make_move (). This includes current turn, en-passant
  capture possibilites, and castling privileges.
  ===========================================================================*/
bool
MaeBoard::undo_move ()
{
   if (this->game_history.empty ())
      return false;

   BoardStatus configuration = this->game_history.top ();
   Move move = configuration.get_move ();
   int size;

   Record::board_key key;
   key.hash_key = this->hash_key;
   key.hash_lock = this->hash_lock;

   if(!this->position_counter.decrease_record (key))
   {
      return false;
   }

   change_turn ();

   if(!remove_piece (move.to ()))
   {
      return false;
   }

   if (!add_piece (move.from (), move.get_moving_piece (), player))
   {
      return false;
   }

   switch (move.get_type ())
   {
      case Move::NORMAL_CAPTURE:
         if (!add_piece (move.to (), move.get_captured_piece (), opponent))
         {
            return false;
         }
         break;

      case Move::EN_PASSANT_CAPTURE:
         size = (this->is_whites_turn ? SIZE: -((int)SIZE));
         if (!add_piece (Squares (move.to () + size), Piece::PAWN, opponent))
         {
            return false;
         }
         break;

      case Move::CASTLE_KING_SIDE:
         if (!remove_piece (Squares (move.from () + 1)))
         {
            return false;
         }
         if (!add_piece (this->corner[player][KING_SIDE], Piece::ROOK, player))
         {
            return false;
         }
         this->is_castled_[player][KING_SIDE] = false;
         break;

      case Move::CASTLE_QUEEN_SIDE:
         if (!remove_piece (Squares (move.from () - 1)))
         {
            return false;
         }
         if (!add_piece (this->corner[player][QUEEN_SIDE], Piece::ROOK, player))
         {
            return false;
         }
         this->is_castled_[player][QUEEN_SIDE] = false;
         break;

      case Move::PROMOTION_MOVE:
         size = (this->is_whites_turn ? -((int)SIZE): SIZE);
         if (move.from () + size != move.to ())
            // There was a capture while doing the promotion
            if (!add_piece (move.to (), move.get_captured_piece (), opponent))
            {
               return false;
            }
         break;

      case Move::SIMPLE_MOVE:
         break;

      default:
         break;
   }

   // Restore the previous configuration completely
   this->can_do_castle[player][KING_SIDE] = configuration.get_king_castle ();
   this->can_do_castle[player][QUEEN_SIDE] = configuration.get_queen_castle ();
   this->en_passant_capture_square = configuration.get_en_passant ();
   this->hash_key = configuration.get_hash_key ();
   this->hash_lock = configuration.get_hash_lock ();

   this->game_history.pop ();

   return true;
}

/*=============================================================================
  Return NO_ERROR if the given piece can actually move from MOVE.FROM () to
  MOVE.TO () according to its own movement rules and the current board status.
  Otherwise, return the appropiate error code (OPPONENTS_TURN, WRONG_MOVEMENT)

  Note: This test is not complete, since it does not check whether the move
  leaves the king in check.

  Precondition: MOVE.FROM () and MOVE.TO () are valid squares on the board.
  (i.e. they're in the range 0 to Board::SQUARES)
  ===========================================================================*/
MaeBoard::Error
MaeBoard::can_move (const Move& move) const
{
   bitboard valid_moves;
   ushort start = move.from ();

   // Is the player trying to move his opponent's pieces?
   if (this->player != this->board[start].player)
      return OPPONENTS_TURN;

   valid_moves =
      this->chessmen[move.get_moving_piece ()]->get_moves (start, this->player, this);

   // Is MOVE.TO () included in the set of valid moves from MOVE.FROM () ?
   if (Util::to_bitboard[move.to ()] & valid_moves)
      return NO_ERROR;

   return WRONG_MOVEMENT;
}

/*=============================================================================
  Label MOVE as a normal capture, en-passant capture, promotion, etc.,
  according to the current board.

  Precondition: MOVE has been proven to be pseudo-legal in the current board
  configuration.
  ===========================================================================*/
void
MaeBoard::label_move (Move& move) const
{
   ushort start = move.from ();
   ushort end = move.to ();
   Piece::Type piece = move.get_moving_piece ();

   if (Util::to_bitboard[end] & ~all_pieces) // Apparently simple moves
   {
      move.set_type (Move::SIMPLE_MOVE);

      if ((Util::to_bitboard[end] & this->en_passant_capture_square) &&
          piece == Piece::PAWN)
      {
         move.set_type (Move::EN_PASSANT_CAPTURE);
      }
      else if (piece == Piece::KING)
      {
         if (start == this->original_king_position[player] && (end == start + 2))
         {
            move.set_type (Move::CASTLE_KING_SIDE);
         }
         else if (start == this->original_king_position[player] && (end + 2 == start))
         {
            move.set_type (Move::CASTLE_QUEEN_SIDE);
         }
      }
   }
   else if (Util::to_bitboard[end] & pieces[opponent]) { // Capture moves
      move.set_type (Move::NORMAL_CAPTURE);
   }
   else
   {
      move.set_type (Move::NULL_MOVE);
   }

   // Promotion moves can happen both as simple moves and as capture moves
   if ((Util::to_bitboard[end] & this->eighth_rank[player]) &&
       piece == Piece::PAWN)
   {
      move.set_type (Move::PROMOTION_MOVE);
   }
}

/*=============================================================================
  Get a bitboard containing all enemy pieces that atack LOCATION
  ===========================================================================*/
bitboard
MaeBoard::attacks_to (Squares location, bool include_king) const
{
   bitboard attackers = 0;
   bitboard pawn_attacks;
   Pawn* pawn = (Pawn*) this->chessmen[Piece::PAWN];
   Piece::Type last_piece = (include_king ? Piece::KING : Piece::QUEEN);

   // Put a piece of TYPE in LOCATION and compute all its pseudo-moves.
   // If there are any opponent pieces in the resulting squares, then there is
   // at least one attack to LOCATION
   for (Piece::Type type = Piece::KNIGHT; type <= last_piece; ++type)
   {
      attackers |=
         this->chessmen[type]->get_moves (location, this->player, this) &
         this->piece[opponent][type];
   }
   pawn_attacks = (pawn->get_capture_move (location, this->player, Piece::EAST) |
                   pawn->get_capture_move (location, this->player, Piece::WEST));

   attackers |= (pawn_attacks & this->piece[opponent][Piece::PAWN]);

   return attackers;
}

/*=============================================================================
  Get a bitboard containing all enemy pieces that atack LOCATION and whose
  value is less than that of a piece of type TYPE
  ===========================================================================*/
bitboard
MaeBoard::threats_to (Squares location, Piece::Type type) const
{
   bitboard attackers = 0;
   bitboard pawn_attackers;
   Pawn* pawn = (Pawn*) chessmen[Piece::PAWN];

   for (Piece::Type attacked = type; attacked > Piece::PAWN; --attacked)
   {
      attackers |=
         this->chessmen[type]->get_moves (location, this->player, this) &
         this->piece[opponent][type];
   }
   pawn_attackers = (pawn->get_capture_move (location, this->player, Piece::EAST) |
                     pawn->get_capture_move (location, this->player, Piece::WEST));

   attackers |= (pawn_attackers & this->piece[opponent][Piece::PAWN]);

   return attackers;
}

/*=============================================================================
  Return TRUE if the king is in check in THIS board.
  ===========================================================================*/
bool
MaeBoard::is_king_in_check () const
{
   uint king_location = Util::MSB_position (this->piece[player][Piece::KING]);

   // The second argument is set to FALSE since one invariant of this class is
   // that no king can be in check by the other (such thing is illegal)
   if (attacks_to (Squares (king_location), false))
      return true;

   return false;
}

/*=============================================================================
  If a pawn has just moved, should we turn on the en-passant flag ?

  Two conditions are necessary for this to happen:
  (1) the pawn made a two-square move, and
  (2) there are enemy pawns to, at least, one of its sides.
  ============================================================================*/
void
MaeBoard::handle_en_passant_move (const Move& move)
{
   if (move.get_moving_piece () != Piece::PAWN)
   {
      if (this->en_passant_capture_square)
      {
         int square = Util::MSB_position (this->en_passant_capture_square);
         this->hash_key ^= this->en_passant_key[square];
         this->hash_lock ^= this->en_passant_key[square];
      }
      this->en_passant_capture_square = 0;
      return;
   }

   Pawn* pawn = (Pawn*) this->chessmen[Piece::PAWN];
   int start = (int) move.from ();
   int end = (int) move.to ();

   if (this->en_passant_capture_square)
   {
      int square = Util::MSB_position (this->en_passant_capture_square);
      hash_key ^= this->en_passant_key[square];
      hash_lock ^= this->en_passant_key[square];
   }
   this->en_passant_capture_square = 0;

   // Turn the en-passant flag if necessary
   if ((pawn->get_side_moves (end, player) & this->piece[opponent][Piece::PAWN]) &&
       (abs (end - start) == SIZE + SIZE))
   {
      int min = (start < end ? start : end);
      int max = (start > end ? start : end);
      int row = (this->is_whites_turn ? min : max);
      int size = (this->is_whites_turn ? SIZE : -((int)SIZE));

      this->en_passant_capture_square = Util::to_bitboard[row + size];
      int square = Util::MSB_position (this->en_passant_capture_square);
      this->hash_key ^= this->en_passant_key[square];
      this->hash_lock ^= this->en_passant_key[square];
   }
}

/*=============================================================================
  Set castling flags to FALSE if either the king has moved or any other piece
  has moved to the corners of the board (possibly a rook has moved or has been
  captured by the enemy)
  ===========================================================================*/
void
MaeBoard::handle_castling_privileges (const Move& move)
{
   ushort start = move.from ();
   ushort end = move.to ();

   // Check whether castling is still possible after this move is made
   if (move.get_moving_piece () == Piece::KING)
   {
      // If this move was a castle, move the rook next to the king
      if (move.get_type () == Move::CASTLE_KING_SIDE)
      {
         add_piece (Squares (end-1), board[end+1].piece, board[end+1].player);
         remove_piece (Squares (end + 1));
         this->is_castled_[player][KING_SIDE] = true;
      }
      else if (move.get_type () == Move::CASTLE_QUEEN_SIDE)
      {
         add_piece (Squares (end+1), board[end-2].piece, board[end-2].player);
         remove_piece (Squares (end - 2));
         this->is_castled_[player][QUEEN_SIDE] = true;
      }
      this->can_do_castle[player][KING_SIDE] = false;
      this->can_do_castle[player][QUEEN_SIDE] = false;

      // Update hash key and hash lock
      this->hash_key ^= castle_key[player][KING_SIDE];
      this->hash_key ^= castle_key[player][QUEEN_SIDE];

      this->hash_lock ^= castle_key[player][KING_SIDE];
      this->hash_lock ^= castle_key[player][QUEEN_SIDE];
   }
   // If anything moves from or to any of the board corners, castling is lost.
   else if (start == this->corner[player][KING_SIDE])
   {
      if (this->can_do_castle[player][KING_SIDE])
      {
         // Update hash key and hash lock
         this->hash_key ^= castle_key[player][KING_SIDE];
         this->hash_lock ^= castle_key[player][KING_SIDE];
      }
      this->can_do_castle[player][KING_SIDE] = false;
   }
   else if (start == this->corner[player][QUEEN_SIDE])
   {
      if (this->can_do_castle[player][QUEEN_SIDE])
      {
         // Update hash key and hash lock
         this->hash_key ^= this->castle_key[player][QUEEN_SIDE];
         this->hash_lock ^= this->castle_key[player][QUEEN_SIDE];
      }
      this->can_do_castle[player][QUEEN_SIDE] = false;
   }
   else if (end == this->corner[opponent][KING_SIDE])
   {
      if (this->can_do_castle[opponent][QUEEN_SIDE])
      {
         // Update hash key and hash lock
         this->hash_key ^= this->castle_key[opponent][KING_SIDE];
         this->hash_lock ^= this->castle_key[opponent][KING_SIDE];
      }
      this->can_do_castle[opponent][KING_SIDE] = false;
   }
   else if (end == this->corner[opponent][QUEEN_SIDE])
   {
      if (this->can_do_castle[opponent][QUEEN_SIDE])
      {
         // Update hash key and hash lock
         this->hash_key ^= this->castle_key[opponent][QUEEN_SIDE];
         this->hash_lock ^= this->castle_key[opponent][QUEEN_SIDE];
      }
      this->can_do_castle[opponent][QUEEN_SIDE] = false;
   }
}

/*=============================================================================
  Exchange a pawn to any other piece (except for a king or another pawn) if it
  has hit the eighth rank. Usually the exchange is done to a queen, but the
  player should be able to choose the piece.
  ===========================================================================*/
void
MaeBoard::handle_promotion_move (const Move& move)
{
   if (move.get_type () == Move::PROMOTION_MOVE)
   {
      remove_piece (move.to ());
      add_piece (move.to (), Piece::QUEEN, this->player);
   }
}

/*=============================================================================
  Create instances of Piece (Knight, Bishop, Queen, etc.) to aid in move
  generation and checking whether moves are valid.
  ===========================================================================*/
void
MaeBoard::load_chessmen ()
{
   this->chessmen[Piece::ROOK] = new Rook ();
   this->chessmen[Piece::KNIGHT] = new Knight ();
   this->chessmen[Piece::BISHOP] = new Bishop ();
   this->chessmen[Piece::QUEEN] = new Queen ();
   this->chessmen[Piece::KING] = new King ();
   this->chessmen[Piece::PAWN] = new Pawn ();
}

/*=============================================================================
  Initialize all the data structures needed to make computations in make_move,
  can_move, and label_move.
  ===========================================================================*/
void
MaeBoard::load_support_data ()
{
   load_chessmen ();

   // Bitboards representing the eigth rank for each player
   this->eighth_rank[Piece::WHITE] = this->eighth_rank[Piece::BLACK] = 0;
   this->eighth_rank[Piece::WHITE] |=
       Util::to_bitboard[a8] |
       Util::to_bitboard[b8] |
       Util::to_bitboard[c8] |
       Util::to_bitboard[d8] |
       Util::to_bitboard[e8] |
       Util::to_bitboard[f8] |
       Util::to_bitboard[g8] |
       Util::to_bitboard[h8];

   this->eighth_rank[Piece::BLACK] |=
       Util::to_bitboard[a1] |
       Util::to_bitboard[b1] |
       Util::to_bitboard[c1] |
       Util::to_bitboard[d1] |
       Util::to_bitboard[e1] |
       Util::to_bitboard[f1] |
       Util::to_bitboard[g1] |
       Util::to_bitboard[h1];

   // Corners for each player
   this->corner[Piece::WHITE][KING_SIDE] = h1;
   this->corner[Piece::WHITE][QUEEN_SIDE] = a1;
   this->corner[Piece::BLACK][KING_SIDE] = h8;
   this->corner[Piece::BLACK][QUEEN_SIDE] = a8;

   this->original_king_position[Piece::WHITE] = e1;
   this->original_king_position[Piece::BLACK] = e8;
}

/*=============================================================================
  Save all variables needed to restore a previous board configuration by
  undoing one move.
  ============================================================================*/
void
MaeBoard::save_restore_information (const Move& move)
{
   BoardStatus restore_information (
       move, this->en_passant_capture_square,
       this->can_do_castle[player][KING_SIDE], this->can_do_castle[player][QUEEN_SIDE],
       this->hash_key, this->hash_lock);

   this->game_history.push (restore_information);
}

/*=============================================================================
  Update the variables reflecting the current turn
  ============================================================================*/
void
MaeBoard::change_turn ()
{
   this->is_whites_turn = !this->is_whites_turn;
   this->opponent = this->player;
   this->player = (this->opponent == Piece::WHITE ? Piece::BLACK : Piece::WHITE);

   // Update hash keys to reflect the turn
   this->hash_key ^= this->turn_key;
   this->hash_lock ^= this->turn_key;
}

// ACCESSORS

/*=============================================================================
  Return all pseudo-legal moves PIECE can make from SQUARE in the current
  board, assuming it is PLAYER's turn to move.
  ===========================================================================*/
bitboard
MaeBoard::get_moves (Piece::Type piece, Squares square) const
{
   // Every piece implements their own moves
   return this->chessmen[piece]->get_moves (square, this->player, this);
}

bitboard
MaeBoard::get_all_pieces () const
{
   return this->all_pieces;
}

bitboard
MaeBoard::get_pieces (Piece::Player player) const
{
   return this->pieces[player];
}

bitboard
MaeBoard::get_pieces (Piece::Player player, Piece::Type piece) const
{
   return this->piece[player][piece];
}

ullong
MaeBoard::get_hash_key () const
{
  return this->hash_key;
}

ullong
MaeBoard::get_hash_lock () const
{
   return this->hash_lock;
}

bool
MaeBoard::is_en_passant_on () const
{
   return this->en_passant_capture_square != 0;
}

bool
MaeBoard::can_castle (Piece::Player player, CastleSide side) const
{
   return this->can_do_castle[player][side];
}

bool
MaeBoard::is_castled (Piece::Player player, CastleSide side) const
{
   return this->is_castled_[player][side];
}

bitboard
MaeBoard::get_en_passant_square () const
{
   return this->en_passant_capture_square;
}

Board::Squares
MaeBoard::get_initial_king_square (Piece::Player player) const
{
   return this->original_king_position[player];
}

Piece::Player
MaeBoard::get_piece_color (Squares square) const
{
   return this->board[square].player;
}

Piece::Player
MaeBoard::get_turn () const
{
   return this->player;
}

Piece::Type
MaeBoard::get_piece (Squares square) const
{
   return this->board[square].piece;
}

uint
MaeBoard::get_move_number () const
{
   uint game_moves = this->game_history.size ();

   if (game_moves % 2 == 1)
      game_moves++;

   return game_moves >> 1; // (game_moves / 2)
}

/*=============================================================================
  Return the number of times THIS board configuration has been seen during the
  present game.
  ===========================================================================*/
ushort
MaeBoard::get_repetition_count () const
{
   Record::board_key key = { this->hash_key, this->hash_lock };
   return this->position_counter.get_repetitions (key);
}

// MUTATORS
void
MaeBoard::set_game_status (GameStatus status)
{
   this->game_status = status;
}

void
MaeBoard::set_en_passant (Squares en_passant_capture_square)
{
   this->en_passant_capture_square =
      Util::to_bitboard[en_passant_capture_square];
}

void
MaeBoard::set_turn (Piece::Player player)
{
   this->is_whites_turn = (player == Piece::WHITE ? true : false);
   this->player = player;
   this->opponent = (this->player == Piece::WHITE ? Piece::BLACK : Piece::WHITE);

   // a hash key for the turn is added to the board key when it's black's turn
   if (!this->is_whites_turn)
   {
      this->hash_key ^= this->turn_key;
      this->hash_lock ^= this->turn_key;
   }
}

void
MaeBoard::set_castling_privilege (
    Piece::Player player, CastleSide side, bool value)
{
   this->can_do_castle[player][side] = value;

   // Whenever a side can no longer castle, a hash key is added to the board key
   if (this->can_do_castle[player][side] == false)
   {
      // Update hash key and hash lock
      this->hash_key ^= this->castle_key[player][side];
      this->hash_lock ^= this->castle_key[player][side];
   }
}
