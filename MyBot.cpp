#include <cstdlib>
#include <ctime>
#include <vector>
#include <unordered_map>
#include <iterator>
#include <fstream>
#include <limits>

#include "hlt.hpp"
#include "networking.hpp"


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
    std::vector<unsigned int> borderWithEnemy;

    std::unordered_map<hlt::Location, float> borderAttackValue;
    std::unordered_map<hlt::Location, float> enemyBordersValue;

    std::unordered_map<hlt::Location, int> postMovesStrength;

    //std::unordered_map<hlt::Location, std::vector<hlt::Location> > possibleMoves;

    while (true) {
        moves.clear();
        borders.clear();
        borderWithEnemy.clear();
        borderAttackValue.clear();
        enemyBordersValue.clear();
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
                                isBorder = true;
                                borderWithEnemy.push_back(neighbor.strength == 0 ? 1 : 0);
                            } else {
                                if (neighbor.strength == 0) {
                                    borderWithEnemy.back()++;
                                }
                            }
                        }
                    }
                } else {
                    bool isEnemyBorder = false;
                    //bool isNearEnemy = false;
                    for (const auto d : CARDINALS) {
                        const hlt::Location neighborLoc = map.getLocation({x, y}, d);
                        const hlt::Site& neighborSite = map.getSite(neighborLoc);
                        if (neighborSite.owner == myID) {
                            float value = s.strength ? 1.f * s.production / s.strength : s.production * 3.f;
                            if (!isEnemyBorder) {
                                enemyBordersValue[{x,y}] = value;
                                isEnemyBorder = true;
                            }
                            borderAttackValue[neighborLoc] = std::max(borderAttackValue[neighborLoc], value);
                        }/* else if (neighborSite.owner > 0 || neighborSite.strength == 0) {
                            isNearEnemy = true;
                        }
                    }
                    if (isEnemyBorder) {
                        if (s.owner == 0 && s.strength > 0 && isNearEnemy) {
                           enemyBordersValue[{x,y}] = 0.f;
                       }
                       for (const auto d : CARDINALS) {
                           const hlt::Location neighborLoc = map.getLocation({x, y}, d);
                           const hlt::Site& neighborSite = map.getSite(neighborLoc);
                           if (neighborSite.owner == myID) {
                                borderAttackValue[neighborLoc] = std::max(borderAttackValue[neighborLoc], enemyBordersValue[{x,y}]);
                            }
                       }*/
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

                    unsigned int myStrength = site.strength;
                    if (myStrength == 0) {
                        moves.emplace(loc, STILL);
                        postMovesStrength[loc] += site.production;
                    }


                    // attack
                    auto itBorder = std::find(borders.begin(), borders.end(), loc);
                    if (itBorder != borders.end())
                    {
                        // should try to attack or reinforce other borders?
                        std::size_t id = std::distance(borders.begin(), itBorder);
                        bool attackEnemy = borderWithEnemy[id];
                        int attackDirection = STILL;
                        
                        if (!attackEnemy) { // TODO call reinforcement code
                            // find best border by attack value
                            hlt::Location bestBorderLoc;
                            float maxAttackValue = 0.f;
                            for (std::size_t i = 0; i < borders.size(); ++i) {
                                const hlt::Location l = borders[i];
                                unsigned int dist = map.getDistance(loc, l);
                                float attackValue = dist ? borderAttackValue[l] / dist : borderAttackValue[l];
                                if (dist <= 5 && attackValue > maxAttackValue) { // TODO: change dist condition to sum of prod on the way
                                    bestBorderLoc = l;
                                    maxAttackValue = attackValue;
                                } 
                            }

                            if (maxAttackValue > 0.f) {
                                int d = map.getDirectionInMyTerritory(loc, bestBorderLoc, myID);
                                attackDirection = d;

                                if (attackDirection != STILL) {
                                    if (site.strength < 20 || site.strength < site.production * 5) {
                                        moves.emplace(loc, STILL);
                                        postMovesStrength[loc] += site.production;
                                        continue;
                                    }
                                    const hlt::Location attackLoc = map.getLocation(loc, attackDirection);
                                    if (myStrength + postMovesStrength[attackLoc] > 255) { // prefer attacking instead of loosing strength
                                        attackDirection = STILL;
                                    }
                                }
                            }
                        }
                        
                        if (attackDirection == STILL) {
                            // find best direction to attack
                            if (attackEnemy) {
                                float maxStrengthDiff = -std::numeric_limits<float>::max();
                                bool overkill = false;
                                for (auto d : CARDINALS) {
                                    const hlt::Location attackLoc = map.getLocation(loc, d);
                                    const hlt::Site& attackSite = map.getSite(attackLoc);
                                    if (attackSite.owner != myID) {
                                        unsigned int damage = map.computeMoveDamage(loc, attackLoc, false);
                                        if (damage > myStrength) {
                                            float strengthDiff = float(damage) - myStrength;
                                            if (!overkill || strengthDiff > maxStrengthDiff) {
                                                maxStrengthDiff = strengthDiff;
                                                attackDirection = d;
                                                overkill = true;
                                            }
                                        }
                                        if (!overkill) {
                                            if (attackSite.strength) continue; // never try to attack squares that are not actually on the border
                                            float strengthDiff = damage ? damage : 0.01f; // ensure the piece always attacks
                                            strengthDiff *= attackSite.production ? attackSite.production : 0.01f; // prevent having 0 diff if prod == 0, but count prod == 0 much lower than prod == 1
                                            if (strengthDiff > maxStrengthDiff)
                                            {
                                                maxStrengthDiff = strengthDiff;
                                                attackDirection = d;
                                            }
                                        }
                                    }
                                }
                            } else {
                                float maxAttackValue = -std::numeric_limits<float>::max();
                                unsigned int attackStrengthRequired = std::numeric_limits<unsigned int>::max();
                                for (auto d : CARDINALS) {
                                    const hlt::Location attackLoc = map.getLocation(loc, d);
                                    const hlt::Site& attackSite = map.getSite(attackLoc);
                                    if (attackSite.owner != 0) continue;
                                    const float attackValue = enemyBordersValue[attackLoc];
                                    if (attackValue > maxAttackValue) {
                                        maxAttackValue = attackValue;
                                        attackDirection = d;
                                        attackStrengthRequired = attackSite.strength;
                                    }
                                }
                                if (myStrength <= attackStrengthRequired) {
                                    attackDirection = STILL; // wait until enough strength accumulated to attack
                                }
                            }
                        }

                        if (attackDirection == STILL) {
                            moves.emplace(loc, STILL);
                            postMovesStrength[loc] += site.production;
                        } else {
                            const hlt::Location attackLoc = map.getLocation(loc, attackDirection);
                            if (myStrength + postMovesStrength[attackLoc] > 255 && (myStrength + site.production <= 255 || site.production < postMovesStrength[attackLoc])) {
                                moves.emplace(loc, STILL);
                                postMovesStrength[loc] += site.production;
                            } else {
                                moves.emplace(loc, attackDirection);
                                postMovesStrength[attackLoc] += myStrength;
                            }
                        }
                        continue;
                    }



                    // grow
                    if (site.strength < 20 || myStrength < site.production * 5) {
                        moves.emplace(loc, STILL);
                        postMovesStrength[loc] += site.production;
                        continue;
                    }



                    // reinforce
                    float bestBorderValue = -std::numeric_limits<float>::max();
                    hlt::Location bestBorderLoc;
                    for (std::size_t i = 0; i < borders.size(); ++i) {
                        const hlt::Location l = borders[i];
                        unsigned int dist = map.getDistance(loc, l);
                        //float val = myStrength < 200 ? 1.f * map.getSite(l).production / dist : 1.f / dist;
                        //if (borderWithEnemy[i]) val *= 3.f;
                        float val = borderAttackValue[l] / dist;
                        if (val > bestBorderValue) {
                            bestBorderValue = val;
                            bestBorderLoc = l;
                        }
                    }

                    std::pair<int,int> directions = map.getDirections(loc, bestBorderLoc);
                    unsigned char d = directions.first;
                    hlt::Location nextLoc = map.getLocation(loc, d);
                    hlt::Site nextSite = map.getSite(nextLoc);
                    // avoid loosing strength
                    if (myStrength + postMovesStrength[nextLoc] > 255) {
                        unsigned char d2 = directions.second;
                        if (d2 != STILL) {
                            nextLoc = map.getLocation(loc, d2);
                            nextSite = map.getSite(nextLoc);
                            if (myStrength + postMovesStrength[nextLoc] > 255 && (myStrength + site.production <= 255 || site.production < postMovesStrength[nextLoc] - nextSite.strength)) {
                                moves.emplace(loc, STILL);
                                postMovesStrength[loc] += site.production;
                            } else {
                                moves.emplace(loc, d2);
                                postMovesStrength[nextLoc] += myStrength;
                            }
                        } else {
                            if (myStrength + site.production <= 255 || site.production < postMovesStrength[nextLoc] - nextSite.strength) {
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



        // force push blocking pieces forward
        // TODO: take into account pushing coming from the side
        for (std::pair<const hlt::Location, int>& move : moves) {
            int dir = move.second;
            if (dir == STILL) continue;
            hlt::Location curLoc = move.first, nextLoc = map.getLocation(move.first, dir);
            hlt::Site curSite = map.getSite(curLoc), nextSite = map.getSite(nextLoc);
            std::size_t nbIter = 0;
            while (nextSite.owner == myID && moves[nextLoc] == STILL && curSite.strength + nextSite.strength > 255 && nbIter < 100) {
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
