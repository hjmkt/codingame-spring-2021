#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <cmath>
#include <unordered_set>
#include <sstream>
#include <chrono>
#include <random>
#include <cstring>

using namespace std;

std::chrono::system_clock::time_point start_time, current_time;
bool first;

enum ActionType{ WAIT, SEED, GROW, COMPLETE };

class Board;

class Action{
public:
    ActionType type;
    int cost;
    char idx;
    char seed_idx;
    double score = 0;

    inline bool operator==(const Action& rhs){
        return this->type==rhs.type && this->idx==rhs.idx && this->seed_idx==rhs.seed_idx;
    }

    inline Action(ActionType type, int cost, char idx=-1, char seed_idx=-1) : type(type), cost(cost), idx(idx), seed_idx(seed_idx){}
};

class MCT{
public:
    Board* board;
    int pnodes;
    int cnodes;
    double bv;
    double ev;
    vector<Action> actions[2];
    vector<vector<MCT*>> children;
    vector<int> indices[2];
    static chrono::system_clock::time_point start_time;

    MCT(Board& board, bool init=false);
    ~MCT();
    void expand(bool root=false);
    Action get_best_action();
    void search(int max_nodes, int depth=0);
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

    void init_shadows(){
        for(int i=0; i<37; ++i){
            auto& cell = this->get_cell(i);
            if(cell.tree_size>0){
                for(int j=0; j<6; ++j){
                    auto& shadings = this->shadings[i][j];
                    if(shadings.size()>0) this->shadows[j][shadings[0]]++;
                }
            }
        }
    }

    //inline Board(Board& board){
        //memcpy((char*)this, (char*)&board, sizeof(Board));
    //}

