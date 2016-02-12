#include <iostream>
#include <string>
#include <memory>

#include "MaeBoard.hpp"
#include "Timer.hpp"
#include "CommandReader.hpp"
#include "CommandExecuter.hpp"
#include "AlphaBetaSearch.hpp"
#include "SimpleMoveGenerator.hpp"
#include "SimpleEvaluator.hpp"

using std::unique_ptr;
using std::cerr;
using std::endl;

using game_rules::Board;
using game_rules::MaeBoard;
using game_rules::Piece;
using game_rules::Move;

using game_engine::MoveGenerator;
using game_engine::SimpleMoveGenerator;
using game_engine::PositionEvaluator;
using game_engine::SimpleEvaluator;
using game_engine::IChessEngine;
using game_engine::AlphaBetaSearch;

using game_ui::Command;
using game_ui::CommandReader;
using game_ui::CommandExecuter;

using diagnostics::Timer;

int
main ()
{
  bool auto_play = false;
  bool xboard_mode = false;

  unique_ptr<Board> board(new MaeBoard);
  unique_ptr<PositionEvaluator> evaluator(new SimpleEvaluator);
  unique_ptr<MoveGenerator> generator(new SimpleMoveGenerator);
  unique_ptr<IChessEngine> search_engine(new AlphaBetaSearch (evaluator.get(), generator.get()));
  unique_ptr<Timer> timer(new Timer);

  Command command;
  unique_ptr<CommandReader> command_reader(new CommandReader);
  unique_ptr<CommandExecuter> command_executer(
      new CommandExecuter(board.get(), search_engine.get(), timer.get()));

  cerr << (*board) << endl;
  cerr << (board->get_turn () == Piece::WHITE ?
           "[White's turn]: " : "[Black's turn]: ");

  command = command_reader->get_command ();
  while (!command.is_quit ())
  {
    if (command.is_move ())
    {
      Move move (command.get_notation ());

      Board::Error error = board->make_move (move, false);
      if (error != Board::NO_ERROR)
      {
        switch (error)
        {
          case Board::GAME_FINISHED:
            cerr << "Game finished" << endl;
            break;
          case Board::NO_PIECE_IN_SQUARE:
            cerr << "No piece in square" << endl;
            break;
          case Board::OPPONENTS_TURN:
            cerr << "Opponents turn" << endl;
            break;
          case Board::WRONG_MOVEMENT:
            cerr << "Wrong movement" << endl;
            break;
          case Board::KING_LEFT_IN_CHECK:
            cerr << "King left in check" << endl;
            break;
          default:
            cerr << "Another error" << endl;
            break;
        }
        cerr << "[Your move is illegal, please try again.]"
             << endl;
      }
      else if (auto_play)
      {
        Command response (Command::THINK);
        command_executer->execute (response);
      }
    }
    else
    {
      if (command.get_value () == Command::XBOARD_MODE)
      {
        xboard_mode = true;
        auto_play = true;
        command_executer->execute (command);
      }
      else if (command.get_value () == Command::COMPUTER_PLAY)
      {
        auto_play = !auto_play;
        if (auto_play)
        {
          Command response (Command::THINK);
          command_executer->execute (response);
        }
      }
      else
        command_executer->execute (command);
    }

    if (!xboard_mode)
    {
      cerr << (*board) << endl;
      cerr << (board->get_turn () == Piece::WHITE ?
               "[White's turn]: " : "[Black's turn]: ");
    }

    command = command_reader->get_command ();
  }

  return 0;
}