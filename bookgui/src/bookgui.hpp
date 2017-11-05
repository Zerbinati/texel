/*
    Texel - A UCI chess engine.
    Copyright (C) 2016  Peter Österlund, peterosterlund2@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * bookgui.hpp
 *
 *  Created on: Apr 2, 2016
 *      Author: petero
 */

#ifndef BOOKGUI_HPP_
#define BOOKGUI_HPP_

#include <gtkmm.h>
#include "bookbuildcontrol.hpp"
#include "gametree.hpp"

class BookGui : public BookBuildControl::ChangeListener {
public:
    BookGui(Glib::RefPtr<Gtk::Application> app);
    BookGui(const BookGui& other) = delete;
    BookGui& operator=(const BookGui& other) = delete;

    void run();

private:
    void getWidgets();
    void connectSignals();

    /** Called by book builder worker thread when book state has changed. */
    void notify() override;

    /** Called by GUI thread when notify() is called by worker thread. */
    void bookStateChanged();

    void updateBoardAndTree();
    void setPosition(const Position& newPos, const std::vector<Move>& movesBefore,
                     const std::vector<Move>& movesAfter);
    void updateQueueView();
    void updatePVView();
    void updatePGNView();
    void updatePGNSelection();
    void updateEnabledState();

    void newBook();
    void openBookFile();
    void saveBookFile();
    bool saveBookFileAs(); // Return true if book was saved
    void quit();
    bool deleteEvent(_GdkEventAny* e);
    bool askSaveIfDirty(); // Return false if user canceled

    void startSearch();
    void softStopSearch();
    void hardStopSearch();

    void threadsValueChanged();
    void compTimeChanged();
    void depthCostChanged();
    void ownPathErrCostChanged();
    void otherPathErrCostChanged();
    void pgnMaxPlyChanged();

    void setFocus();
    void getFocus();
    void clearFocus();

    void importPgn();
    void addToPgn();
    void applyPgn();
    void clearPgn();
    bool pgnButtonPressed(GdkEventButton* event);

    void posGoBack();
    void posGoForward();

    void nextGeneration();
    void toggleAnalyzeMode();


    Glib::RefPtr<Gtk::Application> app;
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::Window* mainWindow;

    Glib::Dispatcher dispatcher;

    // Menu items
    Gtk::MenuItem* newItem = nullptr;
    Gtk::MenuItem* openItem = nullptr;
    Gtk::MenuItem* saveItem = nullptr;
    Gtk::MenuItem* saveAsItem = nullptr;
    Gtk::MenuItem* quitItem = nullptr;

    // Settings widgets
    Gtk::SpinButton* threads = nullptr;
    Gtk::SpinButton* compTime = nullptr;
    Gtk::SpinButton* depthCost = nullptr;
    Gtk::SpinButton* ownPathErrCost = nullptr;
    Gtk::SpinButton* otherPathErrCost = nullptr;
    Gtk::SpinButton* pgnMaxPly = nullptr;

    // Start/stop buttons
    Gtk::Button* startButton = nullptr;
    Gtk::Button* softStopButton = nullptr;
    Gtk::Button* hardStopButton = nullptr;

    // Focus buttons
    Gtk::Button* setFocusButton = nullptr;
    Gtk::Button* getFocusButton = nullptr;
    Gtk::Button* clearFocusButton = nullptr;

    // PGN buttons
    Gtk::Button* importPgnButton = nullptr;
    Gtk::Button* addToPgnButton = nullptr;
    Gtk::Button* applyPgnButton = nullptr;
    Gtk::Button* clearPgnButton = nullptr;

    // Navigate buttons
    Gtk::Button* backButton = nullptr;
    Gtk::Button* forwardButton = nullptr;

    // Analyze buttons
    Gtk::Button* nextGenButton = nullptr;
    Gtk::ToggleButton* analyzeToggle = nullptr;

    Gtk::TextView* pvInfo = nullptr;
    Gtk::TextView* pgnTextView = nullptr;
    Glib::RefPtr<Gtk::TextBuffer::Tag> pgnCurrMoveTag;


    BookBuildControl bbControl;
    Position pos;            // Position corresponding to chess board and tree view.
    std::vector<Move> moves; // Moves leading to pos.
    std::vector<Move> nextMoves; // Moves following pos.

    GameTree gameTree;       // Game tree corresponding to the PGN view.
    std::string pgn;
    std::set<GameTree::RangeToNode> pgnPosToNodes;

    std::string pgnImportFilename; // Last filename for PGN import.
    int pgnImportMaxPly = 40;


    bool loadingBook; // True when loading a book file.

    enum class SearchState {
        RUNNING,      // Threads are running, new jobs are scheduled
        STOPPING,     // Waiting for current jobs to finish
        STOPPED       // Threads are not running
    };
    SearchState searchState;
    bool analysing;   // True when analysis thread is running.
    bool bookDirty;   // True if book is unsaved.
};

#endif /* BOOKGUI_HPP_ */
