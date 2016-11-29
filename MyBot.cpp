#include <cstdlib>
#include <ctime>
#include <vector>
#include <unordered_map>
#include <iterator>
#include <fstream>
#include <limits>

#include "hlt.hpp"
#include "networking.hpp"


/*void addMove(std::unordered_map<hlt::Location, int>& moves, std::vector<std::vector<unsigned short> >) {
    
}*/

int main() {
    srand(time(NULL));

    std::cout.sync_with_stdio(0);
   
    /*std::ofstream logfile("log.txt", std::ios::out); 
    logfile.precision(3);*/

    unsigned char myID;

    hlt::GameMap map;
    getInit(myID, map);

    // init stuff goes here
    //std::vector<std::vector<float> > productionRatio(map.height, std::vector<float>(map.width));

    sendInit("Infestron");

    std::unordered_map<hlt::Location, int> moves;
    std::vector<hlt::Location> borders;
    std::vector<std::vector<unsigned char> > bordersPID;

    //std::vector<std::vector<unsigned short> > postMovesStrength(map.height, std::vector<unsigned short>(map.width));
    std::unordered_map<hlt::Location, unsigned int> postMovesStrength;

    while (true) {
        moves.clear();
        borders.clear();
        bordersPID.clear();
        postMovesStrength.clear();

        getFrame(map);

        // scan
        //logfile << "PROD RATIO=" << std::endl;
        for (unsigned short y = 0; y < map.height; y++) {
            for (unsigned short x = 0; x < map.width; x++) {
                const hlt::Site& s = map.getSite({x,y});
                if (s.owner == myID) {
                    bool isBorder = false;
                    for (const auto d : CARDINALS) {
                        const hlt::Site& neighbor = map.getSite({x, y}, d);
                        if (neighbor.owner != myID) {
                            if (!isBorder) {
                                borders.emplace_back(x, y);
                                bordersPID.emplace_back(std::vector<unsigned char>(1,neighbor.owner));
                                isBorder = true;
                            }
                            else bordersPID.back().push_back(neighbor.owner);
                        }
                    }
                }

                //productionRatio[y][x] = s.strength ? float(s.production) / s.strength : float(s.production);
                //logfile << productionRatio[y][x] << " ";

                postMovesStrength[{x,y}] = s.owner == myID ? s.strength : -s.strength;
            }
            //logfile << std::endl;
        }

        // move
        for (unsigned short y = 0; y < map.height; y++) {
            for (unsigned short x = 0; x < map.width; x++) {
                const hlt::Location loc {x, y};
                const hlt::Site& site = map.getSite(loc);

                if (map.getSite(loc).owner == myID) {

                    bool movedPiece = false;

                    unsigned int myStrength = site.strength;
                    
                    // attack
                    if (std::find(borders.begin(), borders.end(), loc) != borders.end())
                    {
                        float strenghtFactor = .67f;
                        if (strenghtFactor*myStrength >= 255) strenghtFactor = 1.f;

                        float maxStrengthDiff = -std::numeric_limits<float>::max();
                        int attackDirection = STILL;
                        bool overkill = false;
                        for (auto d : CARDINALS) {
                            const hlt::Location attackLoc = map.getLocation(loc, d);
                            const hlt::Site& attackSite = map.getSite(attackLoc);
                            if (attackSite.owner != myID) {
                                float requiredStrength = attackSite.owner != 0 ? myStrength*strenghtFactor : myStrength;
                                unsigned int damage = map.computeMoveDamage(loc, attackLoc);
                                if (damage > myStrength) {
                                    if (!overkill) maxStrengthDiff = 0;
                                    float diff = (damage - myStrength);// * attackSite.production;
                                    if (diff > maxStrengthDiff) {
                                        maxStrengthDiff = diff;
                                        attackDirection = d;
                                        overkill = true;
                                    }
                                }
                                float strengthDiff = damage;
                                if (site.strength && !damage) strengthDiff += 0.01f; // ensure the piece always attacks if it has some strength
                                if (attackSite.strength) strengthDiff = (requiredStrength - attackSite.strength);
                                strengthDiff *= attackSite.production ? attackSite.production : 0.01f; // prevent having 0 diff if prod == 0, but count prod == 0 much lower than prod == 1
                                if (!overkill && strengthDiff > 0) {
                                    if (strengthDiff > maxStrengthDiff)
                                    {
                                        maxStrengthDiff = strengthDiff;
                                        attackDirection = d;
                                    }
                                }
                            }
                        }
                        if (attackDirection == STILL) {
                            moves.emplace(loc, STILL);
                            postMovesStrength[loc] += site.production;
                        } else {
                            const hlt::Location attackLoc = map.getLocation(loc, attackDirection);
                            if (myStrength + site.production <= 255 && myStrength + postMovesStrength[attackLoc] > 255) {
                                moves.emplace(loc, STILL);
                                postMovesStrength[loc] += site.production;
                            } else {
                                moves.emplace(loc, attackDirection);
                                postMovesStrength[attackLoc] += myStrength;
                            }
                        }
                        movedPiece = true;
                    }

                    // grow
                    if (!movedPiece && myStrength < site.production * 5) {
                        moves.emplace(loc, STILL);
                        movedPiece = true;
                        postMovesStrength[loc] += site.production;
                    }

                    // reinforce
                    if (!movedPiece) {
                        float bestBorderValue = -std::numeric_limits<float>::max();
                        hlt::Location bestBorderLoc;
                        for (std::size_t i = 0; i < borders.size(); ++i) {
                            const hlt::Location l = borders[i];
                            unsigned int dist = map.getDistance(loc, l);
                            float val = myStrength < 200 ? 1.f * map.getSite(l).production / dist : 1.f / dist;
                            if (dist == 1) {
                                val *= bordersPID[i].size();
                            }
                            if (val > bestBorderValue) {
                                bestBorderValue = val;
                                bestBorderLoc = l;
                            }
                        }

                        std::pair<int,int> directions = map.getDirections(loc, bestBorderLoc);
                        unsigned char d = directions.first;
                        hlt::Location nextLoc = map.getLocation(loc, d);
                        // avoid loosing strength
                        if (myStrength + postMovesStrength[nextLoc] > 255) {
                            unsigned char d2 = directions.second;
                            if (d2 != STILL) {
                                nextLoc = map.getLocation(loc, d2);
                                if (myStrength + site.production <= 255 && myStrength + postMovesStrength[nextLoc] > 255) {
                                    moves.emplace(loc, STILL);
                                    postMovesStrength[loc] += site.production;
                                } else {
                                    moves.emplace(loc, d2);
                                    postMovesStrength[nextLoc] += myStrength;
                                }
                            } else {
                                if (myStrength + site.production <= 255) {
                                    moves.emplace(loc, STILL);
                                    postMovesStrength[loc] += site.production;
                                } else {
                                    moves.emplace(loc, d);
                                    postMovesStrength[nextLoc] += myStrength;
                                }
                            }
                        } else {
                            moves.emplace(loc, d);
                            postMovesStrength[nextLoc] += myStrength;
                        }
                    }
                }
            }
        }

        // force push blocking pieces forward
        for (std::pair<const hlt::Location, int>& move : moves) {
            int dir = move.second;
            if (dir == STILL) continue;
            hlt::Location curLoc = move.first, nextLoc = map.getLocation(move.first, dir);
            hlt::Site curSite = map.getSite(curLoc), nextSite = map.getSite(nextLoc);
            std::size_t nbIter = 0;
            while (nextSite.owner == myID && moves[nextLoc] == STILL && curSite.strength + nextSite.strength > 255 && nbIter < 1000) {
                moves[nextLoc] = dir;
                curLoc = nextLoc;
                nextLoc = map.getLocation(nextLoc, dir);
                curSite = nextSite;
                nextSite = map.getSite(nextLoc);
                nbIter++;
            }
        }

        /*logfile << "MOVES=";
        std::copy(moves.begin(), moves.end(), std::ostream_iterator<hlt::Move>(logfile, " "));
        logfile << std::endl;*/
        sendFrame(moves);
    }

    return 0;
}
