#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <cmath>
#include <fstream>
#include <random>
#include <unordered_set>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

using namespace std;

enum ActionType{ WAIT, SEED, GROW, COMPLETE };

class Action{
public:
    ActionType type;
    int cost;
    char idx;
    char seed_idx;
    double score = 0;

    bool operator==(const Action& rhs){
        return this->type==rhs.type && this->idx==rhs.idx && this->seed_idx==rhs.seed_idx;
    }

    Action(ActionType type, int cost, char idx=-1, char seed_idx=-1) : type(type), cost(cost), idx(idx), seed_idx(seed_idx){}
};

class Board {
public:
    class Cell {
    public:
        char richness = 0;
        char tree_size = -1;
        bool is_mine = true;
        bool is_dormant = false;
        char dormant_day = -1;
        Cell(){
            this->tree_size = -1;
            this->dormant_day = -1;
        }
        Cell(char richness) : richness(richness){
            this->tree_size = -1;
            this->dormant_day = -1;
        }
    };
    static const char id_to_yx[37][2];
    static const char yx_to_id[7][13];
    static vector<char> neighbors[37][3];
    static vector<char> shadings[37][6];
    static bool initialized;
    array<Cell, 37> cells;
    array<array<char, 37>, 6> shadows;
    char day = 0;
    char sun_dir = 0;
    char nutrients = 0;
    array<int, 2> sun;
    array<int, 2> score;
    array<bool, 2> is_waiting;
    bool shadows_initialized = false;

    void init_shadows(){
        if(!this->shadows_initialized){
            for(int i=0; i<37; ++i){
                auto& cell = this->get_cell(i);
                if(cell.tree_size>0){
                    for(int j=0; j<6; ++j){
                        auto& shadings = this->shadings[i][j];
                        if(shadings.size()>0) this->shadows[j][shadings[0]]++;
                    }
                }
            }
            this->shadows_initialized = true;
        }
    }

    Board(){
        if(!this->initialized){
            this->sun[0] = this->sun[1] = 0;
            this->sun_dir = 0;
            this->nutrients = 0;
            this->score[0] = this->score[1] = 0;
            this->is_waiting[0] = this->is_waiting[1] = false;
            const int dirs[6][2] = {{0, 2}, {-1, 1}, {-1, -1}, {0, -2}, {1, -1}, {1, 1}};
            for(int i=0; i<37; ++i){
                auto [y, x] = this->id_to_yx[i];
                for(int j=0; j<6; ++j){
                    this->shadows[j][i] = 0;
                    auto [dy, dx] = dirs[j];
                    int cy = y+dy;
                    int cx = x+dx;
                    for(int dist=1; dist<=3; ++dist){
                        if(this->is_inside(cy, cx)){
                            Board::shadings[i][j].push_back(this->yx_to_id[cy][cx]);
                        }
                        cy += dy;
                        cx += dx;
                    }
                }
                for(int dist=1; dist<=3; ++dist){
                    for(int cy=max(0, y-dist); cy<=min(6, y+dist); ++cy){
                        int dy = abs(y-cy);
                        int r = dist - dy;
                        int cx1 = x-dy-r*2;
                        int cx2 = x+dy+r*2;
                        if(this->is_inside(cy, cx1)){
                            int idx = this->yx_to_id[cy][cx1];
                            Board::neighbors[i][dist-1].push_back(idx);
                        }
                        if(this->is_inside(cy, cx2)){
                            int idx = this->yx_to_id[cy][cx2];
                            Board::neighbors[i][dist-1].push_back(idx);
                        }
                    }
                }
            }
            this->initialized = true;
        }
    }

    inline bool is_inside(int y, int x){ return y>=0 && y<=6 && x>=abs(3-y) && x<=12-abs(3-y); }

    inline Cell& get_cell(int idx){
        return this->cells[idx];
    }

    int get_points(int player=0){
        return this->score[player]+this->sun[player]/3;
    }

