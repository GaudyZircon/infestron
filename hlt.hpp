#ifndef HLT_H
#define HLT_H

#include <vector>
#include <utility>
#include <random>
#include <functional>

#define STILL 0
#define NORTH 1
#define EAST 2
#define SOUTH 3
#define WEST 4

const int DIRECTIONS[] = {STILL, NORTH, EAST, SOUTH, WEST};
const int CARDINALS[] = {NORTH, EAST, SOUTH, WEST};


namespace hlt{
    struct Location{
        unsigned short x, y;

        Location() : x(0), y(0) {}
        Location(unsigned short w, unsigned short h) : x(w), y(h) {}
    };

    std::ostream& operator<<(std::ostream& o, const Location& l) {
        o << l.x << " " << l.y;
        return o;
    } 

    bool operator==(const Location& l1, const Location& l2) {
        return (l1.x == l2.x) && (l1.y == l2.y);
    }

    bool operator<(const Location& l1, const Location& l2) {
        return ((l1.x + l1.y)*((unsigned int)l1.x + l1.y + 1) / 2) + l1.y < ((l2.x + l2.y)*((unsigned int)l2.x + l2.y + 1) / 2) + l2.y;
    }


    struct Site {
        unsigned char owner;
        unsigned char strength;
        unsigned char production;
    };

    std::ostream& operator<<(std::ostream& o, const Site& s) {
        o << int(s.owner) << " " << int(s.strength) << " " << int(s.production);
        return o;
    } 


    class GameMap {
        public:
            unsigned short width, height; //Number of rows & columns, NOT maximum index.
            std::vector< std::vector<Site> > contents;

            GameMap() :
                width(0),
                height(0),
                contents()
            {}
            GameMap(const GameMap &otherMap) :
                width(otherMap.width),
                height(otherMap.height),
                contents(otherMap.contents)
            {}
            GameMap(int w, int h) :
                width(w),
                height(h),
                contents(height, std::vector<Site>(width, { 0, 0, 0 }))
             {}

            bool inBounds(Location l) const {
                return l.x < width && l.y < height;
            }
            float getDistance(Location l1, Location l2) const {
                short dx = abs(l1.x - l2.x), dy = abs(l1.y - l2.y);
                if (dx > width / 2) dx = width - dx;
                if (dy > height / 2) dy = height - dy;
                return dx + dy;
            }
            float getAngle(Location l1, Location l2) const {
                short dx = l2.x - l1.x, dy = l2.y - l1.y;
                if(dx > width - dx) dx -= width;
                else if(-dx > width + dx) dx += width;
                if(dy > height - dy) dy -= height;
                else if(-dy > height + dy) dy += height;
                return atan2(dy, dx);
            }
            std::pair<short, short> getDiff(Location l1, Location l2) const {
                short dx = l2.x - l1.x, dy = l2.y - l1.y;
                if (dx > width / 2) dx -= width;
                else if (dx < -width / 2) dx += width;
                if (dy > height / 2) dy -= height;
                else if (dy < -height / 2) dy += height;

                return {dx,dy};
            }
            int getDirection(Location l1, Location l2) const {
                std::pair<short, short> diff = getDiff(l1, l2);

                if (std::abs(diff.first) > std::abs(diff.second)) {
                    if (diff.first > 0) return EAST;
                    return WEST;

                }
                if (diff.second > 0) return SOUTH;
                else if (diff.second < 0) return NORTH;
                return STILL;
            }
            std::pair<int,int> getDirections(Location l1, Location l2) const {
                std::pair<short, short> diff = getDiff(l1, l2);
                int xDir = STILL, yDir = STILL;

                if (diff.first > 0) {
                    xDir = EAST;
                } else if (diff.first < 0) {
                    xDir = WEST;
                }
                
                if (diff.second > 0) {
                    yDir = SOUTH;
                } else if (diff.second < 0) {
                    yDir = NORTH;
                }

                if (std::abs(diff.first) > std::abs(diff.second)) {
                    return std::make_pair(xDir, yDir);
                } else {
                    return std::make_pair(yDir, xDir);
                }
            }

            Location getLocation(Location l, unsigned char direction) const {
                if(direction != STILL) {
                    if(direction == NORTH) {
                        if(l.y == 0) l.y = height - 1;
                        else l.y--;
                    }
                    else if(direction == EAST) {
                        if(l.x == width - 1) l.x = 0;
                        else l.x++;
                    }
                    else if(direction == SOUTH) {
                        if(l.y == height - 1) l.y = 0;
                        else l.y++;
                    }
                    else if(direction == WEST) {
                        if(l.x == 0) l.x = width - 1;
                        else l.x--;
                    }
                }
                return l;
            }

            const Site& getSite(Location l) const {
                return contents[l.y][l.x];
            }
            const Site& getSite(Location l, unsigned char direction) const {
                l = getLocation(l, direction);
                return contents[l.y][l.x];
            }

            unsigned int computeMoveDamage(Location src, Location dst) const {
                const Site& srcSite = getSite(src);
                const Site& dstSite = getSite(dst);

                if (srcSite.owner == dstSite.owner) return 0;

                unsigned int damage = std::min(srcSite.strength, dstSite.strength);

                for (const auto d : CARDINALS) {
                    const Site& s = getSite(dst, d);
                    if (s.owner != 0 && srcSite.owner != s.owner) {
                        damage += std::min(srcSite.strength, s.strength);
                    }
                }
                return damage;
            }
    };

    struct Move {
        Location loc; unsigned char dir;

        Move(Location l, unsigned char d) : loc(l), dir(d) {}
        Move(unsigned short x, unsigned short y, unsigned char d) : Move({x,y},d) {}
    };

    std::ostream& operator<<(std::ostream& o, const Move& m) {
        o << m.loc << " " << (int)m.dir;
        return o;
    } 

    bool operator<(const Move& m1, const Move& m2) {
        unsigned int l1Prod = ((m1.loc.x + m1.loc.y)*((unsigned int)m1.loc.x + m1.loc.y + 1) / 2) + m1.loc.y, l2Prod = ((m2.loc.x + m2.loc.y)*((unsigned int)m2.loc.x + m2.loc.y + 1) / 2) + m2.loc.y;
        return ((l1Prod + m1.dir)*(l1Prod + m1.dir + 1) / 2) + m1.dir < ((l2Prod + m2.dir)*(l2Prod + m2.dir + 1) / 2) + m2.dir;
    }
}


namespace std {

    template <>
    struct hash<hlt::Location>
    {
        std::size_t operator()(const hlt::Location& l) const
        {
            using std::hash;

            return hash<unsigned short>()(l.x) ^ (hash<unsigned short>()(l.y) << 1);
        }
    };

}

#endif
