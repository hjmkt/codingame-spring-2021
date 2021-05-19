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
        //if(!this->shadows_initialized){
            for(int i=0; i<37; ++i){
                auto& cell = this->get_cell(i);
                if(cell.tree_size>0){
                    for(int j=0; j<6; ++j){
                        auto& shadings = this->shadings[i][j];
                        if(shadings.size()>0) this->shadows[j][shadings[0]]++;
                    }
                }
            }
            //this->shadows_initialized = true;
        //}
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

    inline Cell& get_cell(int idx){ return this->cells[idx]; }

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
        else{ return 4; }
    }

    vector<Action> get_actions(int player=0){
        char n_trees[4] = {};
        const int grow_costs[3] = {1, 3, 7};
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0 && cell.is_mine){
                ++n_trees[cell.tree_size];
            }
        }

        vector<Action> actions = {Action(ActionType::WAIT, 0)};
        // COMPLETE
        if(this->sun[player]>=4){
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size==3 && cell.is_mine && cell.dormant_day!=this->day){
                    actions.push_back(Action(ActionType::COMPLETE, 4, idx));
                }
            }
        }
        // GROW
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0 && cell.tree_size<=2 && cell.is_mine && cell.dormant_day!=this->day){
                int cost = grow_costs[cell.tree_size]+n_trees[cell.tree_size+1];
                if(this->sun[player]>=cost){
                    actions.push_back(Action(ActionType::GROW, cost, idx));
                }
            }
        }
        // SEED
        int cost = n_trees[0];
        if(this->sun[player]>=cost){
            int seeded[37] = {};
            for(int idx=0; idx<37; ++idx){
                auto& cell = this->get_cell(idx);
                if(cell.tree_size>=1 && cell.is_mine && cell.dormant_day!=this->day){
                    for(int d=0; d<cell.tree_size; ++d){
                        auto& neighbors = Board::neighbors[idx][d];
                        for(char n : neighbors){
                            if(seeded[n]>=4) continue;
                            auto cell = this->get_cell(n);
                            if(cell.tree_size<0 && cell.richness>0){
                                actions.push_back(Action(ActionType::SEED, cost, idx, n));
                                ++seeded[n];
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
            //this->day += 1;
            this->is_waiting[player] = true;
        }
        this->sun[player] -= action.cost;
        this->sun_dir = this->day % 6;
    }

    double evaluate(int step=2){
        if(this->day>=22){
            int n_trees[2] = {};
            for(int i=0; i<37; ++i){
                auto& cell = this->get_cell(i);
                if(cell.tree_size>=0){
                    if(cell.is_mine) ++n_trees[0];
                    else ++n_trees[1];
                }
            }
            int d = n_trees[0] - n_trees[1];
            //int d = 0;
            return this->score[0]+this->sun[0]/3-this->score[1]-this->sun[1]/3 + d/37.0;
        }
        const int grow_costs[3] = {1, 3, 7};
        int shaded[37] = {};
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=1){
                auto& shadings = Board::shadings[idx][this->sun_dir];
                int n = min((int)cell.tree_size, (int)shadings.size());
                for(int d=0; d<n; ++d){
                    shaded[shadings[d]]++;
                }
            }
        }
        double sun_score = this->sun[0] - this->sun[1];
        double n_trees[2][4] = {};
        for(int idx=0; idx<37; ++idx){
            auto& cell = this->get_cell(idx);
            if(cell.tree_size>=0 && cell.is_mine) n_trees[0][cell.tree_size] += 1.0/pow(1+shaded[idx], 0.6) * pow(cell.richness, 0.7);
            if(cell.tree_size>=0 && !cell.is_mine) n_trees[1][cell.tree_size] += 1.0/pow(1+shaded[idx], 0.6) * pow(cell.richness, 0.7);
        }
        double tree_score = n_trees[0][0]+n_trees[0][1]*3+n_trees[0][2]*8+n_trees[0][3]*15 - n_trees[1][0]-n_trees[1][1]*3-n_trees[1][2]*8-n_trees[1][3]*15;
        double score = this->score[0] - this->score[1];
        //cerr << score << " " << sun_score << " " << tree_score << endl;
        double total_score = score*(this->day*this->day/190.0) + sun_score*(1.0-this->day/25)*0.95 + tree_score*(2.5-this->day/15.0);
        return total_score;
    }

    double evaluate(Action& action){
        Board next_board = *this;
        next_board.update(action);
        action.score = next_board.evaluate();
        return action.score;
    }

    Action get_best_action(){
        auto actions = this->get_actions();
        int best_idx = -1;
        double best_score = -100000000;

        if(actions.size()==1){
            best_idx = 0;
            best_score = this->evaluate(actions[0]);
            //cerr << "wait" << endl;
        }
        else if(this->day>=20){
            for(int i=0; i<actions.size(); ++i){
                auto& action = actions[i];
                Board next_board = *this;
                double score = next_board.evaluate(action);
                //cerr << "single " << actions.size() << " " << i << endl;
                if(score>best_score){
                    best_score = score;
                    best_idx = i;
                }
            }
        }
        else{
            Board next = *this;
            next.update(Action(ActionType::WAIT, 0));
            ++next.day;
            next.sun_dir = next.day%6;
            next.update_sun();
            auto ex_actions = next.get_actions();
            //cerr << "Actions " << ex_actions.size() << endl;
            for(int i=1; i<ex_actions.size(); ++i){
                auto& action = ex_actions[i];
                //cerr << "type " << action.type << endl;
                double score = next.evaluate(action);
                //cerr << "ex " << ex_actions.size() << " " << i << " " << action.type << endl;
                if(score>best_score){
                    best_score = score;
                    best_idx = i;
                }
            }
            /*
            for(auto& action : ex_actions){
                cerr << action.type << " " << (int)action.idx << " " << (int)action.seed_idx << " " << action.score << " " << action.cost << endl;
            }*/
            auto& tmp_best = ex_actions[best_idx];
            bool found = false;
            for(int i=0; i<actions.size(); ++i){
                Action& action = actions[i];
                if(action==tmp_best){
                    found = true;
                    best_idx = i;
                    action.score = tmp_best.score;
                    break;
                }
            }
            if(!found){
                best_idx = 0;
                actions[0].score = tmp_best.score;
            }
        }
        //cerr << "best decided " << actions.size() << " " << best_idx << " " << best_score << endl;
        auto best = actions[best_idx];
        return best;
    }

    void print(){
        cerr << endl;
        cerr << "a.cpp" << endl;
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

int main()
{
    auto start = chrono::system_clock::now();
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

    bool first = true;
    string last_action;
    Action last_best(ActionType::WAIT, 0);
    Board prev = board;
    while (1) {
        if(!first) start = chrono::system_clock::now();
        int day, nutrients, score[2], sun[2];
        cin >> day; cin.ignore();
        if(day>=24) return 0;
        cin >> nutrients; cin.ignore();
        board.nutrients = nutrients;
        cin >> sun[0] >> score[0]; cin.ignore();
        cin >> sun[1] >> score[1] >> board.is_waiting[1]; cin.ignore();
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
        board.init_shadows();
        int numberOfPossibleActions; // all legal actions
        cin >> numberOfPossibleActions; cin.ignore();
        for (int i = 0; i < numberOfPossibleActions; i++) {
            string possibleAction;
            getline(cin, possibleAction);
        }
        prev = board;
        stringstream ss2;
        if(board.is_waiting[0]){
            auto end = chrono::system_clock::now();
            double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
            start = chrono::system_clock::now();
            cout << "WAIT" << endl;
            ss2 << "A: WAIT @" << elapsed << "msec";
            last_action = ss2.str();
            continue;
        }
        auto best = board.get_best_action();
        board.update(best);
        last_best = best;

        if(best.type==ActionType::WAIT) cout << "WAIT" << endl;
        else if(best.type==ActionType::SEED) cout << "SEED " << (int)best.idx << " " << (int)best.seed_idx << endl;
        else if(best.type==ActionType::GROW) cout << "GROW " << (int)best.idx << endl;
        else cout << "COMPLETE " << (int)best.idx << endl;
        first = false;

        auto end = chrono::system_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        if(best.type==ActionType::WAIT) ss2 << "A: WAIT @" << elapsed << "msec (" << best.score << ")";
        else if(best.type==ActionType::SEED) ss2 << "A: SEED " << (int)best.idx << " " << (int)best.seed_idx << " @" << elapsed << "msec (" << best.score << ")";
        else if(best.type==ActionType::GROW) ss2 << "A: GROW " << (int)best.idx << " @" << elapsed << "msec (" << best.score << ")";
        else ss2 << "A: COMPLETE " << (int)best.idx << " @" << elapsed << "msec (" << best.score << ")";
        last_action = ss2.str();
    }
}
