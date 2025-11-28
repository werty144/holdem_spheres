#include <cassert>
#include <chrono>
#include <iostream>
#include "PokerHandEvaluator/cpp/include/phevaluator/phevaluator.h"
#include "BS_thread_pool.hpp"
#include <fstream>
#include <random>

constexpr char RANKS[] = {'2','3','4','5','6','7','8','9','T','J','Q','K','A'};
constexpr char SUITS[] = {'s', 'c', 'd', 'h'};
constexpr size_t N_HANDS = 1326;
constexpr size_t N_TRIANGLES = 1862796;

typedef std::array<std::array<bool, N_HANDS>, N_HANDS> graph;

struct Hand {
    phevaluator::Card first;
    phevaluator::Card second;

    bool operator^(const Hand& hand) const {
        return first == hand.first ||
                first == hand.second ||
                second == hand.first ||
                second == hand.second;
    }

    [[nodiscard]] std::string to_string() const {
        return "[" + first.describeCard() + " " + second.describeCard() + "]";
    }

    bool operator==(const Hand& hand) const {
        return first == hand.first && second == hand.second;
    }
};

template<>
struct std::hash<Hand> {
    size_t operator()(const Hand& h) const noexcept {
        return static_cast<int>(h.first) * 52 + static_cast<int>(h.second);
    }
};

inline std::ostream& operator<< (std::ostream& os, const Hand& hand) {
    return os << hand.to_string();
}

std::array<phevaluator::Card, 52> new_deck() {
    std::array<phevaluator::Card, 52> deck;
    for (auto& r: RANKS) {
        for (auto& s: SUITS) {
            auto card = phevaluator::Card({r, s});
            deck[card] = card;
        }
    }
    return deck;
}

std::array<Hand, N_HANDS> get_all_hands() {
    std::array<Hand, N_HANDS> hands;
    auto deck = new_deck();
    size_t curr_ind = 0;
    for (size_t c1= 0 ; c1 < 52; c1++) {
        auto card1 = deck[c1];
        for (size_t c2= c1 + 1 ; c2 < 52; c2++) {
            auto card2 = deck[c2];
            hands[curr_ind] = Hand(card1, card2);
            curr_ind++;
        }
    }
    return hands;
}

std::unordered_map<Hand, size_t> hands_to_index() {
    auto hands = get_all_hands();
    std::unordered_map<Hand, size_t> hands_2_index;
    for (size_t i = 0; i < hands.size(); i++) {
        hands_2_index[hands[i]] = i;
        hands_2_index[Hand(hands[i].second, hands[i].first)] = i;
    }
    return hands_2_index;
}

struct Triangle {
    int hand1, hand2, hand3;

    Triangle() = default;

    Triangle(int hand1, int hand2, int hand3) : hand1(hand1), hand2(hand2), hand3(hand3){}

    Triangle(Hand h1, Hand h2, Hand h3, std::unordered_map<Hand, size_t> hand_2_idx) :
        hand1(hand_2_idx[h1]),
        hand2(hand_2_idx[h2]),
        hand3(hand_2_idx[h3])
    {}
};
bool operator<(const Triangle& a, const Triangle& b) {
    if (a.hand1 != b.hand1) return a.hand1 < b.hand1;
    if (a.hand2 != b.hand2) return a.hand2 < b.hand2;
    return a.hand3 < b.hand3;
}

bool operator==(const Triangle& a, const Triangle& b) {
    if (a.hand1 != b.hand1) return false;
    if (a.hand2 != b.hand2) return false;
    return a.hand3 == b.hand3;
}



int compare(Triangle& a, Triangle& b, graph& preflop_graph) {
    if (
        preflop_graph[a.hand1][b.hand1] && preflop_graph[a.hand1][b.hand2] && preflop_graph[a.hand1][b.hand3] &&
        preflop_graph[a.hand2][b.hand1] && preflop_graph[a.hand2][b.hand2] && preflop_graph[a.hand2][b.hand3] &&
        preflop_graph[a.hand3][b.hand1] && preflop_graph[a.hand3][b.hand2] && preflop_graph[a.hand3][b.hand3]
    ) {
        return -1;
    }

    if (
        preflop_graph[b.hand1][a.hand1] && preflop_graph[b.hand1][a.hand2] && preflop_graph[b.hand1][a.hand3] &&
        preflop_graph[b.hand2][a.hand1] && preflop_graph[b.hand2][a.hand2] && preflop_graph[b.hand2][a.hand3] &&
        preflop_graph[b.hand3][a.hand1] && preflop_graph[b.hand3][a.hand2] && preflop_graph[b.hand3][a.hand3]
    ) {
        return 1;
    }
    return 0;
}


