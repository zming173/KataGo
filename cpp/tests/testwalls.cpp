#include "../tests/tests.h"

#include "../dataio/sgf.h"
#include "../neuralnet/nninputs.h"

using namespace std;
using namespace TestCommon;

//Tests for interior walls on a 9x9 board: the central 3x3 block D4~F6 is turned into C_WALL.

static const char* const WALL_POINTS[9] = {"D4","E4","F4","D5","E5","F5","D6","E6","F6"};

static Loc L(const char* s, const Board& board) {
  return Location::ofString(s,board);
}

//Empty 9x9 board with walls on D4~F6
static Board makeWallBoard() {
  Board board(9,9);
  for(const char* s : WALL_POINTS) {
    bool suc = board.setWall(L(s,board));
    testAssert(suc);
  }
  board.checkConsistency();
  return board;
}

static void play(Board& board, const char* s, Player pla) {
  Loc loc = L(s,board);
  testAssert(board.isLegal(loc,pla,false));
  board.playMoveAssumeLegal(loc,pla);
}

void Tests::runWallTests() {
  cout << "Running wall tests" << endl;

  //setWall basics -------------------------------------------------------------------
  {
    Board board = makeWallBoard();
    for(const char* s : WALL_POINTS)
      testAssert(board.colors[L(s,board)] == C_WALL);
    testAssert(board.numStonesOnBoard() == 0);
    //Walls do not count as stones, so the board is still "empty" for handicap commands
    testAssert(board.isEmpty());

    //Cannot wall a point twice, and cannot add walls once a stone is on the board
    testAssert(!board.setWall(L("E5",board)));
    play(board,"A1",P_BLACK);
    testAssert(!board.setWall(L("B2",board)));
    board.checkConsistency();
  }

  //Hashing --------------------------------------------------------------------------
  {
    Board plain(9,9);
    Board walled = makeWallBoard();
    //A wall board must not hash the same as the plain empty board
    testAssert(plain.pos_hash != walled.pos_hash);

    //Hash built incrementally by setWall must equal the hash recomputed from colors
    Hash128 before = walled.pos_hash;
    walled.checkConsistency();
    testAssert(walled.pos_hash == before);

    //Order in which walls are placed must not matter
    Board reversed(9,9);
    for(int i = 8; i >= 0; i--)
      testAssert(reversed.setWall(L(WALL_POINTS[i],reversed)));
    testAssert(reversed.pos_hash == walled.pos_hash);
  }

  //Scenario 1: black C5 next to the wall has 3 liberties (B5, C4, C6) ----------------
  {
    Board board = makeWallBoard();
    play(board,"C5",P_BLACK);
    testAssert(board.getNumLiberties(L("C5",board)) == 3);
    board.checkConsistency();
  }

  //Scenario 2: black C3, not touching a wall, still has 4 liberties ------------------
  {
    Board board = makeWallBoard();
    play(board,"C3",P_BLACK);
    testAssert(board.getNumLiberties(L("C3",board)) == 4);
    board.checkConsistency();
  }

  //Scenario 3: nobody can play on a wall ----------------------------------------------
  {
    Board board = makeWallBoard();
    for(const char* s : WALL_POINTS) {
      Loc loc = L(s,board);
      testAssert(!board.isLegal(loc,P_BLACK,false));
      testAssert(!board.isLegal(loc,P_WHITE,false));
      testAssert(!board.isLegal(loc,P_BLACK,true));
      testAssert(!board.isLegal(loc,P_WHITE,true));
      testAssert(!board.isOnBoard(loc));
    }
    board.checkConsistency();
  }

  //Scenario 4: black C5 is captured by white B5, C4, C6 ------------------------------
  {
    Board board = makeWallBoard();
    play(board,"C5",P_BLACK);
    play(board,"B5",P_WHITE);
    play(board,"C4",P_WHITE);
    testAssert(board.colors[L("C5",board)] == C_BLACK);
    testAssert(board.getNumLiberties(L("C5",board)) == 1);
    play(board,"C6",P_WHITE);
    testAssert(board.colors[L("C5",board)] == C_EMPTY);
    testAssert(board.numBlackCaptures == 1);
    board.checkConsistency();
  }

  //Scenario 5: white G3 H3 J3 G7 H7 J7 enclose G4~J6 using the wall as a border -------
  {
    Board board = makeWallBoard();
    const char* whites[6] = {"G3","H3","J3","G7","H7","J7"};
    for(const char* s : whites)
      play(board,s,P_WHITE);

    Color area[Board::MAX_ARR_SIZE];
    board.calculateArea(area,true,true,true,false);
    const char* enclosed[9] = {"G4","H4","J4","G5","H5","J5","G6","H6","J6"};
    for(const char* s : enclosed)
      testAssert(area[L(s,board)] == C_WHITE);
    for(const char* s : WALL_POINTS)
      testAssert(area[L(s,board)] == C_EMPTY);
    board.checkConsistency();
  }

  //Scenario 6: a 16-stone white ring around the wall dies when black fills its 20 outside liberties
  {
    Board board = makeWallBoard();
    const char* ring[16] = {
      "C3","D3","E3","F3","G3",
      "C7","D7","E7","F7","G7",
      "C4","C5","C6",
      "G4","G5","G6",
    };
    for(const char* s : ring)
      play(board,s,P_WHITE);
    testAssert(board.getNumLiberties(L("C3",board)) == 20);

    const char* outside[20] = {
      "B3","B4","B5","B6","B7",
      "H3","H4","H5","H6","H7",
      "C2","D2","E2","F2","G2",
      "C8","D8","E8","F8","G8",
    };
    for(int i = 0; i<19; i++)
      play(board,outside[i],P_BLACK);
    testAssert(board.getNumLiberties(L("C3",board)) == 1);
    for(const char* s : ring)
      testAssert(board.colors[L(s,board)] == C_WHITE);

    play(board,outside[19],P_BLACK);
    for(const char* s : ring)
      testAssert(board.colors[L(s,board)] == C_EMPTY);
    testAssert(board.numWhiteCaptures == 16);
    board.checkConsistency();

    //The wall never belongs to anyone, under area scoring (5-1)...
    Color area[Board::MAX_ARR_SIZE];
    board.calculateArea(area,true,true,true,false);
    for(const char* s : WALL_POINTS)
      testAssert(area[L(s,board)] == C_EMPTY);
    for(const char* s : ring)
      testAssert(area[L(s,board)] == C_BLACK);

    //...and under the independent-life path used by territory scoring and taxed rules (5-2)
    for(int keepStones = 0; keepStones <= 1; keepStones++) {
      for(int keepTerritories = 0; keepTerritories <= 1; keepTerritories++) {
        Color ilArea[Board::MAX_ARR_SIZE];
        int whiteMinusBlackIndependentLifeRegionCount = 0;
        board.calculateIndependentLifeArea(
          ilArea,whiteMinusBlackIndependentLifeRegionCount,
          keepTerritories != 0, keepStones != 0, false, false
        );
        for(const char* s : WALL_POINTS)
          testAssert(ilArea[L(s,board)] == C_EMPTY);
      }
    }
  }

  //Scenario 7: the wall counts as an edge for eye detection --------------------------
  {
    Board board = makeWallBoard();
    play(board,"B5",P_BLACK);
    play(board,"C4",P_BLACK);
    play(board,"C6",P_BLACK);
    testAssert(board.isSimpleEye(L("C5",board),P_BLACK));
    play(board,"B4",P_WHITE);
    testAssert(!board.isSimpleEye(L("C5",board),P_BLACK));
    board.checkConsistency();
  }

  //Symmetry (item 6): walls survive getSymBoard in all 8 symmetries --------------------
  {
    Board board = makeWallBoard();
    play(board,"C3",P_BLACK);
    play(board,"G7",P_WHITE);
    play(board,"B5",P_BLACK);
    for(int symmetry = 0; symmetry < SymmetryHelpers::NUM_SYMMETRIES; symmetry++) {
      Board symBoard = SymmetryHelpers::getSymBoard(board,symmetry);
      symBoard.checkConsistency();
      //The central 3x3 maps onto itself under every symmetry
      for(const char* s : WALL_POINTS)
        testAssert(symBoard.colors[L(s,symBoard)] == C_WALL);
      testAssert(symBoard.numStonesOnBoard() == board.numStonesOnBoard());
      for(int y = 0; y<9; y++) {
        for(int x = 0; x<9; x++) {
          Loc loc = Location::getLoc(x,y,9);
          Loc symLoc = SymmetryHelpers::getSymLoc(x,y,board,symmetry);
          testAssert(symBoard.colors[symLoc] == board.colors[loc]);
        }
      }
      if(symmetry == 0)
        testAssert(symBoard.pos_hash == board.pos_hash);
    }
  }

  //Undo/rules change path: BoardHistory copies the initial board including walls -------
  {
    Board board = makeWallBoard();
    BoardHistory hist(board,P_BLACK,Rules::getTrompTaylorish(),0,BoardHistoryModes());
    hist.makeBoardMoveAssumeLegal(board,L("C5",board),P_BLACK,NULL);
    for(const char* s : WALL_POINTS) {
      testAssert(hist.initialBoard.colors[L(s,board)] == C_WALL);
      testAssert(hist.getRecentBoard(0).colors[L(s,board)] == C_WALL);
    }
    board.checkConsistency();
  }

  //loadsgf path (item 8-4): walls are placed before the sgf's initial stones -------------
  {
    vector<string> walls(WALL_POINTS, WALL_POINTS+9);
    std::unique_ptr<CompactSgf> sgf = CompactSgf::parse("(;FF[4]GM[1]SZ[9]AB[cc][gg];W[ee])");
    Board board;
    Player nextPla;
    BoardHistory hist;
    sgf->setupInitialBoardAndHist(Rules::getTrompTaylorish(),board,nextPla,hist,BoardHistoryModes(),walls);
    for(const char* s : WALL_POINTS)
      testAssert(board.colors[L(s,board)] == C_WALL);
    testAssert(board.numStonesOnBoard() == 2);
    board.checkConsistency();

    //Without the extra argument behavior is unchanged: no walls
    Board plainBoard;
    sgf->setupInitialBoardAndHist(Rules::getTrompTaylorish(),plainBoard,nextPla,hist,BoardHistoryModes());
    for(const char* s : WALL_POINTS)
      testAssert(plainBoard.colors[L(s,plainBoard)] != C_WALL);

    //An sgf with a setup stone on a wall must be rejected
    std::unique_ptr<CompactSgf> badSgf = CompactSgf::parse("(;FF[4]GM[1]SZ[9]AB[ee])");
    bool threw = false;
    try {
      badSgf->setupInitialBoardAndHist(Rules::getTrompTaylorish(),board,nextPla,hist,BoardHistoryModes(),walls);
    }
    catch(const StringError&) {
      threw = true;
    }
    testAssert(threw);
  }

  cout << "Wall tests passed" << endl;
}
