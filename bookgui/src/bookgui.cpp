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
 * bookgui.cpp
 *
 *  Created on: Apr 2, 2016
 *      Author: petero
 */

#include "bookgui.hpp"
#include "moveGen.hpp"
#include "textio.hpp"
#include <iostream>

int
main(int argc, char* argv[]) {
    auto app = Gtk::Application::create(argc, argv, "org.petero.bookgui");
    app->set_flags(Gio::APPLICATION_NON_UNIQUE);
    BookGui bookGui(app);
    bookGui.run();
}

BookGui::BookGui(Glib::RefPtr<Gtk::Application> app0)
    : app(app0), mainWindow(nullptr), bbControl(*this),
      processingBook(false), searchState(SearchState::STOPPED),
      analysing(false), bookDirty(false) {
    builder = Gtk::Builder::create();
    builder->add_from_resource("/main/bookgui_glade.xml");
}

void
BookGui::run() {
    builder->get_widget("mainWindow", mainWindow);
    connectSignals();
    getWidgets();
    createChessBoard();
    updateEnabledState();
    setPosition(TextIO::readFEN(TextIO::startPosFEN), {}, {});
    app->run(*mainWindow);
}

// --------------------------------------------------------------------------------

void
BookGui::getWidgets() {
    builder->get_widget("pvInfo", pvInfo);

    pgnCurrMoveTag = Gtk::TextBuffer::Tag::create("currMove");
    pgnCurrMoveTag->property_background() = "rgb(192,192,255)";
    pgnTextView->get_buffer()->get_tag_table()->add(pgnCurrMoveTag);
}

void
BookGui::createChessBoard() {
    Gtk::DrawingArea* chessBoardArea;
    builder->get_widget("chessBoard", chessBoardArea);
    chessBoard = make_unique<ChessBoard>(pos, chessBoardArea);
    chessBoard->signal_move_made.connect(sigc::mem_fun(*this, &BookGui::chessBoardMoveMade));
}