    int get_cost(Action& action, int player=0){
        if(action.type==ActionType::WAIT) return 0;
        else if(action.type==ActionType::SEED){
            char n_trees[4] = {};
            const int grow_costs[3] = {1, 3, 7};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=0 && (int)cell.is_mine+player==1){
                    ++n_trees[cell.tree_size];
                }
            }
            return n_trees[0];
        }
        else if(action.type==ActionType::GROW){
            const int grow_costs[3] = {1, 3, 7};
            auto& cell = this->get_cell(action.idx);
            char n_trees[4] = {};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=0 && (int)cell.is_mine+player==1){
                    ++n_trees[cell.tree_size];
                }
            }
            assert(cell.tree_size<3);
            int cost = grow_costs[cell.tree_size]+n_trees[cell.tree_size+1];
            return cost;
        }
        else{
            return 4;
        }
    }

    vector<Action> get_actions(int player=0){
        char n_trees[4] = {};
        const int grow_costs[3] = {1, 3, 7};
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0 && (int)cell.is_mine+player==1){
                //(cell.tree_size<4);
                ++n_trees[cell.tree_size];
            }
        }

        vector<Action> actions = {Action(ActionType::WAIT, 0)};
        // COMPLETE
        if(this->sun[player]>=4){
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size==3 && (int)cell.is_mine+player==1 && cell.dormant_day!=this->day){
                    actions.push_back(Action(ActionType::COMPLETE, 4, idx));
                }
            }
        }
        // GROW
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0 && cell.tree_size<=2 && (int)cell.is_mine+player==1 && cell.dormant_day!=this->day){
                int cost = grow_costs[cell.tree_size]+n_trees[cell.tree_size+1];
                if(this->sun[player]>=cost){
                    actions.push_back(Action(ActionType::GROW, cost, idx));
                }
            }
        }
        // SEED
        int cost = n_trees[0];
        if(this->sun[player]>=cost){
            bool seeded[37] = {};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=1 && (int)cell.is_mine+player==1 && cell.dormant_day!=this->day){
                    for(int d=0; d<cell.tree_size; ++d){
                        auto& neighbors = Board::neighbors[idx][d];
                        for(char n : neighbors){
                            if(seeded[n]) continue;
                            auto cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                actions.push_back(Action(ActionType::SEED, cost, idx, n));
                                seeded[n] = true;
                            }
                        }
                    }
                }
            }
        }
        return actions;
    }

    void update_sun(){
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>0 && this->shadows[this->sun_dir][idx]<=0){
                if(cell.is_mine) this->sun[0] += cell.tree_size;
                else this->sun[1] += cell.tree_size;
            }
        }
    }

    void update(Action action, int player=0){
        if(action.type==ActionType::SEED){
            this->get_cell(action.idx).dormant_day = this->day;
            this->get_cell(action.seed_idx).dormant_day = this->day;
            this->get_cell(action.seed_idx).tree_size = 0;
            this->get_cell(action.seed_idx).is_mine = player==0;
        }
        else if(action.type==ActionType::COMPLETE){
            this->get_cell(action.idx).tree_size = -1;
            this->score[player] += this->nutrients + (this->get_cell(action.idx).richness-1)*2;
            for(int dist=1; dist<=3; ++dist){
                auto& shadings = Board::shadings[action.idx];
                for(int d=0; d<6; ++d){
                    if(dist-1<shadings[d].size()){
                        this->shadows[d][shadings[d][dist-1]]--;
                    }
                }
            }
        }
        else if(action.type==ActionType::GROW){
            auto& cell = this->get_cell(action.idx);
            cell.tree_size += 1;
            int dist = cell.tree_size - 1;
            auto& shadings = Board::shadings[action.idx];
            for(int d=0; d<6; ++d){
                if(dist<shadings[d].size()){
                    this->shadows[d][shadings[d][dist]]++;
                }
            }
            cell.dormant_day = this->day;
            cell.is_dormant = true;
        }
        else{
            this->is_waiting[player] = true;
        }
        this->sun[player] -= action.cost;
        this->sun_dir = this->day % 6;
    }

    void print(){
        cerr << endl;
        cerr << "codingame.cpp" << endl;
        cerr << "Day" << (int)this->day << ": sun_dir=" << (int)this->sun_dir << ", nutrients=" <<  (int)this->nutrients << endl;
        cerr << "P1: " << this->score[0] << "(" << this->sun[0] << "), " << (this->is_waiting[0]? "waiting" : "active") << endl;
        cerr << "P2: " << this->score[1] << "(" << this->sun[1] << "), " << (this->is_waiting[1]? "waiting" : "active") << endl;
        for(int y=0; y<=6; ++y){
            for(int x=0; x<=12; ++x){
                if((x+y)%2==1 && this->is_inside(y, x)){
                    int idx = this->yx_to_id[y][x];
                    auto& cell = this->get_cell(idx);
                    if(cell.tree_size>=0){
                        if(cell.dormant_day==this->day) cerr << "\033[4m";
                        if(cell.is_mine) cerr << "\033[32m";
                        else cerr << "\033[31m";
                        if(this->shadows[this->sun_dir][idx]==0) cerr << "\033[1m";
                        cerr << (int)cell.tree_size;
                    }
                    else if(cell.richness==0){
                        cerr << "#";
                    }
                    else{
                        cerr << ".";
                    }
                }
                else{
                    if(x<=abs(3-y)) cerr << "  ";
                    else cerr << "   ";
                }
                cerr << "\033[0m";
            }
            for(int i=0; i<10-abs(3-y); ++i) cerr << " ";
            cerr << endl << endl;
        }
        cerr << endl;
    }
};

