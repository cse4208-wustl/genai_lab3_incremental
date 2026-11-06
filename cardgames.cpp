// cardgames.cpp - Simple card games in the terminal:
//
//  * Go Fish: play against computer players with a Texas Hold'em, Pinochle
//    or Uno deck (see the Go Fish section below for the rules used).
//
// and two deal-only games (no betting, no play):
//
//  * Texas Hold'em: each player gets two hole cards, then the flop (3),
//    turn (1) and river (1) are dealt to the table (with burn cards, as in a
//    real game). Every player's best 5-card hand out of their 7 cards is found
//    and the winner(s) announced.
//
//  * Pinochle (four-handed, partnerships): the 48-card pinochle deck is dealt
//    to four players, 12 cards each. Trump is picked at random (there is no
//    bidding), and every player's melds are found and scored.
//
// Build:  g++ -std=c++17 -O2 -Wall -o cardgames cardgames.cpp
// Run:    ./cardgames         (choose a game from the menu)

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------- Cards ----

enum Suit { CLUBS, DIAMONDS, HEARTS, SPADES };

struct Card {
    int rank;   // 2..14 (11=J, 12=Q, 13=K, 14=A)
    Suit suit;
};

std::string rankName(int r) {
    static const char* names[] = {"", "", "2", "3", "4", "5", "6", "7",
                                  "8", "9", "10", "J", "Q", "K", "A"};
    return names[r];
}

std::string rankWord(int r, bool plural = false) {
    static const char* words[] = {"", "", "Two", "Three", "Four", "Five", "Six",
                                  "Seven", "Eight", "Nine", "Ten", "Jack",
                                  "Queen", "King", "Ace"};
    std::string w = words[r];
    if (plural) w += (r == 6) ? "es" : "s";  // "Sixes"
    return w;
}

std::string toString(const Card& c) {
    static const char* suits[] = {"♣", "♦", "♥", "♠"};  // ♣ ♦ ♥ ♠
    bool red = (c.suit == HEARTS || c.suit == DIAMONDS);
    std::string s = rankName(c.rank) + suits[c.suit];
    if (red) s = "\033[31m" + s + "\033[0m";  // red text for hearts/diamonds
    return "[" + s + "]";
}

std::string toString(const std::vector<Card>& cards) {
    std::string s;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i) s += " ";
        s += toString(cards[i]);
    }
    return s;
}

// ----------------------------------------------------------------- Deck ----

class Deck {
public:
    // Shuffles the given cards.
    Deck(std::vector<Card> cards, std::mt19937& rng) : cards_(std::move(cards)) {
        std::shuffle(cards_.begin(), cards_.end(), rng);
    }

    // Standard 52-card deck.
    static std::vector<Card> standard() {
        std::vector<Card> cards;
        for (int s = CLUBS; s <= SPADES; ++s)
            for (int r = 2; r <= 14; ++r)
                cards.push_back({r, static_cast<Suit>(s)});
        return cards;
    }

    // 48-card pinochle deck: two each of 9, 10, J, Q, K, A in every suit.
    static std::vector<Card> pinochle() {
        std::vector<Card> cards;
        for (int copy = 0; copy < 2; ++copy)
            for (int s = CLUBS; s <= SPADES; ++s)
                for (int r = 9; r <= 14; ++r)
                    cards.push_back({r, static_cast<Suit>(s)});
        return cards;
    }

    Card deal() {
        Card c = cards_.back();
        cards_.pop_back();
        return c;
    }
    void burn() { cards_.pop_back(); }

private:
    std::vector<Card> cards_;
};

// ------------------------------------------------------- Hand evaluation ----

enum Category {
    HIGH_CARD, ONE_PAIR, TWO_PAIR, THREE_OF_A_KIND, STRAIGHT,
    FLUSH, FULL_HOUSE, FOUR_OF_A_KIND, STRAIGHT_FLUSH
};