void
BookGui::connectSignals() {
    // Notifications from worker threads
    dispatcher.connect([this]{ bookStateChanged(); });

    // Menu items
    builder->get_widget("newMenuItem", newItem);
    newItem->signal_activate().connect([this]{ newBook(); });

    builder->get_widget("openMenuItem", openItem);
    openItem->signal_activate().connect([this]{ openBookFile(); });

    builder->get_widget("saveMenuItem", saveItem);
    saveItem->signal_activate().connect([this]{ saveBookFile(); });

    builder->get_widget("saveAsMenuItem", saveAsItem);
    saveAsItem->signal_activate().connect([this]{ saveBookFileAs(); });

    builder->get_widget("quitMenuItem", quitItem);
    quitItem->signal_activate().connect([this]{ quit(); });
    mainWindow->signal_delete_event().connect(sigc::mem_fun(*this, &BookGui::deleteEvent));

    // Chess board
    builder->get_widget("hashEntry", hashEntry);
    hashEntry->signal_activate().connect([this]{ hashEntryChanged();});
    builder->get_widget("fenEntry", fenEntry);
    fenEntry->signal_activate().connect([this]{ fenEntryChanged();});

    // Settings
    builder->get_widget("threads", threads);
    threads->signal_value_changed().connect([this]{ threadsValueChanged(); });

    builder->get_widget("compTime", compTime);
    compTime->signal_value_changed().connect([this]{ compTimeChanged(); });

    builder->get_widget("depthCost", depthCost);
    depthCost->signal_value_changed().connect([this]{ depthCostChanged(); });

    builder->get_widget("ownPathErrCost", ownPathErrCost);
    ownPathErrCost->signal_value_changed().connect([this]{ ownPathErrCostChanged(); });

    builder->get_widget("otherPathErrCost", otherPathErrCost);
    otherPathErrCost->signal_value_changed().connect([this]{ otherPathErrCostChanged(); });

    builder->get_widget("pgnMaxPly", pgnMaxPly);
    pgnMaxPly->signal_value_changed().connect([this]{ pgnMaxPlyChanged(); });

    // Start/stop buttons
    builder->get_widget("startButton", startButton);
    startButton->signal_clicked().connect([this]{ startSearch(); });

    builder->get_widget("softStopButton", softStopButton);
    softStopButton->signal_clicked().connect([this]{ softStopSearch(); });

    builder->get_widget("hardStopButton", hardStopButton);
    hardStopButton->signal_clicked().connect([this]{ hardStopSearch(); });

    // Focus buttons
    builder->get_widget("setFocusButton", setFocusButton);
    setFocusButton->signal_clicked().connect([this]{ setFocus(); });

    builder->get_widget("getFocusButton", getFocusButton);
    getFocusButton->signal_clicked().connect([this]{ getFocus(); });

    builder->get_widget("clearFocusButton", clearFocusButton);
    clearFocusButton->signal_clicked().connect([this]{ clearFocus(); });

    // PGN buttons
    builder->get_widget("importPgnButton", importPgnButton);
    importPgnButton->signal_clicked().connect([this]{ importPgn(); });

    builder->get_widget("addToPgnButton", addToPgnButton);
    addToPgnButton->signal_clicked().connect([this]{ addToPgn(); });

    builder->get_widget("applyPgnButton", applyPgnButton);
    applyPgnButton->signal_clicked().connect([this]{ applyPgn(); });

    builder->get_widget("clearPgnButton", clearPgnButton);
    clearPgnButton->signal_clicked().connect([this]{ clearPgn(); });

    // Navigate buttons
    builder->get_widget("backButton", backButton);
    backButton->signal_clicked().connect([this]{ posGoBack(); });

    builder->get_widget("forwardButton", forwardButton);
    forwardButton->signal_clicked().connect([this]{ posGoForward(); });

    // Analyze buttons
    builder->get_widget("nextGenButton", nextGenButton);
    nextGenButton->signal_clicked().connect([this]{ nextGeneration(); });

    builder->get_widget("analyzeToggle", analyzeToggle);
    analyzeToggle->signal_clicked().connect([this]{ toggleAnalyzeMode(); });

    // PGN view
    builder->get_widget("pgnTextView", pgnTextView);
    pgnTextView->signal_button_press_event().connect(sigc::mem_fun(*this, &BookGui::pgnButtonPressed), false);
    pgnTextView->set_events(Gdk::BUTTON_PRESS_MASK);
}

void
BookGui::notify() {
    dispatcher.emit();
}

void
BookGui::bookStateChanged() {
    std::vector<BookBuildControl::Change> changes;
    bbControl.getChanges(changes);
    bool updateEnabled = false;
    for (auto& change : changes) {
        switch (change) {
        case BookBuildControl::Change::TREE:
            bookDirty = true;
            updateBoardAndTree();
            break;
        case BookBuildControl::Change::QUEUE:
            updateEnabled = true;
            updateQueueView();
            if (bbControl.nRunningThreads() == 0)
                searchState = SearchState::STOPPED;
            break;
        case BookBuildControl::Change::PV:
            updatePVView();
            break;
        case BookBuildControl::Change::PROCESSING_COMPLETE:
            updateEnabled = true;
            processingBook = false;
            break;
        }
    }
    if (updateEnabled)
        updateEnabledState();
}

void
BookGui::updateBoardAndTree() {
    chessBoard->queueDraw();
    // FIXME!!
}

void
BookGui::updateHashFenEntry() {
    hashEntry->set_text(num2Hex(pos.bookHash()));
    fenEntry->set_text(TextIO::toFEN(pos));
}

void
BookGui::updateQueueView() {
    // FIXME!!
}

void
BookGui::updatePVView() {
    std::string pv;
    if (analysing)
        bbControl.getPVInfo(pv);
    pvInfo->get_buffer()->set_text(pv);
}

void
BookGui::updatePGNView() {
    gameTree.getGameTreeString(pgn, pgnPosToNodes);
    pgnTextView->get_buffer()->set_text(pgn);
    updatePGNSelection();
}