const char Board::id_to_yx[37][2] = {
    {3, 6},
    {3, 8}, {2, 7}, {2, 5}, {3, 4}, {4, 5}, {4, 7},
    {3, 10}, {2, 9}, {1, 8}, {1, 6}, {1, 4}, {2, 3}, {3, 2}, {4, 3}, {5, 4}, {5, 6}, {5, 8}, {4, 9},
    {3, 12}, {2, 11}, {1, 10}, {0, 9}, {0, 7}, {0, 5}, {0, 3}, {1, 2}, {2, 1}, {3, 0}, {4, 1}, {5, 2}, {6, 3}, { 6, 5}, {6, 7}, {6, 9}, {5, 10}, {4, 11}
};
const char Board::yx_to_id[7][13] = {
    {-1, -1, -1, 25, -1, 24, -1, 23, -1, 22, -1, -1, -1},
    {-1, -1, 26, -1, 11, -1, 10, -1, 9, -1, 21, -1, -1},
    {-1, 27, -1, 12, -1, 3, -1, 2, -1, 8, -1, 20, -1},
    {28, -1, 13, -1, 4, -1, 0, -1, 1, -1, 7, -1, 19},
    {-1, 29, -1, 14, -1, 5, -1, 6, -1, 18, -1, 36, -1},
    {-1, -1, 30, -1, 15, -1, 16, -1, 17, -1, 35, -1, -1},
    {-1, -1, -1, 31, -1, 32, -1, 33, -1, 34, -1, -1, -1}
};
bool Board::initialized = false;
vector<char> Board::neighbors[37][3];
vector<char> Board::shadings[37][6];