// A hand's strength: category first, then tie-breaking ranks in order of
// importance. Two HandValues compare correctly with ordinary < and ==.
struct HandValue {
    std::vector<int> score;          // score[0] = category, rest = kickers
    std::vector<Card> cards;         // the 5 cards making the hand

    Category category() const { return static_cast<Category>(score[0]); }
    bool operator<(const HandValue& o) const { return score < o.score; }
    bool operator==(const HandValue& o) const { return score == o.score; }
};

// Evaluate exactly five cards.
HandValue evaluate5(const std::array<Card, 5>& h) {
    // Count ranks, check flush.
    std::map<int, int> counts;
    bool flush = true;
    for (const Card& c : h) {
        counts[c.rank]++;
        if (c.suit != h[0].suit) flush = false;
    }

    // Groups ordered by (count desc, rank desc): e.g. full house K K K 4 4 -> K, 4
    std::vector<std::pair<int, int>> groups;  // (count, rank)
    for (auto& [rank, cnt] : counts) groups.push_back({cnt, rank});
    std::sort(groups.rbegin(), groups.rend());

    // Straight check (5 distinct ranks spanning 4, or the A-2-3-4-5 "wheel").
    bool straight = false;
    int straightHigh = 0;
    if (groups.size() == 5) {
        int hi = groups.front().second, lo = groups.back().second;
        if (hi - lo == 4) { straight = true; straightHigh = hi; }
        else if (hi == 14 && groups[1].second == 5) { straight = true; straightHigh = 5; }
    }

    HandValue v;
    v.cards.assign(h.begin(), h.end());
    std::vector<int> ranksByGroup;
    for (auto& g : groups) ranksByGroup.push_back(g.second);

    Category cat;
    if (straight && flush)            cat = STRAIGHT_FLUSH;
    else if (groups[0].first == 4)    cat = FOUR_OF_A_KIND;
    else if (groups[0].first == 3 &&
             groups[1].first == 2)    cat = FULL_HOUSE;
    else if (flush)                   cat = FLUSH;
    else if (straight)                cat = STRAIGHT;
    else if (groups[0].first == 3)    cat = THREE_OF_A_KIND;
    else if (groups[0].first == 2 &&
             groups[1].first == 2)    cat = TWO_PAIR;
    else if (groups[0].first == 2)    cat = ONE_PAIR;
    else                              cat = HIGH_CARD;

    v.score.push_back(cat);
    if (cat == STRAIGHT || cat == STRAIGHT_FLUSH)
        v.score.push_back(straightHigh);
    else
        v.score.insert(v.score.end(), ranksByGroup.begin(), ranksByGroup.end());

    // Order the displayed cards to match the hand (pairs together, high first).
    auto key = [&](const Card& c) {
        if (straight) return (straightHigh == 5 && c.rank == 14) ? 1 : c.rank;
        return counts[c.rank] * 100 + c.rank;
    };
    std::sort(v.cards.begin(), v.cards.end(),
              [&](const Card& a, const Card& b) { return key(a) > key(b); });
    return v;
}

// Best 5-card hand from any number (>= 5) of cards: try all combinations.
HandValue bestHand(const std::vector<Card>& cards) {
    HandValue best;
    const int n = static_cast<int>(cards.size());
    for (int a = 0; a < n; ++a)
     for (int b = a + 1; b < n; ++b)
      for (int c = b + 1; c < n; ++c)
       for (int d = c + 1; d < n; ++d)
        for (int e = d + 1; e < n; ++e) {
            HandValue v = evaluate5({cards[a], cards[b], cards[c], cards[d], cards[e]});
            if (best.score.empty() || best < v) best = v;
        }
    return best;
}