void
BookGui::updatePGNSelection() {
    GameNode gn = gameTree.getRootNode();
    bool found = true;
    for (const Move& m : moves) {
        found = false;
        int n = gn.nChildren();
        for (int i = 0; i < n; i++) {
            gn.goForward(i);
            if (m.equals(gn.getMove())) {
                found = true;
                break;
            }
            gn.goBack();
        }
        if (!found)
            break;
    }

    auto buf = pgnTextView->get_buffer();
    buf->remove_tag(pgnCurrMoveTag, buf->begin(), buf->end());
    if (found) {
        std::shared_ptr<Node> node = gn.getNode();
        for (const auto& rtn : pgnPosToNodes) {
            if (rtn.node == node) {
                auto bi = buf->begin();
                bi.set_offset(rtn.begin);
                auto ei = buf->begin();
                ei.set_offset(rtn.end);
                buf->apply_tag(pgnCurrMoveTag, bi, ei);
                break;
            }
        }
    }
}

void
BookGui::updateEnabledState() {
    // Menu items
    bool searchStopped = searchState == SearchState::STOPPED;
    bool builderIdle = searchStopped &&
                       bbControl.nRunningThreads() == 0 && !processingBook;
    newItem->set_sensitive(builderIdle);
    openItem->set_sensitive(builderIdle);
    saveItem->set_sensitive(!bbControl.getBookFileName().empty() && !processingBook);
    saveAsItem->set_sensitive(!processingBook);
    quitItem->set_sensitive(!processingBook);

    // Settings widgets
    threads->set_sensitive(searchStopped);
//  compTime->set_sensitive(true);
    depthCost->set_sensitive(searchStopped);
    ownPathErrCost->set_sensitive(searchStopped);
    otherPathErrCost->set_sensitive(searchStopped);
//  pgnMaxPly->set_sensitive(true);

    // Start/stop buttons
    startButton->set_sensitive(searchStopped);
    softStopButton->set_sensitive(searchState == SearchState::RUNNING);
    hardStopButton->set_sensitive(!searchStopped);

    // Focus buttons
    setFocusButton->set_sensitive(!processingBook);
    getFocusButton->set_sensitive(!processingBook);
    clearFocusButton->set_sensitive(!processingBook);

    // PGN buttons
//  importPgnButton->set_sensitive(true);
//  addToPgnButton->set_sensitive(true);
    applyPgnButton->set_sensitive(!processingBook);
//  clearPgnButton->set_sensitive(true);

    // Navigate buttons
    backButton->set_sensitive(!moves.empty());
    forwardButton->set_sensitive(!nextMoves.empty());

    // Analyze buttons
//  nextGenButton->set_sensitive(true);
//  analyzeToggle->set_sensitive(true);
}

// --------------------------------------------------------------------------------

void
BookGui::
hashEntryChanged() {

}

void
BookGui::fenEntryChanged() {

}

void
BookGui::setPosition(const Position& newPos, const std::vector<Move>& movesBefore,
                     const std::vector<Move>& movesAfter) {
    moves = movesBefore;
    nextMoves = movesAfter;
    if (!newPos.equals(pos)) {
        pos = newPos;
        if (analysing)
            bbControl.startAnalysis(moves);
        updateHashFenEntry();
    }
}

// --------------------------------------------------------------------------------

void
BookGui::newBook() {
    if (searchState != SearchState::STOPPED || bbControl.nRunningThreads() > 0 || processingBook)
        return;
    if (!askSaveIfDirty())
        return;

    bbControl.newBook();
    setPosition(TextIO::readFEN(TextIO::startPosFEN), {}, {});
    bookDirty = false;
    updateBoardAndTree();
    updatePGNSelection();
    updateEnabledState();
}

void
BookGui::openBookFile() {
    if (searchState != SearchState::STOPPED || bbControl.nRunningThreads() > 0 || processingBook)
        return;
    if (!askSaveIfDirty())
        return;

    Gtk::FileChooserDialog dialog("Open book", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*mainWindow);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    std::string filename = bbControl.getBookFileName();
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);
    if (file && !filename.empty())
        file = file->get_parent();
    if (file)
        dialog.set_current_folder_file(file);

    auto bookFilter = Gtk::FileFilter::create();
    bookFilter->set_name("Texel book files");
    bookFilter->add_pattern("*.tbin");
    bookFilter->add_pattern("*.tbin.log");
    dialog.add_filter(bookFilter);
    auto allFilter = Gtk::FileFilter::create();
    allFilter->set_name("All files");
    allFilter->add_pattern("*");
    dialog.add_filter(allFilter);

    if (dialog.run() != Gtk::RESPONSE_OK)
        return;

    filename = dialog.get_filename();
    processingBook = true;
    bbControl.readFromFile(filename);
    updateEnabledState();
}