    inline Board(){
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

    inline Cell& get_cell(int idx){ return this->cells[idx]; }

    int get_cost(Action& action, int player=0){
        if(action.type==ActionType::WAIT) return 0;
        else if(action.type==ActionType::SEED){
            char n_seed = 0;
            const int grow_costs[3] = {1, 3, 7};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size==0 && (int)cell.is_mine!=player){
                    ++n_seed;
                }
            }
            return n_seed;
        }
        else if(action.type==ActionType::GROW){
            const int grow_costs[3] = {1, 3, 7};
            auto& cell = this->get_cell(action.idx);
            int size = cell.tree_size+1;
            char n_tree = 0;
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size==size && (int)cell.is_mine!=player){
                    ++n_tree;
                }
            }
            int cost = grow_costs[cell.tree_size]+n_tree;
            return cost;
        }
        else{ return 4; }
    }

    //__attribute__ ((noinline)) vector<Action> get_actions(int player=0, int max_seed=1000){
    vector<Action> get_actions(int player=0, int max_seed=1000){
        vector<Action> actions = {Action(ActionType::WAIT, 0)};
        if(this->is_waiting[player]) return actions;

        char n_trees[4] = {};
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0 && (int)cell.is_mine!=player){
                ++n_trees[cell.tree_size];
            }
        }

        const int grow_costs[3] = {1, 3, 7};
        int n_seed = 0;
        vector<pair<char, char>> seed_actions;
        int seeded[37] = {};
        int seed_cost = n_trees[0];
        int max_complete_richness = -1;
        int max_complete_idx = -1;
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if((int)cell.is_mine==player || cell.dormant_day==this->day) continue;
            if(cell.tree_size==0){
                int cost = grow_costs[cell.tree_size]+n_trees[cell.tree_size+1];
                if(this->sun[player]>=cost && this->day<=22){
                    actions.push_back(Action(ActionType::GROW, cost, idx));
                }
            }
            else if(cell.tree_size==1){
                int grow_cost = grow_costs[1]+n_trees[2];
                if(this->sun[player]>=grow_cost && this->day<=22){
                    actions.push_back(Action(ActionType::GROW, grow_cost, idx));
                }
                if(this->sun[player]>=seed_cost){
                    auto& neighbors = Board::neighbors[idx][0];
                    for(char n : neighbors){
                        if(seeded[n]>=1) continue;
                        auto& cell = this->get_cell(n);
                        if(cell.tree_size<0 && cell.richness>0){
                            seed_actions.push_back({idx, n});
                            ++seeded[n];
                            ++n_seed;
                        }
                    }
                }
            }
            else if(cell.tree_size==2){
                int grow_cost = grow_costs[2]+n_trees[3];
                if(this->sun[player]>=grow_cost && this->day<=22){
                    actions.push_back(Action(ActionType::GROW, grow_cost, idx));
                }
                if(this->sun[player]>=seed_cost){
                    {
                        auto& neighbors = Board::neighbors[idx][0];
                        for(char n : neighbors){
                            if(seeded[n]>=1) continue;
                            auto cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                seed_actions.push_back({idx, n});
                                ++seeded[n];
                                ++n_seed;
                            }
                        }
                    }
                    {
                        auto& neighbors = Board::neighbors[idx][1];
                        for(char n : neighbors){
                            if(seeded[n]>=2) continue;
                            auto& cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                seed_actions.push_back({idx, n});
                                ++seeded[n];
                                ++n_seed;
                            }
                        }
                    }
                }
            }
            else if(cell.tree_size==3){
                if(this->sun[player]>=4){
                    if(cell.richness>max_complete_richness){
                        max_complete_richness = cell.richness;
                        max_complete_idx = idx;
                    }
                    actions.push_back(Action(ActionType::COMPLETE, 4, idx));
                }
                if(this->sun[player]>=seed_cost){
                    {
                        auto& neighbors = Board::neighbors[idx][0];
                        for(char n : neighbors){
                            if(seeded[n]>=1) continue;
                            auto& cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                seed_actions.push_back({idx, n});
                                ++seeded[n];
                                ++n_seed;
                            }
                        }
                    }
                    {
                        auto& neighbors = Board::neighbors[idx][1];
                        for(char n : neighbors){
                            if(seeded[n]>=2) continue;
                            auto& cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                seed_actions.push_back({idx, n});
                                ++seeded[n];
                                ++n_seed;
                            }
                        }
                    }
                    {
                        auto& neighbors = Board::neighbors[idx][2];
                        for(char n : neighbors){
                            if(seeded[n]>=2) continue;
                            auto& cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                seed_actions.push_back({idx, n});
                                ++seeded[n];
                                ++n_seed;
                            }
                        }
                    }
                }
            }
        }
        if(this->day==23 && max_complete_richness>0){
            return {Action(ActionType::COMPLETE, 4, max_complete_idx)};
            max_seed = 3;
        }
        if(n_seed>max_seed){
            static const int order[37] = {0, 1, 2, 3, 4, 5, 6, 13, 7, 14, 8, 15, 9, 16, 10, 17, 11, 18, 12, 31, 19, 20, 32, 21, 22, 33, 23, 24, 34, 25, 26, 35, 27, 28, 36, 29, 30};
            sort(seed_actions.begin(), seed_actions.end(), [&](auto l, auto r){
                return order[l.second]<order[r.second];
            });
        }
        for(int i=0; i<min(max_seed, n_seed); ++i){
            auto ac = seed_actions[i];
            actions.push_back(Action(ActionType::SEED, seed_cost, ac.first, ac.second));
        }

        return actions;
    }

    void update_sun(){
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>0 && this->shadows[this->sun_dir][idx]<=0){
                this->sun[1-(int)cell.is_mine] += cell.tree_size;
            }
        }
    }

    void update(Action action0, Action action1){
        action0.cost = this->get_cost(action0, 0);
        action1.cost = this->get_cost(action1, 1);
        if(action0.type==ActionType::WAIT && action1.type==ActionType::WAIT){
            ++this->day;
            this->sun_dir = this->day%6;
            if(this->day<=23) this->update_sun();
            this->is_waiting[0] = this->is_waiting[1] = false;
        }
        else if(action0.type==ActionType::SEED && action1.type==ActionType::SEED && action0.seed_idx==action1.seed_idx){
            auto& cell0 = this->get_cell(action0.idx);
            auto& cell1 = this->get_cell(action1.idx);
            cell0.is_dormant = cell1.is_dormant = true;
            cell0.dormant_day = cell1.dormant_day = this->day;
        }
        else{
            this->update(action0, 0);
            this->update(action1, 1);
            if(action0.type==ActionType::COMPLETE) --this->nutrients;
            if(action1.type==ActionType::COMPLETE) --this->nutrients;
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
                        --this->shadows[d][shadings[d][dist-1]];
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
                    ++this->shadows[d][shadings[d][dist]];
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

    double evaluate(bool print=false){
        if(this->day>=24){
            double s = this->score[0]+this->sun[0]/3-this->score[1]-this->sun[1]/3;
            if(s>0) return 1;
            else if(s<0) return 0;
            else{
                int n_trees[2] = {};
                for(int i=0; i<37; ++i){
                    auto& cell = this->get_cell(i);
                    if(cell.tree_size>=0){
                        ++n_trees[1-(int)cell.is_mine];
                    }
                }
                int d = n_trees[0] - n_trees[1];
                return d>0? 1 : d<0? 0 : 0.5;
            }
        }
        const int grow_costs[3] = {1, 3, 7};
        bool shaded[37][6] = {};
        int n_shaded[37][2] = {};
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0){
                int t = max((int)cell.tree_size, 1);
                for(int sd=0; sd<6; ++sd){
                    auto& shadings = Board::shadings[idx][sd];
                    int n = min(t, (int)shadings.size());
                    for(int d=0; d<n; ++d){
                        if(!shaded[shadings[d]][sd]) ++n_shaded[shadings[d]][1-(int)cell.is_mine];
                        shaded[shadings[d]][sd] = true;
                    }
                }
            }
        }
        double total_score = 0;
        if(this->day<9){
            double point_score = (this->score[0]-this->score[1]) * (0.25+pow((double)this->day/23, 4)) * pow(1+this->nutrients/20.0, 0.3);
            double sun_score = (this->sun[0]-this->sun[1]) * 0.3;
            double tree_score = 0;
            double ts_weight[4] = {0.35, 1.0, 2.5, 3*(1+this->day/16.0)};
            static double pos_bias[37] = {
                8, 5, 5, 5, 5, 5, 5,
                2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2
            };
            int n_trees[2][4] = {};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=0){
                    tree_score += (cell.is_mine? 1 : -1) * pow(ts_weight[cell.tree_size], 0.7) * pow(24-(int)this->day, 0.7) * pow((6-n_shaded[idx][1-(int)cell.is_mine])/6.0, 4)*pow(cell.richness, 0.25) * pow(pos_bias[idx], 0.6);
                    ++n_trees[1-(int)cell.is_mine][cell.tree_size];
                }
            }
            double dup_penalty = max(0, n_trees[0][0]-1)*6*pow(1+this->day/24.0, 0.5) + max(0, n_trees[0][1]-1)*2 + max(0, n_trees[0][2]-1)*0.5 + n_trees[0][3]*0 - max(0, n_trees[1][0]-1)*6*pow(1+this->day/24.0, 0.5) - max(0, n_trees[1][1]-1)*2 - max(0, n_trees[1][2]-1)*0.5 - n_trees[1][3]*0;
            //if(print) cerr << point_score << " " << sun_score << " " << tree_score << " " << dup_penalty << " ";
            total_score = point_score + sun_score + tree_score - dup_penalty;
            total_score = 1.0 / (1.0 + exp(-total_score*0.04));
        }
        else if(this->day<16){
            double point_score = (this->score[0]-this->score[1]) * (0.15+pow((double)this->day/23, 4)) * pow(1+this->nutrients/24.0, 0.3);
            double sun_score = (this->sun[0]-this->sun[1]) * 0.25;
            double tree_score = 0;
            double ts_weight[4] = {0.2, 1, 2.2, 2.8*(1+this->day/24.0)};
            static double pos_bias[37] = {
                10, 6, 6, 6, 6, 6, 6,
                2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5,
                1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2
            };
            int n_trees[2][4] = {};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=0){
                    tree_score += (cell.is_mine? 1 : -1) * pow(ts_weight[cell.tree_size], 0.5) * pow(24-(int)this->day, 0.75) * pow((6-n_shaded[idx][1-(int)cell.is_mine])/6.0, 4)*pow(cell.richness, 0.18*(1-this->nutrients/160.0)) * pow(pos_bias[idx], 0.5);
                    ++n_trees[1-(int)cell.is_mine][cell.tree_size];
                }
            }
            double dup_penalty = max(0, n_trees[0][0]-1)*5*pow(1+this->day/24.0, 0.5) + max(0, n_trees[0][1]-1)*2 + max(0, n_trees[0][2]-1)*0.5 + n_trees[0][3]*0 - max(0, n_trees[1][0]-1)*5*pow(1+this->day/24.0, 0.5) - max(0, n_trees[1][1]-1)*2 - max(0, n_trees[1][2]-1)*0.5 - n_trees[1][3]*0;
            //if(print) cerr << point_score << " " << sun_score << " " << tree_score << " " << dup_penalty << " ";
            total_score = point_score + sun_score + tree_score - dup_penalty;
            total_score = 1.0 / (1.0 + exp(-total_score*0.04));
        }
        else{
            double point_score = (this->score[0]-this->score[1]) * (0.55+pow((double)this->day/23, 4)) * pow(1+this->nutrients/20.0, 0.4);
            double sun_score = (this->sun[0]-this->sun[1]) * 0.25;
            double tree_score = 0;
            double ts_weight[4] = {0.2, 1, 2.2, 3*(1+this->day/24.0)};
            static double pos_bias[37] = {
                6, 6, 6, 6, 6, 6, 6, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 2, 2.5, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2, 1, 1.2, 1.2
            };
            int n_trees[2][4] = {};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=0){
                    tree_score += (cell.is_mine? 1 : -1) * pow(ts_weight[cell.tree_size], 0.5) * pow(24-(int)this->day, 0.75) * pow((6-n_shaded[idx][1-(int)cell.is_mine])/6.0, 3)*pow(cell.richness, 0.175) * pow(pos_bias[idx], 0.5);
                    ++n_trees[1-(int)cell.is_mine][cell.tree_size];
                }
            }
            double dup_penalty = max(0, n_trees[0][0]-1)*5*pow(1+this->day/24.0, 0.5) + max(0, n_trees[0][1]-1)*2 + max(0, n_trees[0][2]-1)*0.5 + n_trees[0][3]*0 - max(0, n_trees[1][0]-1)*5*pow(1+this->day/24.0, 0.5) - max(0, n_trees[1][1]-1)*2 - max(0, n_trees[1][2]-1)*0.5 - n_trees[1][3]*0;
            //if(print) cerr << point_score << " " << sun_score << " " << tree_score << " " << dup_penalty << " ";
            total_score = point_score + sun_score + tree_score - dup_penalty;
            total_score = 1.0 / (1.0 + exp(-total_score*0.05));
        }

        return total_score;
    }

    double evaluate(Action& action){
        Board next_board = *this;
        next_board.update(action);
        action.score = next_board.evaluate();
        return action.score;
    }

    Action get_best_action(){
        MCT* mct = new MCT(*this, true);
        Action best = mct->get_best_action();
        best.score = mct->ev;
        delete mct;
        return best;
    }

    void print(){
        cerr << endl;
        cerr << "b.cpp" << endl;
        cerr << "Day" << (int)this->day << ": sun_dir=" << (int)this->sun_dir << ", nutrients=" <<  (int)this->nutrients << endl;
        cerr << "P1: " << this->score[1] << "(" << this->sun[1] << "), " << (this->is_waiting[1]? "waiting" : "active") << endl;
        cerr << "P2: " << this->score[0] << "(" << this->sun[0] << "), " << (this->is_waiting[0]? "waiting" : "active") << endl;
        for(int y=0; y<=6; ++y){
            for(int x=0; x<=12; ++x){
                if((x+y)%2==1 && this->is_inside(y, x)){
                    int idx = this->yx_to_id[y][x];
                    auto& cell = this->get_cell(idx);
                    if(cell.tree_size>=0){
                        if(cell.dormant_day==this->day) cerr << "\033[4m";
                        if(!cell.is_mine) cerr << "\033[32m";
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

inline MCT::MCT(Board& board, bool init){
    this->board = new Board(board);
    pnodes = cnodes = 0;
    ev = 0;
    bv = 0;
    if(init) MCT::start_time = chrono::system_clock::now();
}

MCT::~MCT(){
    delete this->board;
    for(int i=0; i<this->actions[0].size(); ++i){
        for(int j=0; j<this->actions[1].size(); ++j){
            delete this->children[i][j];
        }
    }
}

chrono::system_clock::time_point MCT::start_time;

void MCT::expand(bool root){
    if(this->board->day==23){
        this->actions[0] = this->board->get_actions(0, 2);
        this->actions[1] = this->board->get_actions(1, 2);
    }
    else if(this->board->day==22){
        this->actions[0] = this->board->get_actions(0, 4);
        this->actions[1] = this->board->get_actions(1, 4);
    }
    else if(this->board->day==21){
        this->actions[0] = this->board->get_actions(0, 8);
        this->actions[1] = this->board->get_actions(1, 8);
    }
    else{
        if(root){
            this->actions[0] = this->board->get_actions(0, 48);
            this->actions[1] = this->board->get_actions(1, 48);
        }
        else{
            this->actions[0] = this->board->get_actions(0, 16);
            this->actions[1] = this->board->get_actions(1, 16);
        }
    }
    int n0 = this->actions[0].size();
    int n1 = this->actions[1].size();
    this->children = vector<vector<MCT*>>(n0, vector<MCT*>(n1, nullptr));
    for(int i=0; i<n0; ++i){
        for(int j=0; j<n1; ++j){
            auto* c = this->children[i][j] = new MCT(*this->board);
            c->board->update(this->actions[0][i], this->actions[1][j]);
            c->bv = c->ev = c->board->evaluate();
            c->pnodes = 0;
            c->cnodes = 0;
        }
    }
    this->indices[0] = vector<int>(n0);
    for(int i=0; i<n0; ++i){
        this->indices[0][i] = i;
    }
    this->indices[1] = vector<int>(n1);
    for(int i=0; i<n1; ++i){
        this->indices[1][i] = i;
    }
}

Action MCT::get_best_action(){
    this->search(10000);
    vector<int> n(this->actions[0].size(), 0);
    vector<double> ev(this->actions[0].size(), 0);
    for(int i=0; i<this->actions[0].size(); ++i){
        for(int j=0; j<this->actions[1].size(); ++j){
            auto child = this->children[i][j];
            int nodes = child->pnodes + child->cnodes;
            n[i] += nodes;
            ev[i] += child->ev * nodes;
        }
        ev[i] /= max(n[i], 1);
    }
    auto it = max_element(this->indices[0].begin(), this->indices[0].end(), [&](int l, int r){ return n[l]<n[r]; });
    this->ev = ev[*it];
    return this->actions[0][*it];
}

void MCT::search(int max_nodes, int depth){
    //cerr << "depth: " << depth << endl;
    if(this->board->day>=24){
        int s = this->board->score[0]+this->board->sun[0]/3 - this->board->score[1]-this->board->sun[1]/3;
        if(s>0) this->bv = this->ev = 1+(double)s/10000;
        else if(s<0) this->bv = this->ev = 0+(double)s/10000;
        else{
            int d = 0;
            for(int i=0; i<37; ++i){
                auto& cell = this->board->get_cell(i);
                if(cell.tree_size>=0){
                    if(cell.is_mine) ++d;
                    else --d;
                }
            }
            this->bv = this->ev = (d>0? 1 : d==0? 0.5 : 0);
        }
        if(depth==0 && this->pnodes==0) this->expand(true);
        ++this->pnodes;
        return;
    }
    if(this->pnodes==0){
        if(depth==0) this->bv = this->ev = this->board->evaluate();
        --max_nodes;
        this->pnodes = 1;
    }
    if(max_nodes>0 && this->children.size()==0) this->expand();
    for(int i=0; i<max_nodes; ++i){
        if(depth==0){
            auto current_time = chrono::system_clock::now();
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time-MCT::start_time).count();
            if(first && elapsed>=800){
                return;
            }
            if(!first && elapsed>=80){
                return;
            }
        }
        vector<double> evs[2] = {vector<double>(this->actions[0].size(), 0), vector<double>(this->actions[1].size(), 0)};
        vector<int> n[2] = {vector<int>(this->actions[0].size(), 0), vector<int>(this->actions[1].size(), 0)};
        for(int j=0; j<this->actions[0].size(); ++j){
            for(int k=0; k<this->actions[1].size(); ++k){
                auto child = this->children[j][k];
                int m = max(1, child->pnodes+child->cnodes);
                evs[0][j] += child->ev*m;
                evs[1][k] += child->ev*m;
                n[0][j] += m;
                n[1][k] += m;
            }
        }
        //double lambda = 0.1 * pow(0.3, depth+1);
        double lambda = 0.1;// * pow(1.6, depth);
        //if(this->board->day>=20){
            //lambda *= pow(2, this->board->day-19);
        //}
        auto it0 = max_element(this->indices[0].begin(), this->indices[0].end(), [&](int l, int r){
            double pl = evs[0][l]/max(1, n[0][l]) + lambda * sqrt(log((double)(this->pnodes+this->cnodes))/max(1, n[0][l]));
            double pr = evs[0][r]/max(1, n[0][r]) + lambda * sqrt(log((double)(this->pnodes+this->cnodes))/max(1, n[0][r]));
            int nodes = this->pnodes + this->cnodes;
            cerr << evs[0][l]/max(1, n[0][l]) << " " << lambda*sqrt(log(nodes)/max(1, n[0][l])) << endl;
            return pl<pr? true : pl>pr? false : l<r;
        });
        auto it1 = max_element(this->indices[1].begin(), this->indices[1].end(), [&](int l, int r){
            double pl = 1-evs[1][l]/max(1, n[1][l]) + lambda * sqrt(log((double)(this->pnodes+this->cnodes))/max(1, n[1][l]));
            double pr = 1-evs[1][r]/max(1, n[1][r]) + lambda * sqrt(log((double)(this->pnodes+this->cnodes))/max(1, n[1][r]));
            return pl<pr? true : pl>pr? false : l<r;
        });
        int l = *it0;
        int r = *it1;
        this->children[l][r]->search(1, depth+1);
        ++this->cnodes;
        this->ev = this->bv*this->pnodes;
        // TODO speed up
        int m = 0;
        for(int j=0; j<this->actions[0].size(); ++j){
            for(int k=0; k<this->actions[1].size(); ++k){
                auto& child = this->children[j][k];
                int nodes = child->pnodes + child->cnodes;
                this->ev += child->ev * nodes;
                m += nodes;
            }
        }
        this->ev /= (this->pnodes+this->cnodes);
    }
}


int main()
{
    start_time = chrono::system_clock::now();
    cin.ignore();
    Board board = Board();
    //cin.tie(nullptr);
    //ios_base::sync_with_stdio(false);
    int numberOfCells; // 37
    cin >> numberOfCells; cin.ignore();
    for (int i = 0; i < numberOfCells; i++) {
        int index, richness, neigh;
        cin >> index >> richness >> neigh >> neigh >> neigh >> neigh >> neigh >> neigh; cin.ignore();
        board.get_cell(index).richness = richness;
    }

    first = true;
    string last_action;
    Action last_best(ActionType::WAIT, 0);
    while (1) {
        if(!first) start_time = chrono::system_clock::now();
        int day, nutrients, score[2], sun[2];
        cin >> day; cin.ignore();
        if(day>=24) return 0;
        cin >> nutrients; cin.ignore();
        board.nutrients = nutrients;
        cin >> sun[0] >> score[0]; cin.ignore();
        cin >> sun[1] >> score[1] >> board.is_waiting[1]; cin.ignore();
        stringstream ss;
        if(day!=board.day){
            board.is_waiting[0] = false;
        }
        board.day = day;
        board.sun_dir = board.day%6;
        int numberOfTrees; // the current amount of trees
        cin >> numberOfTrees; cin.ignore();
        for(int i=0; i<37; ++i){
            auto& cell = board.get_cell(i);
            cell.tree_size = -1;
        }
        for (int i = 0; i < numberOfTrees; i++) {
            int cell_index; // location of this tree
            cin >> cell_index;
            auto& cell = board.get_cell(cell_index);
            int tree_size;
            bool is_mine, is_dormant;
            cin >> tree_size >> is_mine >> is_dormant; cin.ignore();
            cell.tree_size = tree_size;
            cell.is_mine = is_mine;
            cell.is_dormant = is_dormant;
            if(is_dormant) cell.dormant_day = board.day;
            else cell.dormant_day = board.day-1;
            if(is_dormant) cell.dormant_day = day;
            else cell.dormant_day = day-1;
        }
        board.sun[0] = sun[0];
        board.sun[1] = sun[1];
        board.score[0] = score[0];
        board.score[1] = score[1];
        board.print();
        cerr << last_action << endl;

        int numberOfPossibleActions; // all legal actions
        cin >> numberOfPossibleActions; cin.ignore();
        for (int i = 0; i < numberOfPossibleActions; i++) {
            string possibleAction;
            getline(cin, possibleAction);
        }
        stringstream ss2;
        if(board.is_waiting[0]){
            cout << "WAIT" << endl;
            continue;
        }
        board.init_shadows();
        auto best = board.get_best_action();

        if(best.type==ActionType::WAIT) cout << "WAIT" << endl;
        else if(best.type==ActionType::SEED) cout << "SEED " << (int)best.idx << " " << (int)best.seed_idx << endl;
        else if(best.type==ActionType::GROW) cout << "GROW " << (int)best.idx << endl;
        else cout << "COMPLETE " << (int)best.idx << endl;
        first = false;

        current_time = chrono::system_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time-start_time).count();
        if(best.type==ActionType::WAIT) ss2 << "B: WAIT @" << elapsed << "msec (" << best.score << "," << board.evaluate(true) << ")";
        else if(best.type==ActionType::SEED) ss2 << "B: SEED " << (int)best.idx << " " << (int)best.seed_idx << " @" << elapsed << "msec (" << best.score << "," << board.evaluate(true) << ")";
        else if(best.type==ActionType::GROW) ss2 << "B: GROW " << (int)best.idx << " @" << elapsed << "msec (" << best.score << "," << board.evaluate(true) << ")";
        else ss2 << "B: COMPLETE " << (int)best.idx << " @" << elapsed << "msec (" << best.score << "," << board.evaluate(true) << ")";
        last_action = ss2.str();
    }
}