std::string describe(const HandValue& v) {
    const auto& s = v.score;
    switch (v.category()) {
        case STRAIGHT_FLUSH:
            return s[1] == 14 ? "Royal Flush"
                              : "Straight Flush, " + rankWord(s[1]) + " high";
        case FOUR_OF_A_KIND:  return "Four of a Kind, " + rankWord(s[1], true);
        case FULL_HOUSE:      return "Full House, " + rankWord(s[1], true) +
                                     " full of " + rankWord(s[2], true);
        case FLUSH:           return "Flush, " + rankWord(s[1]) + " high";
        case STRAIGHT:        return "Straight, " + rankWord(s[1]) + " high";
        case THREE_OF_A_KIND: return "Three of a Kind, " + rankWord(s[1], true);
        case TWO_PAIR:        return "Two Pair, " + rankWord(s[1], true) +
                                     " and " + rankWord(s[2], true);
        case ONE_PAIR:        return "Pair of " + rankWord(s[1], true);
        case HIGH_CARD:       return "High Card, " + rankWord(s[1]);
    }
    return "?";
}

// --------------------------------------------------------- Hold'em game ----

struct Player {
    std::string name;
    std::vector<Card> hole;
    HandValue best;
};

void playHoldem(int numPlayers, std::mt19937& rng) {
    Deck deck(Deck::standard(), rng);
    std::vector<Player> players(numPlayers);
    for (int i = 0; i < numPlayers; ++i) players[i].name = "Player " + std::to_string(i + 1);

    // Deal hole cards one at a time around the table, twice.
    for (int round = 0; round < 2; ++round)
        for (auto& p : players) p.hole.push_back(deck.deal());

    std::cout << "\n=== Hole cards ===\n";
    for (auto& p : players) std::cout << "  " << p.name << ": " << toString(p.hole) << "\n";

    // Community cards, burning one before each street.
    std::vector<Card> board;
    deck.burn();
    for (int i = 0; i < 3; ++i) board.push_back(deck.deal());
    std::cout << "\n=== Flop  ===  " << toString(board) << "\n";
    deck.burn();
    board.push_back(deck.deal());
    std::cout << "=== Turn  ===  " << toString(board) << "\n";
    deck.burn();
    board.push_back(deck.deal());
    std::cout << "=== River ===  " << toString(board) << "\n";

    // Showdown.
    std::cout << "\n=== Showdown ===\n";
    HandValue top;
    for (auto& p : players) {
        std::vector<Card> seven = p.hole;
        seven.insert(seven.end(), board.begin(), board.end());
        p.best = bestHand(seven);
        if (top.score.empty() || top < p.best) top = p.best;
        std::cout << "  " << p.name << ": " << toString(p.best.cards)
                  << "  " << describe(p.best) << "\n";
    }

    std::vector<const Player*> winners;
    for (auto& p : players)
        if (p.best == top) winners.push_back(&p);

    std::cout << "\n";
    if (winners.size() == 1) {
        std::cout << "*** " << winners[0]->name << " wins with "
                  << describe(top) << "! ***\n";
    } else {
        std::cout << "*** Split pot between ";
        for (size_t i = 0; i < winners.size(); ++i) {
            if (i) std::cout << (i + 1 == winners.size() ? " and " : ", ");
            std::cout << winners[i]->name;
        }
        std::cout << " with " << describe(top) << " ***\n";
    }
}

// -------------------------------------------------------- Pinochle game ----
//
// Four players in two partnerships (Players 1 & 3 vs Players 2 & 4), 12 cards
// each. Meld values are the common modern ones:
//
//   Class A (trump)   Run A-10-K-Q-J of trump  15   (double run 150)
//                     Royal marriage K-Q trump  4   (not counted inside a run)
//                     Marriage K-Q other suit   2
//                     Dix (9 of trump)          1   each
//   Class B (around)  Aces around (one/suit)   10   (double 100)
//                     Kings around              8   (double  80)
//                     Queens around             6   (double  60)
//                     Jacks around              4   (double  40)
//   Class C           Pinochle Q♠ + J♦          4   (double pinochle 30)
//
// A card may count in melds of different classes, but not twice in one class.

std::string suitName(Suit s) {
    static const char* names[] = {"♣ Clubs", "♦ Diamonds",
                                  "♥ Hearts", "♠ Spades"};
    return names[s];
}