std::array<std::array<bool, N_HANDS>, N_HANDS> get_preflop_graph() {
    graph preflop_graph{};
    auto hands_2_index = hands_to_index();

    std::ifstream in("hand_comparison.txt");
    std::string c1, c2, c3, c4;
    float p1, p2;
    char _;
    while (in >> _ >> c1 >> c2 >> _ >> c3 >> c4 >> p1 >> p2) {
        auto hand1 = Hand(c1, c2.substr(0, 2));
        auto hand2 = Hand(c3, c4.substr(0, 2));
        auto idx1 = hands_2_index[hand1];
        auto idx2 = hands_2_index[hand2];
        if (p1 > p2) {
            preflop_graph[idx1][idx2] = true;
        }
        if (p2 > p1) {
            preflop_graph[idx2][idx1] = true;
        }
    }
    return preflop_graph;
}

bool has_deux(Hand hand) {
    return hand.first.describeRank() == '2' || hand.second.describeRank() == '2';
}

void list_triangles(graph preflop_graph) {
    std::ofstream out_file("triangles.txt");
    size_t cnt = 0;
    for (size_t i = 0; i < N_HANDS; i++) {
        for (size_t j = i + 1; j < N_HANDS; j++) {
            for (size_t k = j + 1; k < N_HANDS; k++) {
                if (
                    preflop_graph[i][j] && preflop_graph[j][k] && preflop_graph[k][i] ||
                    preflop_graph[i][k] && preflop_graph[k][j] && preflop_graph[j][i]
                    ) {
                    out_file << i << " " << j << " " << k << std::endl;
                    cnt++;
                }
            }
        }
    }
    std::cout << cnt << std::endl;
}

std::vector<Triangle> load_triangles() {
    std::vector<Triangle> triangles(N_TRIANGLES);
    std::ifstream in("triangles.txt");
    int h1, h2, h3;
    size_t idx = 0;
    while (in >> h1 >> h2 >> h3) {
        triangles[idx] = Triangle(h1, h2, h3);
        idx++;
    }
    assert(idx == N_TRIANGLES);
    return triangles;
}

void compute_witness_transitivity(graph& preflop_graph) {
    auto idx_2_hands = get_all_hands();
    size_t max_witness_hypothesis = 1;
    for (size_t i = 0; i < N_HANDS; i++) {
        for (size_t j = i + 1; j < N_HANDS; j++) {
            if (!preflop_graph[j][i]) continue;
            size_t cnt = 0;
            for (size_t k = 0; k < N_HANDS; k++) {
                if (preflop_graph[i][k] && preflop_graph[k][j]) {
                    cnt++;
                }
            }
            if (cnt >= max_witness_hypothesis) {
                max_witness_hypothesis = cnt + 1;
                std:: cout << idx_2_hands[i] << " " << idx_2_hands[j] << std::endl;
                for (size_t k = 0; k < N_HANDS; k++) {
                    if (preflop_graph[i][k] && preflop_graph[k][j]) {
                        std::cout << idx_2_hands[k] << " ";
                    }
                }
                std::cout << std::endl;
            }
        }
    }
}

void count_triangle_graph_edges(graph& preflop_graph, std::vector<Triangle>& triangles) {

    auto jobs = [&preflop_graph, &triangles](size_t i) {
        size_t edge_cnt = 0;
        for (size_t j = i + 1; j < N_TRIANGLES; j++) {
            if (compare(triangles[i], triangles[j], preflop_graph) != 0) {
                edge_cnt++;
            }
        }
        return edge_cnt;
    };
    size_t N = N_TRIANGLES;
    auto t0 = std::chrono::high_resolution_clock::now();

    BS::thread_pool pool;
    auto futures = pool.submit_sequence(0, N, jobs);
    auto results = futures.get();
    // std::vector<size_t> results(N);
    // for (size_t i = 0; i < N; i++) {
    //     results[i] = jobs(i);
    // }


    size_t total_edges = 0;
    for (size_t i = 0; i < N; i++) {
        total_edges += results[i];
    }
    std::cout << total_edges << std::endl;


    auto t1 = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "Time: " << ms << " us\n";
}

