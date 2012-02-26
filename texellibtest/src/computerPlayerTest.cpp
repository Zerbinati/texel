/*
 * computerPlayerTest.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "computerPlayerTest.hpp"

#include "cute.h"

#if 0
    /**
     * Test of getCommand method, of class ComputerPlayer.
     */
    public void testGetCommand() {
        ArrayList<Position> nullHist = new ArrayList<Position>();

        Position pos = TextIO::readFEN("7k/5Q2/p5K1/8/8/8/8/8 b - - 99 80");
        ComputerPlayer cp = new ComputerPlayer();
        cp.maxDepth = 1;
        cp.maxTimeMillis = -1;
        cp.verbose = false;
        std::string result = cp.getCommand(pos, false, nullHist);
        ASSERT_EQUAL("a5", result);     // Only one legal move

        pos = TextIO::readFEN("7k/5Q2/p5K1/8/8/8/8/8 b - - 100 80");
        result = cp.getCommand(pos, false, nullHist);
        ASSERT_EQUAL("draw 50", result);    // Should claim draw without making a move
        
        pos = TextIO::readFEN("3k4/1R6/R7/8/8/8/8/1K6 w - - 100 80");
        result = cp.getCommand(pos, false, nullHist);
        ASSERT_EQUAL("Ra8#", result);       // Can claim draw, but should not
        
        pos = TextIO::readFEN("8/1R5k/R7/8/8/8/B7/1K6 b - - 99 80");
        result = cp.getCommand(pos, false, nullHist);
        ASSERT_EQUAL("draw 50 Kh8", result);     // Should claim draw by 50-move rule

        // Only one possible move. Should realize that draw claim is possible, but very bad
        pos = TextIO::readFEN("6Nk/8/5K1R/q7/q7/q7/8/8 b - - 100 80");
        result = cp.getCommand(pos, false, nullHist);
        ASSERT_EQUAL("Kxg8", result);
    }

    /**
     * Test of draw by repetition, of class ComputerPlayer.
     */
    public void testDrawRep() {
        Game game = new Game(new HumanPlayer(), new HumanPlayer());
        ComputerPlayer cp = new ComputerPlayer();
        cp.maxDepth = 3;
        cp.maxTimeMillis = -1;
        cp.verbose = false;
        game.processString("setpos 7k/5RR1/8/8/8/8/q3q3/2K5 w - - 0 1");
        game.processString("Rh7");
        game.processString("Kg8");
        game.processString("Rhg7");
        std::string result = cp.getCommand(new Position(game.pos), false, game.getHistory());
        ASSERT_EQUAL("Kh8", result); // Not valid to claim draw here
        game.processString("Kh8");
        game.processString("Rh7");
        game.processString("Kg8");
        game.processString("Rhg7");
        result = cp.getCommand(new Position(game.pos), false, game.getHistory());
        ASSERT_EQUAL("draw rep Kh8", result);   // Can't win, but can claim draw.
        
        game.processString("setpos 7k/R7/1R6/8/8/8/8/K7 w - - 0 1");
        game.processString("Ra8");
        game.processString("Kh7");
        result = cp.getCommand(new Position(game.pos), false, game.getHistory());
        ASSERT_EQUAL("Ra7+", result);       // Ra7 is mate-in-two
        game.processString("Ra7");
        game.processString("Kh8");
        game.processString("Ra8");
        game.processString("Kh7");
        result = cp.getCommand(new Position(game.pos), false, game.getHistory());
        ASSERT(!result.equals("Ra7+")); // Ra7 now leads to a draw by repetition
    }
#endif


cute::suite
ComputerPlayerTest::getSuite() const {
    cute::suite s;
//    s.push_back(CUTE());
#if 0
    s.push_back(CUTE(testGetCommand));
    s.push_back(CUTE(testDrawRep));
#endif
    return s;
}
