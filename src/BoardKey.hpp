#ifndef BOARD_KEY_H
#define BOARD_KEY_H

#include "Util.hpp"

namespace game_rules
{
   struct BoardKey
   {
      ullong hash_key;
      ullong hash_lock;
   };
}

#endif // BOARD_KEY_H