// Pinochle card order, high to low: A 10 K Q J 9.
int pinochleOrder(int rank) {
    switch (rank) {
        case 14: return 6;  // A
        case 10: return 5;  // 10 ranks above the King
        case 13: return 4;  // K
        case 12: return 3;  // Q
        case 11: return 2;  // J
        default: return 1;  // 9
    }
}

struct Meld {
    std::string name;
    int points;
};

std::vector<Meld> findMelds(const std::vector<Card>& hand, Suit trump) {
    int n[4][15] = {};  // n[suit][rank] = how many of that card (0, 1 or 2)
    for (const Card& c : hand) n[c.suit][c.rank]++;

    std::vector<Meld> melds;
    auto add = [&](const std::string& name, int pts, int times = 1) {
        for (int i = 0; i < times; ++i) melds.push_back({name, pts});
    };

    // Class A: trump sequences and marriages.
    int runs = std::min({n[trump][14], n[trump][10], n[trump][13],
                         n[trump][12], n[trump][11]});
    if (runs == 2)      add("Double run", 150);
    else if (runs == 1) add("Run (A-10-K-Q-J of trump)", 15);
    add("Royal marriage (K-Q of trump)", 4,
        std::min(n[trump][13], n[trump][12]) - runs);
    for (int s = CLUBS; s <= SPADES; ++s)
        if (s != trump)
            add("Marriage in " + suitName(static_cast<Suit>(s)), 2,
                std::min(n[s][13], n[s][12]));
    add("Dix (9 of trump)", 1, n[trump][9]);

    // Class B: one of a rank in every suit.
    struct Around { int rank; const char* name; int single, dbl; };
    const Around arounds[] = {{14, "Aces", 10, 100}, {13, "Kings", 8, 80},
                              {12, "Queens", 6, 60}, {11, "Jacks", 4, 40}};
    for (const Around& a : arounds) {
        int k = std::min({n[CLUBS][a.rank], n[DIAMONDS][a.rank],
                          n[HEARTS][a.rank], n[SPADES][a.rank]});
        if (k == 2)      add(std::string("Double ") + a.name + " around", a.dbl);
        else if (k == 1) add(std::string(a.name) + " around", a.single);
    }

    // Class C: pinochle.
    int p = std::min(n[SPADES][12], n[DIAMONDS][11]);
    if (p == 2)      add("Double pinochle", 30);
    else if (p == 1) add("Pinochle (Q♠ J♦)", 4);

    return melds;
}

// Show a hand grouped by suit (trump first), each suit high to low.
std::string pinochleHandString(std::vector<Card> hand, Suit trump) {
    auto suitKey = [&](Suit s) { return s == trump ? -1 : static_cast<int>(s); };
    std::sort(hand.begin(), hand.end(), [&](const Card& a, const Card& b) {
        if (a.suit != b.suit) return suitKey(a.suit) < suitKey(b.suit);
        return pinochleOrder(a.rank) > pinochleOrder(b.rank);
    });
    std::string s;
    for (size_t i = 0; i < hand.size(); ++i) {
        if (i) s += (hand[i].suit != hand[i - 1].suit) ? "   " : " ";
        s += toString(hand[i]);
    }
    return s;
}