void
BookGui::saveBookFile() {
    if (bbControl.getBookFileName().empty() || processingBook)
        return;
    processingBook = true;
    bbControl.saveToFile("");
    bookDirty = false;
}

bool
BookGui::saveBookFileAs() {
    if (processingBook)
        return false;

    Gtk::FileChooserDialog dialog("Save As", Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_transient_for(*mainWindow);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);

    std::string filename = bbControl.getBookFileName();
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);
    if (file && !filename.empty())
        dialog.set_filename(filename);
    else if (file)
        dialog.set_current_folder_file(file);

    auto bookFilter = Gtk::FileFilter::create();
    bookFilter->set_name("Texel book files");
    bookFilter->add_pattern("*.tbin");
    bookFilter->add_pattern("*.tbin.log");
    dialog.add_filter(bookFilter);
    auto allFilter = Gtk::FileFilter::create();
    allFilter->set_name("All files");
    allFilter->add_pattern("*");
    dialog.add_filter(allFilter);

    if (dialog.run() != Gtk::RESPONSE_OK)
        return false;
    filename = dialog.get_filename();
    if (filename.empty())
        return false;

    file = Gio::File::create_for_path(filename);
    if (file->get_basename().find('.') == std::string::npos)
        filename = filename + ".tbin";

    processingBook = true;
    bbControl.saveToFile(filename);
    bookDirty = false;
    updateEnabledState();
    return true;
}

void
BookGui::quit() {
    if (!askSaveIfDirty())
        return;
    mainWindow->hide();
}

bool
BookGui::deleteEvent(_GdkEventAny* e) {
    if (!askSaveIfDirty())
        return true;

    return false;
}

bool
BookGui::askSaveIfDirty() {
    if (!bookDirty)
        return true;

    bool hasFilename = !bbControl.getBookFileName().empty();

    Gtk::MessageDialog dialog(*mainWindow, "Save book before closing?", false,
                              Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);
    dialog.set_secondary_text("If you don't save, changes to the book will be lost.");
    const int SAVE = 1;
    const int NOSAVE = 2;
    const int CANCEL = 3;
    dialog.add_button(hasFilename ? "_Save" : "Save _As", SAVE);
    dialog.add_button("Close without saving", NOSAVE);
    dialog.add_button("_Cancel", CANCEL);

    int result = dialog.run();
    if (result == CANCEL)
        return false;
    if (result == NOSAVE)
        return true;

    if (hasFilename)
        saveBookFile();
    else
        return saveBookFileAs();

    return true;
}

// --------------------------------------------------------------------------------

void
BookGui::threadsValueChanged() {
    BookBuildControl::Params params;
    bbControl.getParams(params);
    params.nThreads = threads->get_value_as_int();
    bbControl.setParams(params);
}

void
BookGui::compTimeChanged() {
    BookBuildControl::Params params;
    bbControl.getParams(params);
    params.computationTime = compTime->get_value_as_int();
    bbControl.setParams(params);
}

void
BookGui::depthCostChanged() {
    BookBuildControl::Params params;
    bbControl.getParams(params);
    params.bookDepthCost = depthCost->get_value_as_int();
    bbControl.setParams(params);
}

void
BookGui::ownPathErrCostChanged() {
    BookBuildControl::Params params;
    bbControl.getParams(params);
    params.ownPathErrorCost = ownPathErrCost->get_value_as_int();
    bbControl.setParams(params);
}

void
BookGui::otherPathErrCostChanged() {
    BookBuildControl::Params params;
    bbControl.getParams(params);
    params.otherPathErrorCost = otherPathErrCost->get_value_as_int();
    bbControl.setParams(params);
}