std::vector<Triangle> get_triangle_outgoing_edges(
    Triangle& triangle,
    std::vector<std::vector<Triangle>>& triangles_beaten_by_hand,
    graph preflop_graph
    ) {
    std::vector<Triangle> out;
    auto a = triangles_beaten_by_hand[triangle.hand1];
    auto b = triangles_beaten_by_hand[triangle.hand2];
    auto c = triangles_beaten_by_hand[triangle.hand3];

    if (a.size() <= b.size() && a.size() <= c.size()) {
        for (auto &candidate_triangle : a) {
            if (
                preflop_graph[triangle.hand2][candidate_triangle.hand1] &&
                preflop_graph[triangle.hand2][candidate_triangle.hand2] &&
                preflop_graph[triangle.hand2][candidate_triangle.hand3] &&
                preflop_graph[triangle.hand3][candidate_triangle.hand1] &&
                preflop_graph[triangle.hand3][candidate_triangle.hand2] &&
                preflop_graph[triangle.hand3][candidate_triangle.hand3]
                ) out.emplace_back(candidate_triangle);
        }
    } else if (b.size() <= a.size() && b.size() <= c.size()) {
        for (auto &candidate_triangle : b) {
            if (
                preflop_graph[triangle.hand1][candidate_triangle.hand1] &&
                preflop_graph[triangle.hand1][candidate_triangle.hand2] &&
                preflop_graph[triangle.hand1][candidate_triangle.hand3] &&
                preflop_graph[triangle.hand3][candidate_triangle.hand1] &&
                preflop_graph[triangle.hand3][candidate_triangle.hand2] &&
                preflop_graph[triangle.hand3][candidate_triangle.hand3]
                ) out.emplace_back(candidate_triangle);
        }
    } else {
        for (auto &candidate_triangle : c) {
            if (
                preflop_graph[triangle.hand1][candidate_triangle.hand1] &&
                preflop_graph[triangle.hand1][candidate_triangle.hand2] &&
                preflop_graph[triangle.hand1][candidate_triangle.hand3] &&
                preflop_graph[triangle.hand2][candidate_triangle.hand1] &&
                preflop_graph[triangle.hand2][candidate_triangle.hand2] &&
                preflop_graph[triangle.hand2][candidate_triangle.hand3]
                ) out.emplace_back(candidate_triangle);
        }
    }

    return out;
}

std::pair<double, double> compare_hands(Hand hand1, Hand hand2) {
    auto deck = new_deck();
    size_t win1 = 0, win2 = 0, total = 0;
    for (size_t c1= 0 ; c1 < 52; c1++) {
        auto card1 = deck[c1];
        if (card1 == hand1.first ||
            card1 == hand1.second ||
            card1 == hand2.first ||
            card1 == hand2.second) continue;
        for (size_t c2= c1 + 1 ; c2 < 52; c2++) {
            auto card2 = deck[c2];
            if (card2 == hand1.first ||
                card2 == hand1.second ||
                card2 == hand2.first ||
                card2 == hand2.second) continue;
            for (size_t c3= c2 + 1 ; c3 < 52; c3++) {
                auto card3 = deck[c3];
                if (card3 == hand1.first ||
                    card3 == hand1.second ||
                    card3 == hand2.first ||
                    card3 == hand2.second) continue;
                for (size_t c4= c3 + 1 ; c4 < 52; c4++) {
                    auto card4 = deck[c4];
                    if (card4 == hand1.first ||
                        card4 == hand1.second ||
                        card4 == hand2.first ||
                        card4 == hand2.second) continue;;
                    for (size_t c5= c4 + 1 ; c5 < 52; c5++) {
                        auto card5 = deck[c5];
                        if (card5 == hand1.first ||
                            card5 == hand1.second ||
                            card5 == hand2.first ||
                            card5 == hand2.second) continue;
                        auto rank1 = phevaluator::EvaluateCards(
                            hand1.first, hand1.second, card1, card2, card3, card4, card5);
                        auto rank2 = phevaluator::EvaluateCards(
                            hand2.first, hand2.second, card1, card2, card3, card4, card5);
                        if (rank1 > rank2) {
                            win1++;
                        }
                        if (rank1 < rank2) {
                            win2++;
                        }
                        total++;
                    }
                }
            }
        }
    }
    return {win1 * 1.0 / total, win2 * 1.0 / total};
}