void playPinochle(std::mt19937& rng) {
    const int numPlayers = 4;
    Deck deck(Deck::pinochle(), rng);
    std::vector<std::vector<Card>> hands(numPlayers);

    // Deal in packets of three around the table until the deck is gone.
    for (int round = 0; round < 4; ++round)
        for (auto& h : hands)
            for (int i = 0; i < 3; ++i) h.push_back(deck.deal());

    // No bidding, so trump is chosen at random.
    Suit trump = static_cast<Suit>(std::uniform_int_distribution<int>(0, 3)(rng));
    std::cout << "\n=== Pinochle ===  Trump: " << suitName(trump)
              << " (chosen at random)\n";

    int teamTotal[2] = {0, 0};
    for (int i = 0; i < numPlayers; ++i) {
        std::vector<Meld> melds = findMelds(hands[i], trump);
        int total = 0;
        for (const Meld& m : melds) total += m.points;
        teamTotal[i % 2] += total;

        std::cout << "\n  Player " << i + 1 << " (Team " << (i % 2 ? "B" : "A")
                  << "): " << pinochleHandString(hands[i], trump) << "\n";
        if (melds.empty()) std::cout << "      no meld\n";
        for (const Meld& m : melds)
        {
            // Pad by on-screen width (suit symbols are several bytes in UTF-8).
            size_t width = 0;
            for (unsigned char ch : m.name) if ((ch & 0xC0) != 0x80) ++width;
            std::cout << "      " << m.name << std::string(width < 32 ? 32 - width : 1, ' ')
                      << std::setw(4) << m.points << "\n";
        }
        std::cout << "      Total meld: " << total << "\n";
    }

    std::cout << "\nTeam A (Players 1 & 3) meld: " << teamTotal[0]
              << "\nTeam B (Players 2 & 4) meld: " << teamTotal[1] << "\n\n";
    if (teamTotal[0] == teamTotal[1])
        std::cout << "*** The teams are tied on meld ***\n";
    else
        std::cout << "*** Team " << (teamTotal[0] > teamTotal[1] ? "A" : "B")
                  << " has the most meld! ***\n";
}

// --------------------------------------------------------- Go Fish game ----
//
// You against 1-5 computer players, with a Texas Hold'em (52), Pinochle (48)
// or Uno (108) deck. On your turn ask another player for a rank you hold. If
// they have any they hand them all over and you go again; otherwise "Go fish!"
// and you draw from the pond (going again only if you drew the rank you asked
// for). Four cards of a rank make a book. Every rank in all three decks comes
// in a multiple of four, so the game ends when every card is in a book; the
// player with the most books wins.

struct FishCard {
    std::string rank;   // what players ask for: "7", "K", "Skip", "+4", ...
    int order;          // sort order within a hand
    std::string label;  // how the card is drawn on screen
};

std::vector<FishCard> fishCards(const std::vector<Card>& cards) {
    std::vector<FishCard> out;
    for (const Card& c : cards) out.push_back({rankName(c.rank), c.rank, toString(c)});
    return out;
}

// 108-card Uno deck: in each color one 0, two each of 1-9, Skip, Reverse and
// Draw Two; plus four Wilds and four Wild Draw Fours.
std::vector<FishCard> unoDeck() {
    const char* colorName[] = {"Red", "Yellow", "Green", "Blue"};
    const char* colorCode[] = {"\033[31m", "\033[33m", "\033[32m", "\033[34m"};
    const char* ranks[] = {"0", "1", "2", "3", "4", "5", "6", "7",
                           "8", "9", "Skip", "Rev", "+2"};
    std::vector<FishCard> out;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 13; ++r)
            for (int copy = 0; copy < (r == 0 ? 1 : 2); ++copy)
                out.push_back({ranks[r], r,
                               std::string("[") + colorCode[c] + colorName[c] + " " +
                                   ranks[r] + "\033[0m]"});
    for (int i = 0; i < 4; ++i) {
        out.push_back({"Wild", 13, "[\033[35mWild\033[0m]"});
        out.push_back({"+4", 14, "[\033[35mWild +4\033[0m]"});
    }
    return out;
}