void
BookGui::pgnMaxPlyChanged() {
    pgnImportMaxPly = pgnMaxPly->get_value_as_int();
}

// --------------------------------------------------------------------------------

void
BookGui::startSearch() {
    if (searchState != SearchState::STOPPED)
        return;

    bbControl.startSearch();
    searchState = SearchState::RUNNING;
    updateEnabledState();
}

void
BookGui::softStopSearch() {
    if (searchState != SearchState::RUNNING)
        return;
    bbControl.stopSearch(false);
    searchState = SearchState::STOPPING;
    updateEnabledState();
}

void
BookGui::hardStopSearch() {
    if (searchState == SearchState::STOPPED)
        return;
    bbControl.stopSearch(true);
    searchState = SearchState::STOPPING;
    updateEnabledState();
}

// --------------------------------------------------------------------------------

void
BookGui::setFocus() {
    bbControl.setFocus(pos);
}

void
BookGui::getFocus() {
    Position newPos;
    std::vector<Move> movesBefore, movesAfter;
    if (!bbControl.getFocus(newPos, movesBefore, movesAfter))
        return;
    setPosition(newPos, movesBefore, movesAfter);
    updateBoardAndTree();
    updatePGNSelection();
}

void
BookGui::clearFocus() {
    Position startPos = TextIO::readFEN(TextIO::startPosFEN);
    bbControl.setFocus(startPos);
}

// --------------------------------------------------------------------------------

void
BookGui::importPgn() {
    Gtk::FileChooserDialog dialog("Import PGN", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.set_transient_for(*mainWindow);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_OK);

    std::string filename = pgnImportFilename;
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);
    if (file && !filename.empty())
        file = file->get_parent();
    if (file)
        dialog.set_current_folder_file(file);

    auto bookFilter = Gtk::FileFilter::create();
    bookFilter->set_name("PGN files");
    bookFilter->add_pattern("*.pgn");
    dialog.add_filter(bookFilter);
    auto allFilter = Gtk::FileFilter::create();
    allFilter->set_name("All files");
    allFilter->add_pattern("*");
    dialog.add_filter(allFilter);

    if (dialog.run() != Gtk::RESPONSE_OK)
        return;

    pgnImportFilename = dialog.get_filename();

    int nGames = 0;
    try {
        Position startPos = TextIO::readFEN(TextIO::startPosFEN);
        std::ifstream is(pgnImportFilename);
        PgnReader reader(is);
        GameTree gt;
        while (reader.readPGN(gt)) {
            nGames++;
            if (gt.getRootNode().getPos().equals(startPos)) {
                gameTree.insertTree(gt, pgnImportMaxPly);
            } else {
                std::cerr << "Skipping game " << nGames << ". Custom start position." << std::endl;
            }
        }
    } catch (...) {
        std::cerr << "Error parsing game " << nGames << std::endl;
        throw;
    }
    updatePGNView();
}

void
BookGui::addToPgn() {
    gameTree.insertMoves(moves);
    updatePGNView();
}

void
BookGui::applyPgn() {
    // FIXME!!
    updatePGNView();
}

void
BookGui::clearPgn() {
    gameTree = GameTree();
    updatePGNView();
}

bool
BookGui::pgnButtonPressed(GdkEventButton* event) {
    int bx, by;
    pgnTextView->window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT, (int)rint(event->x),
                                         (int)rint(event->y), bx, by);
    Gtk::TextBuffer::iterator bufIter;
    int trailing;
    pgnTextView->get_iter_at_position(bufIter, trailing, bx, by);
    int offs = bufIter.get_offset();

    std::shared_ptr<Node> node;
    auto iter = pgnPosToNodes.upper_bound(GameTree::RangeToNode(offs, 0, nullptr));
    if (iter != pgnPosToNodes.begin()) {
        --iter;
        if (iter->begin <= offs && offs < iter->end)
            node = iter->node;
    }
    if (node) {
        GameNode gn = gameTree.getNode(node);
        Position newPos = gn.getPos();
        std::vector<Move> movesBefore, movesAfter;
        while (!gn.getMove().isEmpty()) {
            movesBefore.push_back(gn.getMove());
            gn.goBack();
        }
        std::reverse(movesBefore.begin(), movesBefore.end());
        gn = gameTree.getNode(node);
        while (gn.nChildren() > 0) {
            gn.goForward(0);
            movesAfter.push_back(gn.getMove());
        }
        setPosition(newPos, movesBefore, movesAfter);
        updateBoardAndTree();
        updatePGNSelection();
        updateEnabledState();
    }

    return true;
}