void compute_hand_comparison() {
    auto all_hands = get_all_hands();
    std::vector<std::pair<Hand, Hand>> hand_pairs;
    for (size_t h1 = 0; h1 < N_HANDS; h1++) {
        auto hand1 = all_hands[h1];
        for (size_t h2 = h1 + 1; h2 < N_HANDS; h2++) {
            auto hand2 = all_hands[h2];
            if (hand1^hand2) continue;
            hand_pairs.emplace_back(hand1, hand2);
        }
    }

    auto jobs = [&hand_pairs](size_t i) {
        auto hand1 = hand_pairs[i].first;
        auto hand2 = hand_pairs[i].second;
        return compare_hands(hand1, hand2);
    };

    BS::thread_pool pool;
    auto futures = pool.submit_sequence(
        0, hand_pairs.size(), jobs);
    auto results = futures.get();

    std::ofstream out_file("hand_comparison.txt");

    for (size_t i = 0; i < hand_pairs.size(); i++) {
        out_file << hand_pairs[i].first << " " << hand_pairs[i].second << " "
        << results[i].first << " " << results[i].second << std::endl;
    }
}

void print_triangle(Triangle& triangle) {
    auto idx_2_hands = get_all_hands();
    std::cout << idx_2_hands[triangle.hand1] << " "
    << idx_2_hands[triangle.hand2] << " "
    << idx_2_hands[triangle.hand3] << std::endl;
}

std::vector<std::vector<Triangle>> triangles_beaten_by_hand(std::vector<Triangle>& triangles, graph preflop_graph) {
    BS::thread_pool pool;

    auto jobs = [&preflop_graph, &triangles](const size_t hand) {
        std::vector<Triangle> res{};
        for (auto & triangle : triangles) {
            if (preflop_graph[hand][triangle.hand1] &&
                preflop_graph[hand][triangle.hand2] &&
                preflop_graph[hand][triangle.hand3])
            {
                res.emplace_back(triangle);
            }
        }
        return res;
    };

    auto futures = pool.submit_sequence(0, N_HANDS, jobs);
    return futures.get();
}


void playground1() {
    graph preflop_graph = get_preflop_graph();
    std::cout << "Loaded preflop graph" << std::endl;
    auto hand_2_idx = hands_to_index();
    auto triangles = load_triangles();
    std::cout << "Loaded triangles" << std::endl;
    auto triangles_beaten = triangles_beaten_by_hand(triangles, preflop_graph);
    std::cout << "Computed triangles beaten" << std::endl;

    auto all_hands = get_all_hands();

    size_t total = 0;
    for (int i = 0; i < N_HANDS; i++) {
        std::cout << all_hands[i] << ": " << triangles_beaten[i].size() << std::endl;
        total += triangles_beaten[i].size();
    }
    std::cout << total * 1.0 / N_HANDS << std::endl;

    auto pre_low_triangle = Triangle(
        Hand("5h", "4d"),
            Hand("5d", "4c"),
            Hand("5c", "4h"),
            hand_2_idx
    );
    for (auto &triangle : get_triangle_outgoing_edges(pre_low_triangle, triangles_beaten, preflop_graph)) {
        std::cout << all_hands[triangle.hand1] << " " << all_hands[triangle.hand2] << " " << all_hands[triangle.hand3] << std::endl;
    }
}

void playground2() {
    graph preflop_graph = get_preflop_graph();
    auto hand_2_idx = hands_to_index();
    auto triangles = load_triangles();
    std::cout << "Loaded triangles" << std::endl;
    auto triangles_beaten = triangles_beaten_by_hand(triangles, preflop_graph);
    std::cout << "Computed triangles beaten" << std::endl;

    auto hand1= Hand("Ad", "As");
    auto hand2 = Hand("Ah", "As");
    auto idx1 = hand_2_idx[hand1];
    auto idx2 = hand_2_idx[hand2];
    std::cout << triangles_beaten[idx1].size() << " " << triangles_beaten[idx2].size() << std::endl;
}

void playground3() {
    auto hand_2_idx = hands_to_index();
    auto high_triangle = Triangle(
        Hand("Ah", "Kd"),
        Hand("Ad", "Kc"),
        Hand("Ac", "Kh"),
        hand_2_idx);
    auto low_triangle = Triangle(
        Hand("2s", "6c"),
        Hand("2h", "5h"),
        Hand("3h", "4h"),
        hand_2_idx);
}

int main() {
    playground1();
}