std::string upper(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

struct FishPlayer {
    std::string name;
    bool human = false;
    std::vector<FishCard> hand;
    std::vector<std::string> books;
    std::set<std::string> knownRanks;  // ranks others have seen them ask for
};

class GoFish {
public:
    GoFish(std::vector<FishCard> deck, int computers, std::mt19937& rng) : rng_(rng) {
        std::shuffle(deck.begin(), deck.end(), rng_);
        pond_ = std::move(deck);
        totalBooks_ = static_cast<int>(pond_.size()) / 4;

        players_.push_back({"You", true, {}, {}, {}});
        for (int i = 1; i <= computers; ++i)
            players_.push_back({"Computer " + std::to_string(i), false, {}, {}, {}});

        int handSize = players_.size() <= 3 ? 7 : 5;
        for (int i = 0; i < handSize; ++i)
            for (auto& p : players_) draw(p);
        for (auto& p : players_) collectBooks(p);
    }

    std::vector<FishPlayer> players_;  // public so tests can make player 0 a computer

    void play() {
        size_t cur = 0;
        while (booksMade() < totalBooks_ && !quit_) {
            FishPlayer& p = players_[cur];
            bool again = true;
            while (again && booksMade() < totalBooks_ && !quit_) {
                if (p.hand.empty()) {
                    if (pond_.empty()) break;  // nothing to do; skip this player
                    std::cout << p.name << (p.human ? " have" : " has")
                              << " no cards and draw" << (p.human ? "" : "s")
                              << " from the pond.\n";
                    draw(p);
                }
                if (targetsFor(cur).empty()) {  // nobody else holds cards
                    if (!pond_.empty()) {
                        std::cout << p.name << (p.human ? " draw" : " draws")
                                  << " a card (nobody else has cards).\n";
                        draw(p);
                        collectBooks(p);
                    }
                    break;
                }
                again = p.human ? humanTurn(cur) : computerTurn(cur);
            }
            cur = (cur + 1) % players_.size();
        }
        if (!quit_) showResults();
    }

private:
    std::vector<FishCard> pond_;
    std::mt19937& rng_;
    int totalBooks_;
    bool quit_ = false;

    int booksMade() const {
        int n = 0;
        for (auto& p : players_) n += static_cast<int>(p.books.size());
        return n;
    }

    bool draw(FishPlayer& p) {
        if (pond_.empty()) return false;
        p.hand.push_back(pond_.back());
        pond_.pop_back();
        return true;
    }

    static int countRank(const FishPlayer& p, const std::string& rank) {
        int n = 0;
        for (auto& c : p.hand) n += (c.rank == rank);
        return n;
    }

    // Lay down every complete book (4 of a rank) in the player's hand.
    void collectBooks(FishPlayer& p) {
        std::map<std::string, int> counts;
        for (auto& c : p.hand) counts[c.rank]++;
        for (auto& [rank, n] : counts) {
            for (int b = 0; b < n / 4; ++b) {
                int removed = 0;
                std::string shown;
                for (auto it = p.hand.begin(); it != p.hand.end() && removed < 4;) {
                    if (it->rank == rank) {
                        shown += it->label + " ";
                        it = p.hand.erase(it);
                        ++removed;
                    } else {
                        ++it;
                    }
                }
                p.books.push_back(rank);
                std::cout << "  >> " << p.name << (p.human ? " make" : " makes")
                          << " a book: " << shown << "\n";
            }
            if (countRank(p, rank) == 0) p.knownRanks.erase(rank);
        }
    }

    std::vector<size_t> targetsFor(size_t asker) const {
        std::vector<size_t> t;
        for (size_t i = 0; i < players_.size(); ++i)
            if (i != asker && !players_[i].hand.empty()) t.push_back(i);
        return t;
    }

    // Carry out an ask. Returns true if the asker gets another turn.
    bool ask(size_t asker, size_t target, const std::string& rank) {
        FishPlayer& a = players_[asker];
        FishPlayer& t = players_[target];
        std::cout << a.name << (a.human ? " ask " : " asks ")
                  << (t.human ? "you" : t.name) << " for " << rank << "s.\n";
        a.knownRanks.insert(rank);

        std::vector<FishCard> got;
        for (auto it = t.hand.begin(); it != t.hand.end();) {
            if (it->rank == rank) { got.push_back(*it); it = t.hand.erase(it); }
            else ++it;
        }
        t.knownRanks.erase(rank);

        if (!got.empty()) {
            std::cout << "  " << (t.human ? "You hand" : t.name + " hands") << " over "
                      << got.size() << " card" << (got.size() > 1 ? "s" : "") << ".\n";
            a.hand.insert(a.hand.end(), got.begin(), got.end());
            collectBooks(a);
            return true;
        }

        std::cout << "  " << (t.human ? "You say" : t.name + " says") << ": Go fish!\n";
        if (!draw(a)) {
            std::cout << "  The pond is empty.\n";
            return false;
        }
        const FishCard& drawn = a.hand.back();
        bool lucky = drawn.rank == rank;
        if (a.human) std::cout << "  You draw " << drawn.label << ".\n";
        else         std::cout << "  " << a.name << " draws a card.\n";
        if (lucky)
            std::cout << "  " << (a.human ? "You" : a.name)
                      << " fished the " << rank << " " << (a.human ? "you" : "they")
                      << " asked for" << (a.human ? "" : " " + drawn.label)
                      << " and go" << (a.human ? "" : "es") << " again!\n";
        collectBooks(a);
        return lucky;
    }

    void showTable(size_t me) const {
        std::cout << "\n------------------------------------------------------------\n";
        std::cout << "Pond: " << pond_.size() << " cards\n";
        for (size_t i = 0; i < players_.size(); ++i) {
            if (i == me) continue;
            auto& p = players_[i];
            std::cout << "  " << i << ") " << p.name << ": " << p.hand.size()
                      << " cards, books: " << booksString(p) << "\n";
        }
        auto hand = players_[me].hand;
        std::sort(hand.begin(), hand.end(),
                  [](const FishCard& a, const FishCard& b) { return a.order < b.order; });
        std::cout << "Your books: " << booksString(players_[me]) << "\nYour hand: ";
        for (size_t i = 0; i < hand.size(); ++i) {
            if (i) std::cout << (hand[i].rank != hand[i - 1].rank ? "   " : " ");
            std::cout << hand[i].label;
        }
        std::cout << "\n";
    }

    static std::string booksString(const FishPlayer& p) {
        if (p.books.empty()) return "none";
        std::string s;
        for (auto& b : p.books) s += (s.empty() ? "" : " ") + b;
        return s;
    }

    // Match typed text to a rank in the player's hand ("k" -> "K", "rev" etc.)
    static std::string matchRank(const FishPlayer& p, std::string typed) {
        typed = upper(typed);
        if (typed == "REVERSE") typed = "REV";
        if (typed == "DRAW2" || typed == "DRAWTWO") typed = "+2";
        if (typed == "WILD+4" || typed == "W+4") typed = "+4";
        for (auto& c : p.hand)
            if (upper(c.rank) == typed) return c.rank;
        return "";
    }

    bool humanTurn(size_t me) {
        showTable(me);
        std::vector<size_t> targets = targetsFor(me);
        size_t target = targets[0];
        if (targets.size() > 1) {
            while (true) {
                std::cout << "Ask which player (";
                for (size_t i = 0; i < targets.size(); ++i)
                    std::cout << (i ? ", " : "") << targets[i];
                std::cout << ") or quit? ";
                std::string in;
                if (!(std::cin >> in) || upper(in) == "QUIT") { quit_ = true; return false; }
                bool ok = false;
                for (size_t t : targets)
                    if (in == std::to_string(t)) { target = t; ok = true; }
                if (ok) break;
                std::cout << "That player can't be asked.\n";
            }
        }
        std::string rank;
        while (true) {
            std::cout << "Ask " << players_[target].name
                      << " for which rank (or quit)? ";
            std::string in;
            if (!(std::cin >> in) || upper(in) == "QUIT") { quit_ = true; return false; }
            rank = matchRank(players_[me], in);
            if (!rank.empty()) break;
            std::cout << "You must ask for a rank that is in your hand.\n";
        }
        return ask(me, target, rank);
    }

    bool computerTurn(size_t me) {
        FishPlayer& p = players_[me];
        std::vector<size_t> targets = targetsFor(me);

        // First choice: a rank I hold that I've seen someone else ask for.
        std::vector<std::pair<size_t, std::string>> known;
        for (size_t t : targets)
            for (auto& r : players_[t].knownRanks)
                if (countRank(p, r) > 0) known.push_back({t, r});
        if (!known.empty()) {
            auto pick = known[std::uniform_int_distribution<size_t>(0, known.size() - 1)(rng_)];
            return ask(me, pick.first, pick.second);
        }

        // Otherwise: the rank I hold most of, from a random player.
        std::map<std::string, int> counts;
        for (auto& c : p.hand) counts[c.rank]++;
        int most = 0;
        for (auto& [r, n] : counts) most = std::max(most, n);
        std::vector<std::string> best;
        for (auto& [r, n] : counts) if (n == most) best.push_back(r);
        std::string rank = best[std::uniform_int_distribution<size_t>(0, best.size() - 1)(rng_)];
        size_t target = targets[std::uniform_int_distribution<size_t>(0, targets.size() - 1)(rng_)];
        return ask(me, target, rank);
    }

    void showResults() const {
        std::cout << "\n=== Game over: all " << totalBooks_ << " books are made ===\n";
        size_t most = 0;
        for (auto& p : players_) {
            most = std::max(most, p.books.size());
            std::cout << "  " << p.name << ": " << p.books.size() << " books ("
                      << booksString(p) << ")\n";
        }
        std::vector<std::string> winners;
        for (auto& p : players_) if (p.books.size() == most) winners.push_back(p.name);
        std::cout << "\n*** ";
        if (winners.size() == 1)
            std::cout << winners[0] << (winners[0] == "You" ? " win" : " wins");
        else {
            std::cout << "Tie between ";
            for (size_t i = 0; i < winners.size(); ++i)
                std::cout << (i ? (i + 1 == winners.size() ? " and " : ", ") : "") << winners[i];
        }
        std::cout << " with " << most << " books! ***\n";
    }
};

void playGoFish(std::mt19937& rng) {
    std::string in;
    int deckChoice = 0;
    while (deckChoice < 1 || deckChoice > 3) {
        std::cout << "Which deck?\n"
                     "  1) Texas Hold'em deck (52 cards)\n"
                     "  2) Pinochle deck (48 cards)\n"
                     "  3) Uno deck (108 cards)\n> ";
        if (!(std::cin >> in)) return;
        deckChoice = std::atoi(in.c_str());
    }
    int computers = 0;
    while (computers < 1 || computers > 5) {
        std::cout << "Number of computer opponents (1-5): ";
        if (!(std::cin >> in)) return;
        computers = std::atoi(in.c_str());
    }

    std::vector<FishCard> deck = deckChoice == 1 ? fishCards(Deck::standard())
                               : deckChoice == 2 ? fishCards(Deck::pinochle())
                                                 : unoDeck();
    GoFish game(std::move(deck), computers, rng);
    game.play();
}

// ----------------------------------------------------------------- Menu ----

int main() {
    std::mt19937 rng(std::random_device{}());

    while (true) {
        std::cout << "\nChoose a game:\n"
                     "  1) Texas Hold'em\n"
                     "  2) Pinochle\n"
                     "  3) Go Fish\n"
                     "  q) Quit\n"
                     "> ";
        std::string choice;
        if (!(std::cin >> choice) || choice == "q" || choice == "Q") break;

        if (choice == "1") {
            int numPlayers = 0;
            while (numPlayers < 2 || numPlayers > 10) {
                std::cout << "Number of players (2-10): ";
                if (!(std::cin >> numPlayers)) return 0;
            }
            playHoldem(numPlayers, rng);
        } else if (choice == "2") {
            playPinochle(rng);
        } else if (choice == "3") {
            playGoFish(rng);
        } else {
            std::cout << "Please enter 1, 2, 3 or q.\n";
        }
    }
    return 0;
}