// --------------------------------------------------------------------------------

void
BookGui::posGoBack() {
    if (moves.empty())
        return;

    Position newPos = TextIO::readFEN(TextIO::startPosFEN);
    UndoInfo ui;
    int N = moves.size();
    for (int i = 0; i < N - 1; i++)
        newPos.makeMove(moves[i], ui);

    nextMoves.insert(nextMoves.begin(), moves[N-1]);
    moves.pop_back();

    setPosition(newPos, moves, nextMoves);
    updateBoardAndTree();
    updatePGNSelection();
    updateEnabledState();
}

void
BookGui::posGoForward() {
    if (nextMoves.empty())
        return;

    moves.push_back(nextMoves[0]);
    Position newPos = pos;
    UndoInfo ui;
    newPos.makeMove(nextMoves[0], ui);
    nextMoves.erase(nextMoves.begin());

    setPosition(newPos, moves, nextMoves);
    updateBoardAndTree();
    updatePGNSelection();
    updateEnabledState();
}

void
BookGui::chessBoardMoveMade(const Move& move) {
    MoveList moveList;
    MoveGen::pseudoLegalMoves(pos, moveList);
    MoveGen::removeIllegal(pos, moveList);
    auto performMove = [this](const Move& move){
        Position newPos = pos;
        UndoInfo ui;
        newPos.makeMove(move, ui);
        moves.push_back(move);
        if (!nextMoves.empty()) {
            if (nextMoves[0] == move)
                nextMoves.erase(nextMoves.begin());
            else
                nextMoves.clear();
        }
        setPosition(newPos, moves, nextMoves);
        updateBoardAndTree();
        updatePGNSelection();
        updateEnabledState();
    };
    bool canPromote = false;
    for (int i = 0; i < moveList.size; i++) {
        const Move& m = moveList[i];
        if (m == move) {
            performMove(m);
            return;
        } else {
            Move moveNoPromote(m.from(), m.to(), Piece::EMPTY);
            if (moveNoPromote == move)
                canPromote = true;
        }
    }
    if (canPromote) {
        int prom = getPromotePiece();
        if (prom != Piece::EMPTY) {
            Move m(move.from(), move.to(), prom);
            performMove(m);
        }
    }
}

int
BookGui::getPromotePiece() {
    Gtk::MessageDialog dialog(*mainWindow, "Promote to?", false,
                              Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE, true);
    const int QUEEN = 1;
    const int ROOK = 2;
    const int BISHOP = 3;
    const int KNIGHT = 4;
    dialog.add_button("Queen", QUEEN);
    dialog.add_button("Rook", ROOK);
    dialog.add_button("Bishop", BISHOP);
    dialog.add_button("Knight", KNIGHT);

    int result = dialog.run();
    bool wtm = pos.isWhiteMove();
    switch (result) {
    case QUEEN:
        return wtm ? Piece::WQUEEN : Piece::BQUEEN;
    case ROOK:
        return wtm ? Piece::WROOK : Piece::BROOK;
    case BISHOP:
        return wtm ? Piece::WBISHOP : Piece::BBISHOP;
    case KNIGHT:
        return wtm ? Piece::WKNIGHT : Piece::BKNIGHT;
    default:
        return Piece::EMPTY;
    }
}

// --------------------------------------------------------------------------------

void
BookGui::nextGeneration() {
    bbControl.nextGeneration();
}

void
BookGui::toggleAnalyzeMode() {
    if (analyzeToggle->get_active()) {
        bbControl.startAnalysis(moves);
        analysing = true;
    } else {
        bbControl.stopAnalysis();
        analysing = false;
    }
}
