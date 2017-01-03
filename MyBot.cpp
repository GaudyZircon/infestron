#include <cstdlib>
#include <ctime>
#include <vector>
#include <set>
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

    unsigned int turnNr = 0u;
    while (true) {
        //logfile << "TURN " << turnNr << std::endl;

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
                            float value = s.strength ? 1.f * s.production / s.strength : 0.2f * s.production;
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




        // stores:
        // - the number of turns at which the site was affected to a border (in fact the distance to the border)
        // - the affectation of a site to a border
        std::unordered_map<hlt::Location, std::pair<unsigned int, hlt::Location> > affectations;

        // stores:
        // - the number of turns required to take the border
        // - the left-over strength when a border is taken so it can be used for other borders
        std::unordered_map<hlt::Location, std::pair<unsigned int, unsigned int> > bordersTaken;


        if (!std::any_of(borderWithEnemy.begin(), borderWithEnemy.end(), [](unsigned int i){return i > 0;})) { // switch algo when enemy bot found

            /////////////////////////////
            // AFFECT CELLS TO BORDERS //
            /////////////////////////////

            // This is done by checking next ring of sites after next ring of sites (in the bot territory) around each border
            // The borders are processed in order of their respective values
            // A border can steal a site from another one if its value is higher

            std::vector<std::pair<float, hlt::Location> > sortedBordersByValue;
            for (const std::pair<hlt::Location, float>& bav : enemyBordersValue) {
                sortedBordersByValue.emplace_back(bav.second, bav.first);
            }
            std::sort(sortedBordersByValue.begin(), sortedBordersByValue.end(), [](const std::pair<float, hlt::Location>& p1, const std::pair<float, hlt::Location>& p2) {return p1.first > p2.first;});
            /*logfile << "sortedBordersByValue = ";
            for (const auto&p : sortedBordersByValue) {
                logfile << p.first << "/" << p.second << " ";
            }
            logfile << std::endl;*/

            struct BorderStatus {
                std::vector<hlt::Location> nextLocsToVisit;
                std::set<hlt::Location> seenLocs;
                unsigned int requiredStrength;
                unsigned int affectedStrength;
                unsigned int affectedProduction;
            };

            std::unordered_map<hlt::Location, BorderStatus> bordersStats;
            for (const std::pair<float, hlt::Location>& sbv : sortedBordersByValue) {
                const hlt::Location bLoc = sbv.second;
                BorderStatus& bs = bordersStats[bLoc];
                const hlt::Site& b =  map.getSite(bLoc);
                bs.requiredStrength = b.strength ? b.strength : 255u;
                bs.nextLocsToVisit.push_back(bLoc);
            }

            unsigned int nProp = 0; // max distance to affect (prevents spending too much time for neglible improvment
            std::size_t borderIndexStart = 0; // borders indices lower than this have already enough strength or can't find more
            while (nProp < 10 && borderIndexStart < sortedBordersByValue.size()) {
                for (std::size_t bavi = borderIndexStart; bavi < sortedBordersByValue.size(); ++bavi) {
                    const std::pair<float, hlt::Location>& bav = sortedBordersByValue[bavi];
                    BorderStatus& bs = bordersStats[bav.second];

                    if (bs.affectedStrength > bs.requiredStrength) continue;

                    std::vector<hlt::Location> curBordersToAttack = {bav.second};
                    std::vector<hlt::Location> nextBordersToAttack;

                    std::vector<hlt::Location> neighborLocs;
                    neighborLocs.reserve(3 * bs.nextLocsToVisit.size()); // reserve worst case

                    for (hlt::Location loc : bs.nextLocsToVisit) {
                        for (auto d : CARDINALS) {
                            const hlt::Location nextLoc = map.getLocation(loc, d);
                            const hlt::Site& nextSite = map.getSite(nextLoc);
                            if ((nextSite.owner == myID || (bordersTaken.find(nextLoc) != bordersTaken.end() && bordersTaken[nextLoc].first < nProp))
                                    && bs.seenLocs.find(nextLoc) == bs.seenLocs.end()) {
                                neighborLocs.push_back(nextLoc);
                            }
                        }
                    }
                    std::swap(bs.nextLocsToVisit, neighborLocs);

                    for (hlt::Location nextLoc : bs.nextLocsToVisit) {
                        // TODO should we already sum production to affectedStrength in this loop?
                        // TODO should we order the neighbors to visit by their strength to reduce the number of affectations?
                        if (affectations.find(nextLoc) == affectations.end()) {
                            const hlt::Site& nextSite = map.getSite(nextLoc);
                            unsigned int nextSiteStrength = bordersTaken.find(nextLoc) != bordersTaken.end() ? bordersTaken[nextLoc].second : nextSite.strength;
                            bs.affectedStrength += nextSiteStrength;
                            bs.affectedProduction += nextSite.production;
                            affectations.emplace(nextLoc, std::make_pair(nProp, bav.second));
                            // TODO: store move nextLoc -> curLoc
                            //logfile << "affectation! " << nextLoc << "->" << bav.second << ":" << nProp << std::endl;
                        } else {
                            hlt::Location otherBorder = affectations[nextLoc].second;
                            if (bav.first > enemyBordersValue[otherBorder]) {
                                // steal from less valuable border
                                const hlt::Site& nextSite = map.getSite(nextLoc);
                                unsigned int nextSiteStrength = bordersTaken.find(nextLoc) != bordersTaken.end() ? bordersTaken[nextLoc].second : nextSite.strength;
                                unsigned int nTurnsAffectedToOtherBorder = nProp - affectations[nextLoc].first + 1;
                                bs.affectedStrength += nextSiteStrength;
                                bs.affectedProduction += nextSite.production;
                                BorderStatus& otherbs = bordersStats[otherBorder];
                                otherbs.affectedStrength -= nextSiteStrength + (nTurnsAffectedToOtherBorder * nextSite.production);
                                otherbs.affectedProduction -= nextSite.production;
                                affectations[nextLoc] = std::make_pair(nProp, bav.second);
                                if (otherbs.affectedStrength <= otherbs.requiredStrength) {
                                    // remove from borders taken if strength is now insufficient
                                    bordersTaken.erase(otherBorder);
                                }
                                // TODO: store move nextLoc -> curLoc
                                //logfile << "RE-affectation! " << nextLoc << "->" << bav.second << ":" << nProp << std::endl;
                            }
                        }
                        bs.seenLocs.insert(nextLoc);
                        if (bs.affectedStrength > bs.requiredStrength) break;
                    }
                    if (bs.affectedStrength > bs.requiredStrength) { // first check strength during current turn
                        bordersTaken.emplace(bav.second, std::make_pair(nProp, bs.affectedStrength - bs.requiredStrength));
                    }
                    bs.affectedStrength += bs.affectedProduction; // sum current turn production to next turn's strength
                    if (bs.affectedStrength > bs.requiredStrength) { // then check for strength at the beginning of next turn
                        bordersTaken.emplace(bav.second, std::make_pair(nProp+1, bs.affectedStrength - bs.requiredStrength));
                    }

                    // advance border index to start with in next loop iteration if current border has enough strength or if there no more locs to visit
                    if (borderIndexStart == bavi && (bs.nextLocsToVisit.empty() || bs.affectedStrength > bs.requiredStrength)) ++borderIndexStart;

                    // TODO take new borders around taken border into account for next round of affectations
                    /*if (bs.affectedStrength > bs.requiredStrength)  {
                        for (hlt::Location border : curBordersToAttack) {
                            for (auto d : CARDINALS) {
                                const hlt::Location newLoc = map.getLocation(border, d);
                                const hlt::Site& newSite = map.getSite(newLoc);
                                if (newSite.owner == myID || enemyBordersValue.find(newLoc) != enemyBordersValue.end()) continue;
                                float value = newSite.strength ? 1.f * newSite.production / newSite.strength : 20.f * newSite.production;
                                if (newSite.owner != 0) value *= 20.f;

                                if (value > sortedBordersByValue[borderIndexStart].first) {
                                    enemyBordersValue[newLoc] = value; // TODO move up
                                    unsigned int strengthToAdd = newSite.strength ? newSite.strength : 1024;
                                    if (newSite.owner != 0) strengthToAdd = 255;
                                    bs.requiredStrength += strengthToAdd;
                                    nextBordersToAttack.push_back(newLoc);
                                } else {
                                    // TODO insert with sort in sortedBordersByValue after borderIndexStart
                                }
                            }
                        }
                    }
                    std::swap(curBordersToAttack, nextBordersToAttack);
                    nextBordersToAttack.clear();*/
                }
                ++nProp;
            }

            /*logfile << "affectations = ";
            for (const auto& a : affectations) {
                logfile << a.first << "->" << a.second.first << ":" << a.second.second << " ; ";
            }
            logfile << std::endl;*/
            /*logfile << "bordersTaken = ";
            for (const auto& bt : bordersTaken) {
                logfile << bt.first << "->" << bt.second.first << ":" << bt.second.second << " ; ";
            }
            logfile << std::endl;*/
        }



        //////////////////
        // CREATE MOVES //
        //////////////////
        for (unsigned short y = 0; y < map.height; y++) {
            for (unsigned short x = 0; x < map.width; x++) {
                const hlt::Location loc {x, y};
                const hlt::Site& site = map.getSite(loc);

                if (map.getSite(loc).owner == myID) {

                    unsigned int myStrength = site.strength;
                    // never move sites with 0 strength
                    if (myStrength == 0) {
                        moves.emplace(loc, STILL);
                        postMovesStrength[loc] += site.production;
                    }

                    // handle affected sites first
                    auto itAffect = affectations.find(loc);
                    if (itAffect != affectations.end()) {
                        hlt::Location borderLoc = itAffect->second.second;
                        auto itBorderTaken = bordersTaken.find(borderLoc);

                        bool doMove = false;
                        if (itBorderTaken != bordersTaken.end()) {
                            unsigned int turnAffectation = itAffect->second.first;
                            unsigned int turnBorderTaken = itBorderTaken->second.first;
                            if (turnAffectation < turnBorderTaken) {
                                moves.emplace(loc, STILL);
                                postMovesStrength[loc] += site.production;
                                continue;
                            }
                            if (turnAffectation == 0) {
                                moves.emplace(loc, map.getDirection(loc, borderLoc));
                                postMovesStrength[borderLoc] += myStrength;
                            }
                            // prefer wasting less production over expansion speed when the piece has to move more than a short distance
                            if (turnAffectation < 3) {
                                doMove = true;
                            }
                        }

                        if (!doMove && myStrength < site.production * 5) {
                            moves.emplace(loc, STILL);
                            postMovesStrength[loc] += site.production;
                            continue;
                        }

                        std::pair<int,int> directions = map.getDirectionsInMyTerritory(loc, borderLoc, myID);
                        unsigned char d = directions.first;
                        hlt::Location nextLoc = map.getLocation(loc, d);
                        hlt::Site nextSite = map.getSite(nextLoc);
                        // avoid loosing strength
                        if (myStrength + postMovesStrength[nextLoc] > 255) {
                            unsigned char d2 = directions.second;
                            if (d2 != STILL) {
                                nextLoc = map.getLocation(loc, d2);
                                nextSite = map.getSite(nextLoc);
                                if (myStrength + postMovesStrength[nextLoc] > 255 && (myStrength + site.production <= 255 /*|| site.production < postMovesStrength[nextLoc] - nextSite.strength*/)) {
                                    moves.emplace(loc, STILL);
                                    postMovesStrength[loc] += site.production;
                                } else {
                                    moves.emplace(loc, d2);
                                    postMovesStrength[nextLoc] += myStrength;
                                }
                            } else {
                                if (0) {//myStrength + site.production <= 255 /*|| site.production < postMovesStrength[nextLoc] - nextSite.strength*/) {
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
                        continue;
                    }


                    // handle borders
                    auto itBorder = std::find(borders.begin(), borders.end(), loc);
                    if (itBorder != borders.end())
                    {
                        // should try to attack or reinforce other borders?
                        std::size_t id = std::distance(borders.begin(), itBorder);
                        bool attackEnemy = borderWithEnemy[id];
                        int attackDirection = STILL;
                        
                        if (!attackEnemy) { // TODO factorize with reinforcement code
                            // find best border by attack value
                            hlt::Location bestBorderLoc;
                            float maxAttackValue = 0.f;
                            for (std::size_t i = 0; i < borders.size(); ++i) {
                                const hlt::Location l = borders[i];
                                unsigned int dist = map.getDistance(loc, l);
                                float attackValue = dist ? borderAttackValue[l] / dist : borderAttackValue[l] * 1.1f; // TODO: change dist to sum of prod on the way
                                if (attackValue > maxAttackValue) {
                                    bestBorderLoc = l;
                                    maxAttackValue = attackValue;
                                } 
                            }

                            if (maxAttackValue > 0.f) {
                                attackDirection = map.getDirectionInMyTerritory(loc, bestBorderLoc, myID);
                                if (attackDirection != STILL) {
                                    if (site.strength < site.production * 5) {
                                        moves.emplace(loc, STILL);
                                        postMovesStrength[loc] += site.production;
                                        continue;
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
                                    float attackValue = enemyBordersValue[attackLoc];
                                    //if (postMovesStrength[attackLoc] > 0) attackValue = 0; // prefer attacking elsewhere if site already under attack
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
                            const hlt::Site& attackSite = map.getSite(attackLoc);
                            if (myStrength + postMovesStrength[attackLoc] > 255 && (myStrength + site.production <= 255 || site.production < postMovesStrength[attackLoc] - attackSite.strength)) {
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

        ++turnNr;
    }

    return 0;
}