int main(int argc, char** argv)
{
    ifstream from_a(argv[1]);

    ofstream to_a(argv[2]);
    to_a << endl;

    ifstream from_b(argv[3]);

    ofstream to_b(argv[4]);
    to_b << endl;

    Board board = Board();
    board.nutrients = 20;
    board.sun[0] = board.sun[1] = 2;
    for(int i=0; i<7; ++i){
        auto& cell = board.get_cell(i);
        cell.richness = 3;
    }
    for(int i=7; i<19; ++i){
        auto& cell = board.get_cell(i);
        cell.richness = 2;
    }
    for(int i=19; i<37; ++i){
        auto& cell = board.get_cell(i);
        cell.richness = 1;
    }

    vector<vector<int>> initial_positions = {
        {19, 22, 28, 31}, {19, 23, 28, 32}, {19, 24, 28, 33}, {19, 25, 28, 34},
        {20, 23, 29, 32}, {20, 24, 29, 33}, {20, 25, 29, 34}, {20, 26, 29, 35},
        {21, 24, 30, 33}, {21, 25, 30, 34}, {21, 26, 30, 35}, {21, 27, 30, 36},
    };

    random_device seed;
    mt19937_64 rnd(seed());
    auto initial_position = initial_positions[rnd()%(initial_positions.size())];
    unordered_set<int> samples = {
        1, 2, 3, 7, 8, 9, 10, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 27
    };
    samples.erase(initial_position[0]);
    samples.erase(initial_position[1]);
    auto& cell0 = board.get_cell(initial_position[0]);
    cell0.is_mine = true;
    cell0.tree_size = 1;
    auto& cell1 = board.get_cell(initial_position[1]);
    cell1.is_mine = true;
    cell1.tree_size = 1;
    auto& cell2 = board.get_cell(initial_position[2]);
    cell2.is_mine = false;
    cell2.tree_size = 1;
    auto& cell3 = board.get_cell(initial_position[3]);
    cell3.is_mine = false;
    cell3.tree_size = 1;

    int num_disabled = rnd()%6;
    vector<int> cands(samples.begin(), samples.end());
    random_shuffle(cands.begin(), cands.end());
    for(int i=0; i<num_disabled; ++i){
        int c0 = cands[i];
        auto [y, x] = board.id_to_yx[c0];
        int ry = 6-y;
        int rx = 12-x;
        int c1 = board.yx_to_id[ry][rx];
        auto& cell0 = board.get_cell(c0);
        auto& cell1 = board.get_cell(c1);
        cell0.richness = 0;
        cell1.richness = 0;
    }

    if(rnd()%2==1){
        unordered_set<int> samples = {
            1, 2, 3, 7, 8, 9, 10, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 27
        };
        for(int idx : samples){
            int c0 = idx;
            auto [y, x] = board.id_to_yx[c0];
            int ry = 6-y;
            int rx = 12-x;
            int c1 = board.yx_to_id[ry][rx];
            auto& cell0 = board.get_cell(c0);
            auto& cell1 = board.get_cell(c1);
            swap(cell0, cell1);
        }
    }
    if(rnd()%2==1 && false){
        unordered_set<int> samples = {
            1, 2, 3, 7, 8, 9, 10, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 27
        };
        for(int idx : samples){
            int c0 = idx;
            auto [y, x] = board.id_to_yx[c0];
            int ry = y;
            int rx = 12-x;
            int c1 = board.yx_to_id[ry][rx];
            auto& cell0 = board.get_cell(c0);
            auto& cell1 = board.get_cell(c1);
            swap(cell0, cell1);
        }
    }

    cin.tie(nullptr);
    ios_base::sync_with_stdio(false);
    int number_of_cells = 37;
    to_a << number_of_cells << endl;
    to_b << number_of_cells << endl;
    for (int idx = 0; idx<number_of_cells; idx++) {
        auto& cell = board.get_cell(idx);
        to_a << idx << endl << (int)cell.richness << endl << 0 << endl << 0 << endl << 0 << endl << 0 << endl << 0 << endl << 0 << endl;
        to_b << idx << endl << (int)cell.richness << endl << 0 << endl << 0 << endl << 0 << endl << 0 << endl << 0 << endl << 0 << endl;
    }

    while (1) {
        board.print();

        if(!board.is_waiting[0]) to_a << (int)board.day << endl << (int)board.nutrients << endl;
        if(!board.is_waiting[1]) to_b << (int)board.day << endl << (int)board.nutrients << endl;
        if(!board.is_waiting[0]) to_a << board.sun[0] << endl << board.score[0] << endl << board.sun[1] << endl << board.score[1] << endl << board.is_waiting[1] << endl;
        if(!board.is_waiting[1]) to_b << board.sun[1] << endl << board.score[1] << endl << board.sun[0] << endl << board.score[0] << endl << board.is_waiting[0] << endl;
        int number_of_trees = 0; // the current amount of trees
        vector<int> trees;
        for(int idx=0; idx<37; ++idx){
            if(board.get_cell(idx).tree_size>=0){
                ++number_of_trees;
                trees.push_back(idx);
            }
        }
        if(!board.is_waiting[0]) to_a << number_of_trees << endl;
        if(!board.is_waiting[1]) to_b << number_of_trees << endl;

        for (int i = 0; i < number_of_trees; i++) {
            int idx = trees[i];
            auto& cell = board.get_cell(idx);
            if(!board.is_waiting[0]) to_a << (int)idx << endl;
            if(!board.is_waiting[1]) to_b << (int)idx << endl;
            if(!board.is_waiting[0]) to_a << (int)cell.tree_size << endl << cell.is_mine << endl << (cell.dormant_day==board.day) << endl << endl;
            if(!board.is_waiting[1]) to_b << (int)cell.tree_size << endl << !cell.is_mine << endl << (cell.dormant_day==board.day) << endl << endl;
        }
        board.init_shadows();
        if(!board.is_waiting[0]){
            auto actions_a = board.get_actions(0);
            to_a << actions_a.size() << endl;
            for(int i=0; i<actions_a.size(); ++i){
                auto& action = actions_a[i];
                if(action.type==ActionType::WAIT) to_a << "WAIT" << endl;
                else if(action.type==ActionType::SEED) to_a << "SEED " << (int)action.idx << " " << (int)action.seed_idx << endl;
                else if(action.type==ActionType::GROW) to_a << "GROW " << (int)action.idx << endl;
                else to_a << "COMPLETE " << (int)action.idx << endl;
            }
        }
        if(!board.is_waiting[1]){
            auto actions_b = board.get_actions(1);
            to_b << actions_b.size() << endl;
            for(int i=0; i<actions_b.size(); ++i){
                auto& action = actions_b[i];
                if(action.type==ActionType::WAIT) to_b << "WAIT" << endl;
                else if(action.type==ActionType::SEED) to_b << "SEED " << (int)action.idx << " " << (int)action.seed_idx << endl;
                else if(action.type==ActionType::GROW) to_b << "GROW " << (int)action.idx << endl;
                else to_b << "COMPLETE " << (int)action.idx << endl;
            }
        }

        string type_a = "WAIT", type_b = "WAIT";
        if(!board.is_waiting[0]) from_a >> type_a;
        if(!board.is_waiting[1]) from_b >> type_b;

        int completes = 0;
        Board tmp = board;
        if(type_a=="SEED" && type_b=="SEED"){
            int idx_a = -1, idx_b = -1, seed_idx_a = -1, seed_idx_b = -1;
            if(!board.is_waiting[0]) from_a >> idx_a >> seed_idx_a;
            if(!board.is_waiting[1]) from_b >> idx_b >> seed_idx_b;
            if(seed_idx_a==seed_idx_b){
                cerr << "A: SEED " << idx_a << " " << seed_idx_a << " duplicate" << endl;
                cerr << "B: SEED " << idx_b << " " << seed_idx_b << " duplicate" << endl;
                auto& cell_a = board.get_cell(idx_a);
                auto& cell_b = board.get_cell(idx_b);
                cell_a.is_dormant = true;
                cell_a.dormant_day = board.day;
                cell_b.is_dormant = true;
                cell_b.dormant_day = board.day;
            }
            else{
                Action action_a(ActionType::SEED, 0, idx_a, seed_idx_a);
                action_a.cost = board.get_cost(action_a, 0);
                board.update(action_a, 0);
                cerr << "A: SEED " << idx_a << " " << seed_idx_a << endl;

                Action action_b(ActionType::SEED, 0, idx_b, seed_idx_b);
                action_b.cost = tmp.get_cost(action_b, 1);
                tmp.update(action_b, 1);
                board.update(action_b, 1);
                board.score[1] = tmp.score[1];
                board.sun[1] = tmp.sun[1];
                cerr << "B: SEED " << idx_b << " " << seed_idx_b << endl;
            }
        }
        else{
            if(type_a=="WAIT"){
                Action action(ActionType::WAIT, 0);
                board.update(action, 0);
                cerr << "A: WAIT" << endl;
            }
            else if(type_a=="SEED"){
                int idx, seed_idx;
                from_a >> idx >> seed_idx;
                Action action(ActionType::SEED, 0, idx, seed_idx);
                action.cost = board.get_cost(action, 0);
                board.update(action, 0);
                cerr << "A: SEED " << idx << " " << seed_idx << endl;
            }
            else if(type_a=="GROW"){
                int idx;
                from_a >> idx;
                Action action(ActionType::GROW, 0, idx);
                action.cost = board.get_cost(action, 0);
                board.update(action, 0);
                cerr << "A: GROW " << idx << endl;
            }
            else{
                int idx;
                from_a >> idx;
                Action action(ActionType::COMPLETE, 4, idx);
                board.update(action, 0);
                cerr << "A: COMPLETE " << idx << endl;
                ++completes;
            }
            if(type_b=="WAIT"){
                Action action(ActionType::WAIT, 0);
                board.update(action, 1);
                tmp.update(action, 1);
                board.score[1] = tmp.score[1];
                board.sun[1] = tmp.sun[1];
                cerr << "B: WAIT" << endl;
            }
            else if(type_b=="SEED"){
                int idx, seed_idx;
                from_b >> idx >> seed_idx;
                Action action(ActionType::SEED, 1, idx, seed_idx);
                action.cost = tmp.get_cost(action, 1);
                board.update(action, 1);
                tmp.update(action, 1);
                board.score[1] = tmp.score[1];
                board.sun[1] = tmp.sun[1];
                cerr << "B: SEED " << idx << " " << seed_idx << endl;
            }
            else if(type_b=="GROW"){
                int idx;
                from_b >> idx;
                Action action(ActionType::GROW, 1, idx);
                action.cost = tmp.get_cost(action, 1);
                board.update(action, 1);
                tmp.update(action, 1);
                board.score[1] = tmp.score[1];
                board.sun[1] = tmp.sun[1];
                cerr << "B: GROW " << idx << endl;
            }
            else{
                int idx;
                from_b >> idx;
                Action action(ActionType::COMPLETE, 4, idx);
                board.update(action, 1);
                tmp.update(action, 1);
                board.score[1] = tmp.score[1];
                board.sun[1] = tmp.sun[1];
                cerr << "B: COMPLETE " << idx << endl;
                ++completes;
            }
        }
        board.nutrients = max(0, board.nutrients-completes);
        if(board.is_waiting[0] && board.is_waiting[1]){
            board.is_waiting[0] = false;
            board.is_waiting[1] = false;
            ++board.day;
            board.sun_dir = board.day%6;
            board.update_sun();
            if(board.day==24){
                int d = 0;
                for(int i=0; i<37; ++i){
                    auto& cell = board.get_cell(i);
                    if(cell.tree_size>=0){
                        if(cell.is_mine) ++d;
                        else --d;
                    }
                }
                d = (d>0? 1 : d==0? 0 : -1);
                cout << "PlayerA: " << board.score[0] << " " << board.sun[0] << " " << board.get_points(0)*4+d << endl;
                cout << "PlayerB: " << board.score[1] << " " << board.sun[1] << " " << board.get_points(1)*4-d << endl;
                cerr << "PlayerA: " << board.score[0] << " " << board.sun[0] << " " << board.get_points(0)*4+d << endl;
                cerr << "PlayerB: " << board.score[1] << " " << board.sun[1] << " " << board.get_points(1)*4-d << endl;
                to_a << 24 << endl;
                to_b << 24 << endl;
                from_a.close();
                to_a.close();
                from_b.close();
                to_b.close();
                return 0;
            }
        }
    }
}
